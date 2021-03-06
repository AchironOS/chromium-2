// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/switches.h"

#include "base/basictypes.h"

namespace switches {

// If set apps downloaded are not deleted.
const char kDontDeleteOnDownload[] = "dont-delete-on-download";

// If set apps downloaded are saved in with a predictable filename, to help
// remote debugging: when gdb is used through gdbserver, it needs to be able to
// find locally any loaded library. For this, gdb use the filename of the
// library. When using this flag, the application are named with the sha256 of
// their content.
const char kPredictableAppFilenames[] = "predictable-app-filenames";

}  // namespace switches
