// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/component_cloud_policy_service.h"

#include <map>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/pickle.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "chrome/browser/policy/cloud/component_cloud_policy_store.h"
#include "chrome/browser/policy/cloud/component_cloud_policy_updater.h"
#include "chrome/browser/policy/cloud/external_policy_data_fetcher.h"
#include "chrome/browser/policy/cloud/resource_cache.h"
#include "chrome/browser/policy/proto/cloud/device_management_backend.pb.h"
#include "chrome/browser/policy/schema_map.h"
#include "components/policy/core/common/schema.h"
#include "net/url_request/url_request_context_getter.h"

namespace em = enterprise_management;

namespace policy {

const char ComponentCloudPolicyService::kComponentNamespaceCache[] =
    "component-namespace-cache";

namespace {

bool NotInSchemaMap(const scoped_refptr<SchemaMap> schema_map,
                    PolicyDomain domain,
                    const std::string& component_id) {
  return schema_map->GetSchema(PolicyNamespace(domain, component_id)) == NULL;
}

}  // namespace

ComponentCloudPolicyService::Delegate::~Delegate() {}

// Owns the objects that live on the background thread, and posts back to the
// thread that the ComponentCloudPolicyService runs on whenever the policy
// changes.
class ComponentCloudPolicyService::Backend
    : public ComponentCloudPolicyStore::Delegate {
 public:
  // This class can be instantiated on any thread but from then on, may be
  // accessed via the |task_runner_| only. Policy changes are posted to the
  // |service| via the |service_task_runner|. The |cache| is used to load and
  // store local copies of the downloaded policies.
  Backend(base::WeakPtr<ComponentCloudPolicyService> service,
          scoped_refptr<base::SequencedTaskRunner> task_runner,
          scoped_refptr<base::SequencedTaskRunner> service_task_runner,
          scoped_ptr<ResourceCache> cache);
  virtual ~Backend();

  // This is invoked right after the constructor but on the background thread.
  // Used to create the store on the right thread.
  void Init();

  // Reads the initial list of components and the initial policy.
  void FinalizeInit();

  // Allows downloading of external data via the |external_policy_data_fetcher|.
  void Connect(
      scoped_ptr<ExternalPolicyDataFetcher> external_policy_data_fetcher);

  // Stops updating remote data. Cached policies are still served.
  void Disconnect();

  // Loads the initial policies from the store. |username| and |dm_token| are
  // used to validate the cached policies.
  void SetCredentials(const std::string& username, const std::string& dm_token);

  // Passes a policy protobuf to the backend, to start its validation and
  // eventual download of the policy data on the background thread.
  // This is ignored if the backend isn't connected.
  void UpdateExternalPolicy(scoped_ptr<em::PolicyFetchResponse> response);

  // ComponentCloudPolicyStore::Delegate implementation:
  virtual void OnComponentCloudPolicyStoreUpdated() OVERRIDE;

  // Passes the current SchemaMap so that the disk cache can purge components
  // that aren't being tracked anymore.
  void OnSchemasUpdated(scoped_refptr<SchemaMap> schema_map);

 private:
  scoped_ptr<PolicyNamespaceKeys> ReadCachedComponents();

  // The ComponentCloudPolicyService that owns |this|. Used to inform the
  // |service_| when policy changes.
  base::WeakPtr<ComponentCloudPolicyService> service_;

  // The thread that |this| runs on. Used to post tasks to be run by |this|.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The thread that the |service_| runs on. Used to post policy changes to the
  // right thread.
  scoped_refptr<base::SequencedTaskRunner> service_task_runner_;

  scoped_ptr<ResourceCache> cache_;
  scoped_ptr<ComponentCloudPolicyStore> store_;
  scoped_ptr<ComponentCloudPolicyUpdater> updater_;
  scoped_refptr<SchemaMap> schema_map_;

  DISALLOW_COPY_AND_ASSIGN(Backend);
};

ComponentCloudPolicyService::Backend::Backend(
    base::WeakPtr<ComponentCloudPolicyService> service,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_refptr<base::SequencedTaskRunner> service_task_runner,
    scoped_ptr<ResourceCache> cache)
    : service_(service),
      task_runner_(task_runner),
      service_task_runner_(service_task_runner),
      cache_(cache.Pass()) {}

ComponentCloudPolicyService::Backend::~Backend() {}

void ComponentCloudPolicyService::Backend::Init() {
  DCHECK(!store_);
  store_.reset(new ComponentCloudPolicyStore(this, cache_.get()));
}

void ComponentCloudPolicyService::Backend::FinalizeInit() {
  // Read the components that were cached in the last OnSchemasUpdated() call.
  scoped_ptr<PolicyNamespaceKeys> components = ReadCachedComponents();

  // Read the initial policy.
  store_->Load();
  scoped_ptr<PolicyBundle> policy(new PolicyBundle);
  policy->CopyFrom(store_->policy());

  service_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ComponentCloudPolicyService::OnBackendInitialized,
                 service_,
                 base::Passed(&components),
                 base::Passed(&policy)));
}

void ComponentCloudPolicyService::Backend::SetCredentials(
    const std::string& username,
    const std::string& dm_token) {
  store_->SetCredentials(username, dm_token);
}

void ComponentCloudPolicyService::Backend::Connect(
    scoped_ptr<ExternalPolicyDataFetcher> external_policy_data_fetcher) {
  updater_.reset(new ComponentCloudPolicyUpdater(
      task_runner_,
      external_policy_data_fetcher.Pass(),
      store_.get()));
}

void ComponentCloudPolicyService::Backend::Disconnect() {
  updater_.reset();
}

void ComponentCloudPolicyService::Backend::UpdateExternalPolicy(
    scoped_ptr<em::PolicyFetchResponse> response) {
  if (updater_)
    updater_->UpdateExternalPolicy(response.Pass());
}

void ComponentCloudPolicyService::Backend::
    OnComponentCloudPolicyStoreUpdated() {
  if (!schema_map_) {
    // No schemas have been registered yet, so keep serving the initial policy
    // obtained from FinalizeInit().
    return;
  }

  scoped_ptr<PolicyBundle> bundle(new PolicyBundle);
  bundle->CopyFrom(store_->policy());
  schema_map_->FilterBundle(bundle.get());
  service_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ComponentCloudPolicyService::OnPolicyUpdated,
                 service_,
                 base::Passed(&bundle)));
}

void ComponentCloudPolicyService::Backend::OnSchemasUpdated(
    scoped_refptr<SchemaMap> schema_map) {
  // Store the current list of components in the cache.
  const DomainMap& domains = schema_map->GetDomains();
  for (DomainMap::const_iterator domain = domains.begin();
       domain != domains.end(); ++domain) {
    std::string domain_policy_type;
    if (!ComponentCloudPolicyStore::GetPolicyType(domain->first,
                                                  &domain_policy_type)) {
      continue;
    }
    Pickle pickle;
    const ComponentMap& components = domain->second;
    for (ComponentMap::const_iterator comp = components.begin();
         comp != components.end(); ++comp) {
      pickle.WriteString(comp->first);
    }
    std::string data(reinterpret_cast<const char*>(pickle.data()),
                     pickle.size());
    cache_->Store(kComponentNamespaceCache, domain_policy_type, data);

    // Purge any components that have been removed.
    if (store_) {
      store_->Purge(domain->first,
                    base::Bind(&NotInSchemaMap, schema_map, domain->first));
    }
  }

  // Serve policies that may have updated while the backend was waiting for
  // the schema_map.
  const bool trigger_update = !schema_map_;
  schema_map_ = schema_map;
  if (trigger_update)
    OnComponentCloudPolicyStoreUpdated();
}

scoped_ptr<ComponentCloudPolicyService::PolicyNamespaceKeys>
    ComponentCloudPolicyService::Backend::ReadCachedComponents() {
  scoped_ptr<PolicyNamespaceKeys> keys(new PolicyNamespaceKeys);
  std::map<std::string, std::string> contents;
  cache_->LoadAllSubkeys(kComponentNamespaceCache, &contents);
  for (std::map<std::string, std::string>::iterator it = contents.begin();
       it != contents.end(); ++it) {
    PolicyDomain domain;
    if (ComponentCloudPolicyStore::GetPolicyDomain(it->first, &domain)) {
      const Pickle pickle(it->second.data(), it->second.size());
      PickleIterator pickit(pickle);
      std::string id;
      while (pickit.ReadString(&id))
        keys->insert(std::make_pair(it->first, id));
    } else {
      cache_->Delete(kComponentNamespaceCache, it->first);
    }
  }
  return keys.Pass();
}

ComponentCloudPolicyService::ComponentCloudPolicyService(
    Delegate* delegate,
    CloudPolicyStore* store,
    scoped_ptr<ResourceCache> cache,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner)
    : delegate_(delegate),
      backend_task_runner_(backend_task_runner),
      io_task_runner_(io_task_runner),
      client_(NULL),
      store_(store),
      is_initialized_(false),
      has_initial_keys_(false),
      weak_ptr_factory_(this) {
  store_->AddObserver(this);

  backend_.reset(new Backend(weak_ptr_factory_.GetWeakPtr(),
                             backend_task_runner_,
                             base::MessageLoopProxy::current(),
                             cache.Pass()));
  backend_task_runner_->PostTask(
      FROM_HERE, base::Bind(&Backend::Init, base::Unretained(backend_.get())));

  if (store_->is_initialized())
    InitializeBackend();
}

ComponentCloudPolicyService::~ComponentCloudPolicyService() {
  DCHECK(CalledOnValidThread());
  store_->RemoveObserver(this);
  if (client_)
    client_->RemoveObserver(this);
  io_task_runner_->DeleteSoon(FROM_HERE,
                              external_policy_data_fetcher_backend_.release());
  backend_task_runner_->DeleteSoon(FROM_HERE, backend_.release());
}

// static
bool ComponentCloudPolicyService::SupportsDomain(PolicyDomain domain) {
  return ComponentCloudPolicyStore::SupportsDomain(domain);
}

void ComponentCloudPolicyService::Connect(
    CloudPolicyClient* client,
    scoped_refptr<net::URLRequestContextGetter> request_context) {
  DCHECK(CalledOnValidThread());
  DCHECK(!client_);
  client_ = client;
  client_->AddObserver(this);
  DCHECK(!external_policy_data_fetcher_backend_);
  external_policy_data_fetcher_backend_.reset(
      new ExternalPolicyDataFetcherBackend(io_task_runner_,
                                           request_context));
  // Create the updater in the backend.
  backend_task_runner_->PostTask(FROM_HERE, base::Bind(
      &Backend::Connect,
      base::Unretained(backend_.get()),
      base::Passed(external_policy_data_fetcher_backend_->CreateFrontend(
          backend_task_runner_))));
  if (is_initialized())
    InitializeClient();
}

void ComponentCloudPolicyService::Disconnect() {
  DCHECK(CalledOnValidThread());
  if (client_) {
    // Unregister all the current components.
    RemoveNamespacesToFetch(keys_);

    client_->RemoveObserver(this);
    client_ = NULL;

    io_task_runner_->DeleteSoon(
        FROM_HERE, external_policy_data_fetcher_backend_.release());
    backend_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&Backend::Disconnect, base::Unretained(backend_.get())));
  }
}

void ComponentCloudPolicyService::OnSchemasUpdated(
    const scoped_refptr<SchemaMap>& schema_map) {
  DCHECK(CalledOnValidThread());

  // Send the new schemas to the backend, to purge the cache.
  backend_task_runner_->PostTask(FROM_HERE,
                                 base::Bind(&Backend::OnSchemasUpdated,
                                            base::Unretained(backend_.get()),
                                            schema_map));

  // Register the current list of components for supported domains at the
  // |client_|.
  PolicyNamespaceKeys new_keys;
  const DomainMap& domains = schema_map->GetDomains();
  for (DomainMap::const_iterator domain = domains.begin();
       domain != domains.end(); ++domain) {
    std::string domain_policy_type;
    if (!ComponentCloudPolicyStore::GetPolicyType(domain->first,
                                                  &domain_policy_type)) {
      continue;
    }
    const ComponentMap& components = domain->second;
    for (ComponentMap::const_iterator comp = components.begin();
         comp != components.end(); ++comp) {
      new_keys.insert(std::make_pair(domain_policy_type, comp->first));
    }
  }

  if (client_ && is_initialized() && UpdateClientNamespaces(keys_, new_keys))
      delegate_->OnComponentCloudPolicyRefreshNeeded();

  keys_.swap(new_keys);
  has_initial_keys_ = true;
}

void ComponentCloudPolicyService::OnPolicyFetched(CloudPolicyClient* client) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(client_, client);
  // Pass each PolicyFetchResponse whose policy type is registered to the
  // Backend.
  const CloudPolicyClient::ResponseMap& responses = client_->responses();
  for (CloudPolicyClient::ResponseMap::const_iterator it = responses.begin();
       it != responses.end(); ++it) {
    const PolicyNamespaceKey& key(it->first);
    if (ContainsKey(keys_, key)) {
      scoped_ptr<em::PolicyFetchResponse> response(
          new em::PolicyFetchResponse(*it->second));
      backend_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&Backend::UpdateExternalPolicy,
                     base::Unretained(backend_.get()),
                     base::Passed(&response)));
    }
  }
}

void ComponentCloudPolicyService::OnRegistrationStateChanged(
    CloudPolicyClient* client) {
  DCHECK(CalledOnValidThread());
  // Ignored.
}

void ComponentCloudPolicyService::OnClientError(CloudPolicyClient* client) {
  DCHECK(CalledOnValidThread());
  // Ignored.
}

void ComponentCloudPolicyService::OnStoreLoaded(CloudPolicyStore* store) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(store_, store);
  if (store_->is_initialized()) {
    if (is_initialized()) {
      // The backend is already initialized; update the credentials, in case
      // a new dmtoken or server key became available.
      SetCredentialsAndReloadClient();
    } else {
      // The |store_| just became ready; initialize the backend now.
      InitializeBackend();
    }
  }
}

void ComponentCloudPolicyService::OnStoreError(CloudPolicyStore* store) {
  DCHECK(CalledOnValidThread());
  OnStoreLoaded(store);
}

void ComponentCloudPolicyService::InitializeBackend() {
  DCHECK(CalledOnValidThread());
  DCHECK(!is_initialized());
  DCHECK(store_->is_initialized());

  // Set the credentials for the initial policy load, if available.
  SetCredentialsAndReloadClient();

  backend_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&Backend::FinalizeInit, base::Unretained(backend_.get())));
}

void ComponentCloudPolicyService::OnBackendInitialized(
    scoped_ptr<PolicyNamespaceKeys> initial_keys,
    scoped_ptr<PolicyBundle> initial_policy) {
  DCHECK(CalledOnValidThread());
  // InitializeBackend() may be called multiple times if the |store_| fires
  // events while the backend is loading.
  if (is_initialized())
    return;

  // OnSchemasUpdated() may have been called while the backend was initializing;
  // in that case ignore the cached keys.
  if (!has_initial_keys_)
    keys_.swap(*initial_keys);

  // A client may have already connected while the backend was initializing.
  if (client_)
    InitializeClient();

  // Set the initial policy, and send the initial update callback.
  is_initialized_ = true;
  OnPolicyUpdated(initial_policy.Pass());
}

void ComponentCloudPolicyService::InitializeClient() {
  DCHECK(CalledOnValidThread());
  // Register all the current components.
  AddNamespacesToFetch(keys_);
  // The client may already have PolicyFetchResponses for registered components;
  // load them now.
  OnPolicyFetched(client_);
  if (!keys_.empty() && is_initialized())
    delegate_->OnComponentCloudPolicyRefreshNeeded();
}

void ComponentCloudPolicyService::OnPolicyUpdated(
    scoped_ptr<PolicyBundle> policy) {
  DCHECK(CalledOnValidThread());
  policy_.Swap(policy.get());
  // Don't propagate updates until the initial store Load() has been done.
  if (is_initialized())
    delegate_->OnComponentCloudPolicyUpdated();
}

void ComponentCloudPolicyService::SetCredentialsAndReloadClient() {
  DCHECK(CalledOnValidThread());
  const em::PolicyData* policy = store_->policy();
  if (!policy || !policy->has_username() || !policy->has_request_token())
    return;
  backend_task_runner_->PostTask(FROM_HERE,
                                 base::Bind(&Backend::SetCredentials,
                                            base::Unretained(backend_.get()),
                                            policy->username(),
                                            policy->request_token()));
  // If this was the initial register, or if the signing key changed, then the
  // previous OnPolicyFetched() call had its PolicyFetchResponses rejected
  // because the credentials weren't updated yet. Reload all the responses in
  // the client now to handle those cases; if those responses have already been
  // validated then they will be ignored.
  if (client_)
    OnPolicyFetched(client_);
}

bool ComponentCloudPolicyService::UpdateClientNamespaces(
    const PolicyNamespaceKeys& old_keys,
    const PolicyNamespaceKeys& new_keys) {
  DCHECK(CalledOnValidThread());
  PolicyNamespaceKeys added =
      base::STLSetDifference<PolicyNamespaceKeys>(new_keys, old_keys);
  PolicyNamespaceKeys removed =
      base::STLSetDifference<PolicyNamespaceKeys>(old_keys, new_keys);
  AddNamespacesToFetch(added);
  RemoveNamespacesToFetch(removed);
  return !added.empty();
}

void ComponentCloudPolicyService::AddNamespacesToFetch(
    const PolicyNamespaceKeys& keys) {
  DCHECK(CalledOnValidThread());
  for (PolicyNamespaceKeys::const_iterator it = keys.begin();
       it != keys.end(); ++it) {
    client_->AddNamespaceToFetch(*it);
  }
}

void ComponentCloudPolicyService::RemoveNamespacesToFetch(
    const PolicyNamespaceKeys& keys) {
  DCHECK(CalledOnValidThread());
  for (PolicyNamespaceKeys::const_iterator it = keys.begin();
       it != keys.end(); ++it) {
    client_->RemoveNamespaceToFetch(*it);
  }
}

}  // namespace policy
