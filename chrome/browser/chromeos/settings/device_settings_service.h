// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_SETTINGS_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_SETTINGS_SERVICE_H_

#include <deque>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/stl_util.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/tpm_token_loader.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "policy/proto/device_management_backend.pb.h"

namespace crypto {
class RSAPrivateKey;
}

namespace enterprise_management {
class ChromeDeviceSettingsProto;
}

namespace chromeos {

class OwnerKeyUtil;
class SessionManagerOperation;

// Keeps the public and private halves of the owner key. Both may be missing,
// but if the private key is present, the public half will be as well. This
// class is immutable and refcounted in order to allow safe access from any
// thread.
class OwnerKey : public base::RefCountedThreadSafe<OwnerKey> {
 public:
  OwnerKey(scoped_ptr<std::vector<uint8> > public_key,
           scoped_ptr<crypto::RSAPrivateKey> private_key);

  const std::vector<uint8>* public_key() {
    return public_key_.get();
  }

  std::string public_key_as_string() {
    return std::string(reinterpret_cast<const char*>(
        vector_as_array(public_key_.get())), public_key_->size());
  }

  crypto::RSAPrivateKey* private_key() {
    return private_key_.get();
  }

 private:
  friend class base::RefCountedThreadSafe<OwnerKey>;
  ~OwnerKey();

  scoped_ptr<std::vector<uint8> > public_key_;
  scoped_ptr<crypto::RSAPrivateKey> private_key_;

  DISALLOW_COPY_AND_ASSIGN(OwnerKey);
};

// Deals with the low-level interface to Chromium OS device settings. Device
// settings are stored in a protobuf that's protected by a cryptographic
// signature generated by a key in the device owner's possession. Key and
// settings are brokered by the session_manager daemon.
//
// The purpose of DeviceSettingsService is to keep track of the current key and
// settings blob. For reading and writing device settings, use CrosSettings
// instead, which provides a high-level interface that allows for manipulation
// of individual settings.
//
// DeviceSettingsService generates notifications for key and policy update
// events so interested parties can reload state as appropriate.
class DeviceSettingsService : public SessionManagerClient::Observer,
                              public TPMTokenLoader::Observer {
 public:
  // Indicates ownership status of the device.
  enum OwnershipStatus {
    // Listed in upgrade order.
    OWNERSHIP_UNKNOWN = 0,
    OWNERSHIP_NONE,
    OWNERSHIP_TAKEN
  };

  typedef base::Callback<void(OwnershipStatus)> OwnershipStatusCallback;
  typedef base::Callback<void(bool)> IsCurrentUserOwnerCallback;

  // Status codes for Store().
  enum Status {
    STORE_SUCCESS,
    STORE_KEY_UNAVAILABLE,       // Owner key not yet configured.
    STORE_POLICY_ERROR,          // Failure constructing the settings blob.
    STORE_OPERATION_FAILED,      // IPC to session_manager daemon failed.
    STORE_NO_POLICY,             // No settings blob present.
    STORE_INVALID_POLICY,        // Invalid settings blob.
    STORE_VALIDATION_ERROR,      // Unrecoverable policy validation failure.
    STORE_TEMP_VALIDATION_ERROR, // Temporary policy validation failure.
  };

  // Observer interface.
  class Observer {
   public:
    virtual ~Observer();

    // Indicates device ownership status changes.
    virtual void OwnershipStatusChanged() = 0;

    // Gets call after updates to the device settings.
    virtual void DeviceSettingsUpdated() = 0;
  };

  // Manage singleton instance.
  static void Initialize();
  static bool IsInitialized();
  static void Shutdown();
  static DeviceSettingsService* Get();

  // Creates a device settings service instance. This is meant for unit tests,
  // production code uses the singleton returned by Get() above.
  DeviceSettingsService();
  virtual ~DeviceSettingsService();

  // To be called on startup once threads are initialized and DBus is ready.
  void SetSessionManager(SessionManagerClient* session_manager_client,
                         scoped_refptr<OwnerKeyUtil> owner_key_util);

  // Prevents the service from making further calls to session_manager_client
  // and stops any pending operations.
  void UnsetSessionManager();

  // Returns the currently active device settings. Returns NULL if the device
  // settings have not been retrieved from session_manager yet.
  const enterprise_management::PolicyData* policy_data() {
    return policy_data_.get();
  }
  const enterprise_management::ChromeDeviceSettingsProto*
      device_settings() const {
    return device_settings_.get();
  }

  // Returns the currently used owner key.
  scoped_refptr<OwnerKey> GetOwnerKey();

  // Returns the status generated by the last operation.
  Status status() {
    return store_status_;
  }

  // Triggers an attempt to pull the public half of the owner key from disk and
  // load the device settings.
  void Load();

  // Signs |settings| with the private half of the owner key and sends the
  // resulting policy blob to session manager for storage. The result of the
  // operation is reported through |callback|. If successful, the updated device
  // settings are present in policy_data() and device_settings() when the
  // callback runs.
  void SignAndStore(
      scoped_ptr<enterprise_management::ChromeDeviceSettingsProto> new_settings,
      const base::Closure& callback);

  // Sets the management related settings in PolicyData. Note that if
  // |management_mode| is NOT_MANAGED, |request_token| and |device_id| should be
  // empty strings.
  void SetManagementSettings(
      enterprise_management::PolicyData::ManagementMode management_mode,
      const std::string& request_token,
      const std::string& device_id,
      const base::Closure& callback);

  // Stores a policy blob to session_manager. The result of the operation is
  // reported through |callback|. If successful, the updated device settings are
  // present in policy_data() and device_settings() when the callback runs.
  void Store(scoped_ptr<enterprise_management::PolicyFetchResponse> policy,
             const base::Closure& callback);

  // Returns the ownership status. May return OWNERSHIP_UNKNOWN if the disk
  // hasn't been checked yet.
  OwnershipStatus GetOwnershipStatus();

  // Determines the ownership status and reports the result to |callback|. This
  // is guaranteed to never return OWNERSHIP_UNKNOWN.
  void GetOwnershipStatusAsync(const OwnershipStatusCallback& callback);

  // Checks whether we have the private owner key.
  bool HasPrivateOwnerKey();

  // Determines whether the current user is the owner. The callback is
  // guaranteed not to be called before it is possible to determine if the
  // current user is the owner (by testing existence of the private owner key).
  void IsCurrentUserOwnerAsync(const IsCurrentUserOwnerCallback& callback);

  // Sets the identity of the user that's interacting with the service. This is
  // relevant only for writing settings through SignAndStore().
  void SetUsername(const std::string& username);
  const std::string& GetUsername() const;

  // Adds an observer.
  void AddObserver(Observer* observer);
  // Removes an observer.
  void RemoveObserver(Observer* observer);

  // SessionManagerClient::Observer:
  virtual void OwnerKeySet(bool success) OVERRIDE;
  virtual void PropertyChangeComplete(bool success) OVERRIDE;

  // TPMTokenLoader::Observer:
  virtual void OnTPMTokenReady() OVERRIDE;

 private:
  // Enqueues a new operation. Takes ownership of |operation| and starts it
  // right away if there is no active operation currently.
  void Enqueue(SessionManagerOperation* operation);

  // Enqueues a load operation.
  void EnqueueLoad(bool force_key_load);

  // Makes sure there's a reload operation so changes to the settings (and key,
  // in case force_key_load is set) are getting picked up.
  void EnsureReload(bool force_key_load);

  // Runs the next pending operation.
  void StartNextOperation();

  // Updates status, policy data and owner key from a finished operation.
  // Starts the next pending operation if available.
  void HandleCompletedOperation(const base::Closure& callback,
                                SessionManagerOperation* operation,
                                Status status);

  // Updates status and invokes the callback immediately.
  void HandleError(Status status, const base::Closure& callback);

  // Assembles PolicyData based on |settings| and the current |policy_data_|
  // and |username_|.
  scoped_ptr<enterprise_management::PolicyData> AssemblePolicy(
      const enterprise_management::ChromeDeviceSettingsProto& settings) const;

  // Returns the current management mode.
  enterprise_management::PolicyData::ManagementMode GetManagementMode() const;

  // Returns true if it is okay to transfer from the current mode to the new
  // mode. This function should be called in SetManagementMode().
  bool CheckManagementModeTransition(
      enterprise_management::PolicyData::ManagementMode new_mode) const;

  SessionManagerClient* session_manager_client_;
  scoped_refptr<OwnerKeyUtil> owner_key_util_;

  Status store_status_;

  std::vector<OwnershipStatusCallback> pending_ownership_status_callbacks_;
  std::vector<IsCurrentUserOwnerCallback>
      pending_is_current_user_owner_callbacks_;

  std::string username_;
  scoped_refptr<OwnerKey> owner_key_;
  // Whether TPM token still needs to be initialized.
  bool waiting_for_tpm_token_;
  // Whether TPM token was ready when the current owner key was set.
  // Implies that the current user is owner iff the private owner key is set.
  bool owner_key_loaded_with_tpm_token_;

  scoped_ptr<enterprise_management::PolicyData> policy_data_;
  scoped_ptr<enterprise_management::ChromeDeviceSettingsProto> device_settings_;

  // The queue of pending operations. The first operation on the queue is
  // currently active; it gets removed and destroyed once it completes.
  std::deque<SessionManagerOperation*> pending_operations_;

  ObserverList<Observer, true> observers_;

  // For recoverable load errors how many retries are left before we give up.
  int load_retries_left_;

  base::WeakPtrFactory<DeviceSettingsService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DeviceSettingsService);
};

// Helper class for tests. Initializes the DeviceSettingsService singleton on
// construction and tears it down again on destruction.
class ScopedTestDeviceSettingsService {
 public:
  ScopedTestDeviceSettingsService();
  ~ScopedTestDeviceSettingsService();

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedTestDeviceSettingsService);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_SETTINGS_SERVICE_H_
