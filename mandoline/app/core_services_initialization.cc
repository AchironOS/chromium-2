// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/app/core_services_initialization.h"

#include "mojo/runner/context.h"

namespace mandoline {

void InitCoreServicesForContext(mojo::runner::Context* context) {
  // TODO(erg): We should probably handle this differently; these could be
  // autogenerated from package manifests.
  mojo::shell::ApplicationManager* manager = context->application_manager();
  manager->RegisterApplicationPackageAlias(GURL("mojo:clipboard"),
                                           GURL("mojo:core_services"), "Core");
#if !defined(OS_ANDROID)
  manager->RegisterApplicationPackageAlias(
      GURL("mojo:network_service"), GURL("mojo:core_services"), "Network");
#endif
  manager->RegisterApplicationPackageAlias(
      GURL("mojo:surfaces_service"), GURL("mojo:core_services"), "Surfaces");
  manager->RegisterApplicationPackageAlias(GURL("mojo:tracing"),
                                           GURL("mojo:core_services"), "Core");
  manager->RegisterApplicationPackageAlias(GURL("mojo:view_manager"),
                                           GURL("mojo:core_services"), "Core");
  manager->RegisterApplicationPackageAlias(GURL("mojo:window_manager"),
                                           GURL("mojo:core_services"), "Core");
}

}  // namespace mandoline
