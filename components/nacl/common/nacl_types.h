// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_COMMON_NACL_TYPES_H_
#define COMPONENTS_NACL_COMMON_NACL_TYPES_H_

#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/shared_memory.h"
#include "base/process/process_handle.h"
#include "build/build_config.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_platform_file.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

#if defined(OS_WIN)
#include <windows.h>   // for HANDLE
#endif

// TODO(gregoryd): add a Windows definition for base::FileDescriptor
namespace nacl {

#if defined(OS_WIN)
typedef HANDLE FileDescriptor;
inline HANDLE ToNativeHandle(const FileDescriptor& desc) {
  return desc;
}
#elif defined(OS_POSIX)
typedef base::FileDescriptor FileDescriptor;
inline int ToNativeHandle(const FileDescriptor& desc) {
  return desc.fd;
}
#endif

// We allocate a page of shared memory for sharing crash information from
// trusted code in the NaCl process to the renderer.
static const int kNaClCrashInfoShmemSize = 4096;
static const int kNaClCrashInfoMaxLogSize = 1024;

// Types of untrusted NaCl processes.
enum NaClAppProcessType {
  kUnknownNaClProcessType,
  // Runs user-provided *native* code. Enabled for Chrome Web Store apps.
  kNativeNaClProcessType,
  // Runs user-provided code that is translated from *bitcode* by an
  // in-browser PNaCl translator.
  kPNaClProcessType,
  // Runs pnacl-llc/linker *native* code. These nexes are browser-provided
  // (not user-provided).
  kPNaClTranslatorProcessType,
  kNumNaClProcessTypes
};

// Represents a request to prefetch a file that's listed in the "files" section
// of a NaCl manifest file.
struct NaClResourcePrefetchRequest {
  NaClResourcePrefetchRequest();
  NaClResourcePrefetchRequest(const std::string& file_key,
                              const std::string& resource_url);
  ~NaClResourcePrefetchRequest();

  std::string file_key;  // a key for open_resource.
  std::string resource_url;
};

// Represents a single prefetched file that's listed in the "files" section of
// a NaCl manifest file.
struct NaClResourcePrefetchResult {
  NaClResourcePrefetchResult();
  NaClResourcePrefetchResult(IPC::PlatformFileForTransit file,
                             const base::FilePath& file_path,
                             const std::string& file_key);
  ~NaClResourcePrefetchResult();

  IPC::PlatformFileForTransit file;
  base::FilePath file_path_metadata;  // a key for validation caching
  std::string file_key;  // a key for open_resource
};

// Parameters sent to the NaCl process when we start it.
struct NaClStartParams {
  NaClStartParams();
  ~NaClStartParams();

  IPC::PlatformFileForTransit nexe_file;
  // Used only as a key for validation caching.
  base::FilePath nexe_file_path_metadata;

  std::vector<NaClResourcePrefetchResult> prefetched_resource_files;
  IPC::PlatformFileForTransit imc_bootstrap_handle;
  IPC::PlatformFileForTransit irt_handle;
#if defined(OS_MACOSX)
  IPC::PlatformFileForTransit mac_shm_fd;
#endif
#if defined(OS_POSIX)
  IPC::PlatformFileForTransit debug_stub_server_bound_socket;
#endif

  bool validation_cache_enabled;
  std::string validation_cache_key;
  // Chrome version string. Sending the version string over IPC avoids linkage
  // issues in cases where NaCl is not compiled into the main Chromium
  // executable or DLL.
  std::string version;

  bool enable_debug_stub;
  bool enable_ipc_proxy;

  NaClAppProcessType process_type;

  // For NaCl <-> renderer crash information reporting.
  base::SharedMemoryHandle crash_info_shmem_handle;

  // NOTE: Any new fields added here must also be added to the IPC
  // serialization in nacl_messages.h and (for POD fields) the constructor
  // in nacl_types.cc.
};

// Parameters sent to the browser process to have it launch a NaCl process.
//
// If you change this, you will also need to update the IPC serialization in
// nacl_host_messages.h.
struct NaClLaunchParams {
  NaClLaunchParams();
  NaClLaunchParams(
      const std::string& manifest_url,
      const IPC::PlatformFileForTransit& nexe_file,
      uint64_t nexe_token_lo,
      uint64_t nexe_token_hi,
      const std::vector<
          NaClResourcePrefetchRequest>& resource_prefetch_request_list,
      int render_view_id,
      uint32 permission_bits,
      bool uses_nonsfi_mode,
      NaClAppProcessType process_type);
  ~NaClLaunchParams();

  std::string manifest_url;
  // On Windows, the HANDLE passed here is valid in the renderer's context.
  // It's the responsibility of the browser to duplicate this handle properly
  // for passing it to the plugin.
  IPC::PlatformFileForTransit nexe_file;
  uint64_t nexe_token_lo;
  uint64_t nexe_token_hi;
  std::vector<NaClResourcePrefetchRequest> resource_prefetch_request_list;

  int render_view_id;
  uint32 permission_bits;
  bool uses_nonsfi_mode;

  NaClAppProcessType process_type;
};

struct NaClLaunchResult {
  NaClLaunchResult();
  NaClLaunchResult(
      FileDescriptor imc_channel_handle,
      const IPC::ChannelHandle& ppapi_ipc_channel_handle,
      const IPC::ChannelHandle& trusted_ipc_channel_handle,
      const IPC::ChannelHandle& manifest_service_ipc_channel_handle,
      base::ProcessId plugin_pid,
      int plugin_child_id,
      base::SharedMemoryHandle crash_info_shmem_handle);
  ~NaClLaunchResult();

  // For plugin loader <-> renderer IMC communication.
  FileDescriptor imc_channel_handle;

  // For plugin <-> renderer PPAPI communication.
  IPC::ChannelHandle ppapi_ipc_channel_handle;

  // For plugin loader <-> renderer control communication (loading and
  // starting nexe).
  IPC::ChannelHandle trusted_ipc_channel_handle;

  // For plugin <-> renderer ManifestService communication.
  IPC::ChannelHandle manifest_service_ipc_channel_handle;

  base::ProcessId plugin_pid;
  int plugin_child_id;

  // For NaCl <-> renderer crash information reporting.
  base::SharedMemoryHandle crash_info_shmem_handle;
};

}  // namespace nacl

#endif  // COMPONENTS_NACL_COMMON_NACL_TYPES_H_
