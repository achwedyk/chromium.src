// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_STORAGE_POLICY_VALUE_STORE_H_
#define CHROME_BROWSER_EXTENSIONS_API_STORAGE_POLICY_VALUE_STORE_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "extensions/browser/api/storage/settings_observer.h"
#include "extensions/browser/value_store/value_store.h"

namespace policy {
class PolicyMap;
}

namespace extensions {

// A ValueStore that is backed by another, persistent ValueStore, and stores
// the policies for a specific extension there. This ValueStore is used to
// run the function of the storage.managed namespace; it's read-only for the
// extension. The ManagedValueStoreCache sends updated policy to this store
// and manages its lifetime.
class PolicyValueStore : public ValueStore {
 public:
  PolicyValueStore(const std::string& extension_id,
                   const scoped_refptr<SettingsObserverList>& observers,
                   scoped_ptr<ValueStore> delegate);
  virtual ~PolicyValueStore();

  // Stores |policy| in the persistent database represented by the |delegate_|
  // and notifies observers with the changes from the previous policy.
  void SetCurrentPolicy(const policy::PolicyMap& policy);

  // Clears all the stored data and deletes the database.
  void DeleteStorage();

  // ValueStore implementation:
  virtual size_t GetBytesInUse(const std::string& key) override;
  virtual size_t GetBytesInUse(const std::vector<std::string>& keys) override;
  virtual size_t GetBytesInUse() override;
  virtual ReadResult Get(const std::string& key) override;
  virtual ReadResult Get(const std::vector<std::string>& keys) override;
  virtual ReadResult Get() override;
  virtual WriteResult Set(
      WriteOptions options,
      const std::string& key,
      const base::Value& value) override;
  virtual WriteResult Set(
      WriteOptions options, const base::DictionaryValue& values) override;
  virtual WriteResult Remove(const std::string& key) override;
  virtual WriteResult Remove(const std::vector<std::string>& keys) override;
  virtual WriteResult Clear() override;
  // Hopefully, as a Read-Only database, there is no reason to use these.
  virtual bool Restore() override;
  virtual bool RestoreKey(const std::string& key) override;

  // For unit tests.
  ValueStore* delegate() { return delegate_.get(); }

 private:
  std::string extension_id_;
  scoped_refptr<SettingsObserverList> observers_;
  scoped_ptr<ValueStore> delegate_;

  DISALLOW_COPY_AND_ASSIGN(PolicyValueStore);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_STORAGE_POLICY_VALUE_STORE_H_
