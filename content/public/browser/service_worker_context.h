// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CONTEXT_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "url/gurl.h"

namespace content {

class ServiceWorkerHost;
class ServiceWorkerHostClient;

// Represents the per-StoragePartition ServiceWorker data.  Must be used from
// the UI thread.
class ServiceWorkerContext {
 public:
  // https://rawgithub.com/slightlyoff/ServiceWorker/master/spec/service_worker/index.html#url-scope:
  // roughly, must be of the form "<origin>/<path>/*".
  typedef GURL Scope;

  typedef base::Callback<void(bool)> ResultCallback;

  // Equivalent to calling navigator.serviceWorker.register(script_url, {scope:
  // pattern}) from a renderer, except that |pattern| is an absolute URL instead
  // of relative to some current origin.
  //
  // Optionally provide a |client| for communication from the service worker.
  //
  // |callback| is passed a |ServiceWorkerHost| when the JS promise is fulfilled
  // or NULL when the JS promise is rejected. |callback| is run on UI thread.
  //
  // The registration can fail if:
  //  * |script_url| is on a different origin from |pattern|
  //  * Fetching |script_url| fails.
  //  * |script_url| fails to parse or its top-level execution fails.
  //    TODO: The error message for this needs to be available to developers.
  //  * Something unexpected goes wrong, like a renderer crash or a full disk.
  virtual void RegisterServiceWorker(const Scope& scope,
                                     const GURL& script_url,
                                     ServiceWorkerHostClient* client,
                                     const ResultCallback& callback) = 0;

  // Equivalent to calling navigator.serviceWorker.unregister(pattern) from a
  // renderer, except that |pattern| is an absolute URL instead of relative to
  // some current origin.
  //
  // |callback| is passed true when the JS promise is fulfilled or false when
  // the JS promise is rejected.  |callback| is run on UI thread.
  //
  // Unregistration can fail if:
  //  * No Service Worker was registered for |pattern|.
  //  * Something unexpected goes wrong, like a renderer crash.
  virtual void UnregisterServiceWorker(const Scope& pattern,
                                       const ResultCallback& callback) = 0;

  // Provides a ServiceWorkerHost object, via callback, for communicating with
  // the service worker registered for |scope|. May return NULL if there's an
  // error. Should the service worker be unregistered or for some other reason
  // become unavailable the ServiceWorkerHost will be deleted; test the weak
  // pointer before use.
  //
  // Optionally provide a |client| for communication from the service worker.
  //
  // |callback| is run on UI thread.
  virtual void GetServiceWorkerHost(const Scope& scope,
                                    ServiceWorkerHostClient* client,
                                    const ResultCallback& callback) = 0;

  // Synchronously releases all of the RenderProcessHosts that have Service
  // Workers running inside them, and prevents any new Service Worker instances
  // from starting up.
  virtual void Terminate() = 0;

 protected:
  ServiceWorkerContext() {}
  virtual ~ServiceWorkerContext() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CONTEXT_H_
