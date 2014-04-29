// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_COMMON_TEST_TEST_UTILS_H_
#define MOJO_COMMON_TEST_TEST_UTILS_H_

#include <stddef.h>

#include <string>

#include "base/files/file_path.h"

namespace mojo {

namespace embedder {
struct PlatformHandle;
}

namespace test {

// On success, |bytes_written| is updated to the number of bytes written;
// otherwise it is untouched.
bool BlockingWrite(const embedder::PlatformHandle& handle,
                   const void* buffer,
                   size_t bytes_to_write,
                   size_t* bytes_written);

// On success, |bytes_read| is updated to the number of bytes read; otherwise it
// is untouched.
bool BlockingRead(const embedder::PlatformHandle& handle,
                  void* buffer,
                  size_t buffer_size,
                  size_t* bytes_read);

// If the read is done successfully or would block, the function returns true
// and updates |bytes_read| to the number of bytes read (0 if the read would
// block); otherwise it returns false and leaves |bytes_read| untouched.
// |handle| must already be in non-blocking mode.
bool NonBlockingRead(const embedder::PlatformHandle& handle,
                     void* buffer,
                     size_t buffer_size,
                     size_t* bytes_read);

// Returns the path to the mojom js bindings file.
base::FilePath GetFilePathForJSResource(const std::string& path);

}  // namespace test
}  // namespace mojo

#endif  // MOJO_COMMON_TEST_TEST_UTILS_H_
