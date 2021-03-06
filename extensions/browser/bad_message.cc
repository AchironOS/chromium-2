// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/bad_message.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "content/public/browser/render_process_host.h"

namespace extensions {
namespace bad_message {

void ReceivedBadMessage(content::RenderProcessHost* host,
                        BadMessageReason reason) {
  LOG(ERROR) << "Terminating extension renderer for bad IPC message, reason "
             << reason;
  UMA_HISTOGRAM_ENUMERATION("Stability.BadMessageTerminated.Extensions", reason,
                            BAD_MESSAGE_MAX);
  host->ShutdownForBadMessage();
}

}  // namespace bad_message
}  // namespace extensions
