// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_EASY_UNLOCK_PRIVATE_EASY_UNLOCK_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_EASY_UNLOCK_PRIVATE_EASY_UNLOCK_PRIVATE_API_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "extensions/browser/api/bluetooth_socket/bluetooth_socket_api.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"

// Implementations for chrome.easyUnlockPrivate API functions.

namespace content {
class BrowserContext;
}

namespace extensions {
namespace api {

namespace easy_unlock {
struct SeekDeviceResult;
}  // easy_unlock

class EasyUnlockPrivateCryptoDelegate;

class EasyUnlockPrivateAPI : public BrowserContextKeyedAPI {
 public:
  static BrowserContextKeyedAPIFactory<EasyUnlockPrivateAPI>*
      GetFactoryInstance();

  static const bool kServiceRedirectedInIncognito = true;

  explicit EasyUnlockPrivateAPI(content::BrowserContext* context);
  virtual ~EasyUnlockPrivateAPI();

  EasyUnlockPrivateCryptoDelegate* crypto_delegate() {
    return crypto_delegate_.get();
  }

 private:
  friend class BrowserContextKeyedAPIFactory<EasyUnlockPrivateAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "EasyUnlockPrivate"; }

  scoped_ptr<EasyUnlockPrivateCryptoDelegate> crypto_delegate_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateAPI);
};

// TODO(tbarzic): Replace SyncExtensionFunction/AsyncExtensionFunction overrides
// with UIThreadExtensionFunction throughout the file.
class EasyUnlockPrivateGetStringsFunction : public SyncExtensionFunction {
 public:
  EasyUnlockPrivateGetStringsFunction();

 protected:
  virtual ~EasyUnlockPrivateGetStringsFunction();

  // SyncExtensionFunction:
  virtual bool RunSync() override;

 private:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.getStrings",
                             EASYUNLOCKPRIVATE_GETSTRINGS)

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateGetStringsFunction);
};

class EasyUnlockPrivatePerformECDHKeyAgreementFunction
    : public AsyncExtensionFunction {
 public:
  EasyUnlockPrivatePerformECDHKeyAgreementFunction();

 protected:
  virtual ~EasyUnlockPrivatePerformECDHKeyAgreementFunction();

  virtual bool RunAsync() override;

 private:
  void OnData(const std::string& secret_key);

  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.performECDHKeyAgreement",
                             EASYUNLOCKPRIVATE_PERFORMECDHKEYAGREEMENT)

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivatePerformECDHKeyAgreementFunction);
};

class EasyUnlockPrivateGenerateEcP256KeyPairFunction
    : public AsyncExtensionFunction {
 public:
  EasyUnlockPrivateGenerateEcP256KeyPairFunction();

 protected:
  virtual ~EasyUnlockPrivateGenerateEcP256KeyPairFunction();

  virtual bool RunAsync() override;

 private:
  void OnData(const std::string& public_key,
              const std::string& private_key);

  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.generateEcP256KeyPair",
                             EASYUNLOCKPRIVATE_GENERATEECP256KEYPAIR)

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateGenerateEcP256KeyPairFunction);
};

class EasyUnlockPrivateCreateSecureMessageFunction
    : public AsyncExtensionFunction {
 public:
  EasyUnlockPrivateCreateSecureMessageFunction();

 protected:
  virtual ~EasyUnlockPrivateCreateSecureMessageFunction();

  virtual bool RunAsync() override;

 private:
  void OnData(const std::string& message);

  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.createSecureMessage",
                             EASYUNLOCKPRIVATE_CREATESECUREMESSAGE)

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateCreateSecureMessageFunction);
};

class EasyUnlockPrivateUnwrapSecureMessageFunction
    : public AsyncExtensionFunction {
 public:
  EasyUnlockPrivateUnwrapSecureMessageFunction();

 protected:
  virtual ~EasyUnlockPrivateUnwrapSecureMessageFunction();

  virtual bool RunAsync() override;

 private:
  void OnData(const std::string& data);

  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.unwrapSecureMessage",
                             EASYUNLOCKPRIVATE_UNWRAPSECUREMESSAGE)

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateUnwrapSecureMessageFunction);
};

class EasyUnlockPrivateSeekBluetoothDeviceByAddressFunction
    : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.seekBluetoothDeviceByAddress",
                             EASYUNLOCKPRIVATE_SEEKBLUETOOTHDEVICEBYADDRESS)
  EasyUnlockPrivateSeekBluetoothDeviceByAddressFunction();

 private:
  virtual ~EasyUnlockPrivateSeekBluetoothDeviceByAddressFunction();

  // AsyncExtensionFunction:
  virtual bool RunAsync() override;

  // Callbacks that are called when the seek operation succeeds or fails.
  void OnSeekSuccess();
  void OnSeekFailure(const std::string& error_message);

  DISALLOW_COPY_AND_ASSIGN(
      EasyUnlockPrivateSeekBluetoothDeviceByAddressFunction);
};

class EasyUnlockPrivateConnectToBluetoothServiceInsecurelyFunction
    : public core_api::BluetoothSocketAbstractConnectFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "easyUnlockPrivate.connectToBluetoothServiceInsecurely",
      EASYUNLOCKPRIVATE_CONNECTTOBLUETOOTHSERVICEINSECURELY)
  EasyUnlockPrivateConnectToBluetoothServiceInsecurelyFunction();

 private:
  virtual ~EasyUnlockPrivateConnectToBluetoothServiceInsecurelyFunction();

  // BluetoothSocketAbstractConnectFunction:
  virtual void ConnectToService(device::BluetoothDevice* device,
                                const device::BluetoothUUID& uuid) override;

  DISALLOW_COPY_AND_ASSIGN(
      EasyUnlockPrivateConnectToBluetoothServiceInsecurelyFunction);
};

class EasyUnlockPrivateUpdateScreenlockStateFunction
    : public SyncExtensionFunction {
 public:
  EasyUnlockPrivateUpdateScreenlockStateFunction();

 protected:
  virtual ~EasyUnlockPrivateUpdateScreenlockStateFunction();

  virtual bool RunSync() override;

 private:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.updateScreenlockState",
                             EASYUNLOCKPRIVATE_UPDATESCREENLOCKSTATE)

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateUpdateScreenlockStateFunction);
};

class EasyUnlockPrivateSetPermitAccessFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.setPermitAccess",
                             EASYUNLOCKPRIVATE_SETPERMITACCESS)
  EasyUnlockPrivateSetPermitAccessFunction();

 private:
  virtual ~EasyUnlockPrivateSetPermitAccessFunction();

  // SyncExtensionFunction:
  virtual bool RunSync() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateSetPermitAccessFunction);
};

class EasyUnlockPrivateGetPermitAccessFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.getPermitAccess",
                             EASYUNLOCKPRIVATE_GETPERMITACCESS)
  EasyUnlockPrivateGetPermitAccessFunction();

 private:
  virtual ~EasyUnlockPrivateGetPermitAccessFunction();

  // SyncExtensionFunction:
  virtual bool RunSync() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateGetPermitAccessFunction);
};

class EasyUnlockPrivateClearPermitAccessFunction
    : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.clearPermitAccess",
                             EASYUNLOCKPRIVATE_CLEARPERMITACCESS)
  EasyUnlockPrivateClearPermitAccessFunction();

 private:
  virtual ~EasyUnlockPrivateClearPermitAccessFunction();

  // SyncExtensionFunction:
  virtual bool RunSync() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateClearPermitAccessFunction);
};

class EasyUnlockPrivateSetRemoteDevicesFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.setRemoteDevices",
                             EASYUNLOCKPRIVATE_SETREMOTEDEVICES)
  EasyUnlockPrivateSetRemoteDevicesFunction();

 private:
  virtual ~EasyUnlockPrivateSetRemoteDevicesFunction();

  // SyncExtensionFunction:
  virtual bool RunSync() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateSetRemoteDevicesFunction);
};

class EasyUnlockPrivateGetRemoteDevicesFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.getRemoteDevices",
                             EASYUNLOCKPRIVATE_GETREMOTEDEVICES)
  EasyUnlockPrivateGetRemoteDevicesFunction();

 private:
  virtual ~EasyUnlockPrivateGetRemoteDevicesFunction();

  // SyncExtensionFunction:
  virtual bool RunSync() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateGetRemoteDevicesFunction);
};

class EasyUnlockPrivateGetSignInChallengeFunction :
    public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.getSignInChallenge",
                             EASYUNLOCKPRIVATE_GETSIGNINCHALLENGE)
  EasyUnlockPrivateGetSignInChallengeFunction();

 private:
  virtual ~EasyUnlockPrivateGetSignInChallengeFunction();

  // SyncExtensionFunction:
  virtual bool RunSync() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateGetSignInChallengeFunction);
};

class EasyUnlockPrivateTrySignInSecretFunction :
    public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.trySignInSecret",
                             EASYUNLOCKPRIVATE_TRYSIGNINSECRET)
  EasyUnlockPrivateTrySignInSecretFunction();

 private:
  virtual ~EasyUnlockPrivateTrySignInSecretFunction();

  // SyncExtensionFunction:
  virtual bool RunSync() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateTrySignInSecretFunction);
};

class EasyUnlockPrivateGetUserInfoFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.getUserInfo",
                             EASYUNLOCKPRIVATE_GETUSERINFO)
  EasyUnlockPrivateGetUserInfoFunction();

 private:
  virtual ~EasyUnlockPrivateGetUserInfoFunction();

  // SyncExtensionFunction:
  virtual bool RunSync() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateGetUserInfoFunction);
};

}  // namespace api
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_EASY_UNLOCK_PRIVATE_EASY_UNLOCK_PRIVATE_API_H_
