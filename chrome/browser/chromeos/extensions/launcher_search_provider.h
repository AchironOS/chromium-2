// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LAUNCHER_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LAUNCHER_SEARCH_PROVIDER_H_

#include "chrome/browser/extensions/chrome_extension_function.h"

namespace extensions {

// Implements chrome.launcherSearchProvider.setSearchResults method.
class LauncherSearchProviderSetSearchResultsFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("launcherSearchProvider.setSearchResults",
                             LAUNCHERSEARCHPROVIDER_SETSEARCHRESULTS)
 protected:
  ~LauncherSearchProviderSetSearchResultsFunction() override;
  bool RunSync() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_LAUNCHER_SEARCH_PROVIDER_H_
