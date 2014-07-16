// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_context_wrapper.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_observer.h"
#include "content/browser/service_worker/service_worker_host_impl.h"
#include "content/browser/service_worker/service_worker_process_manager.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_worker_host_client.h"
#include "ipc/ipc_message.h"
#include "webkit/browser/quota/quota_manager_proxy.h"

namespace {

using content::BrowserThread;
using content::SERVICE_WORKER_OK;
using content::ServiceWorkerContext;
using content::ServiceWorkerRegistration;
using content::ServiceWorkerStatusCode;

void PostResultToUIFromStatusOnIO(
    const ServiceWorkerContext::ResultCallback& callback,
    ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!callback.is_null()) {
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(callback, status == SERVICE_WORKER_OK));
  }
}

}  // namespace

namespace content {

ServiceWorkerContextWrapper::ServiceWorkerContextWrapper(
    BrowserContext* browser_context)
    : observer_list_(
          new ObserverListThreadSafe<ServiceWorkerContextObserver>()),
      process_manager_(new ServiceWorkerProcessManager(browser_context)),
      is_incognito_(false) {
}

ServiceWorkerContextWrapper::~ServiceWorkerContextWrapper() {
}

void ServiceWorkerContextWrapper::Init(
    const base::FilePath& user_data_directory,
    quota::QuotaManagerProxy* quota_manager_proxy) {
  is_incognito_ = user_data_directory.empty();
  scoped_refptr<base::SequencedTaskRunner> database_task_runner =
      BrowserThread::GetBlockingPool()->
          GetSequencedTaskRunnerWithShutdownBehavior(
              BrowserThread::GetBlockingPool()->GetSequenceToken(),
              base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
  scoped_refptr<base::MessageLoopProxy> disk_cache_thread =
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::CACHE);
  InitInternal(user_data_directory, database_task_runner,
               disk_cache_thread, quota_manager_proxy);
}

void ServiceWorkerContextWrapper::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  process_manager_->Shutdown();
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&ServiceWorkerContextWrapper::ShutdownOnIO, this));
}

void ServiceWorkerContextWrapper::DeleteAndStartOver() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  context_core_->DeleteAndStartOver(
      base::Bind(&ServiceWorkerContextWrapper::DidDeleteAndStartOver, this));
}

ServiceWorkerContextCore* ServiceWorkerContextWrapper::context() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return context_core_.get();
}

void ServiceWorkerContextWrapper::RegisterServiceWorker(
    const Scope& scope,
    const GURL& script_url,
    ServiceWorkerHostClient* client,
    const ResultCallback& callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&ServiceWorkerContextWrapper::RegisterServiceWorker,
                   this,
                   scope,
                   script_url,
                   client,
                   callback));
    return;
  }

  context()->RegisterServiceWorker(
      scope,
      script_url,
      -1,
      NULL /* provider_host */,
      base::Bind(&ServiceWorkerContextWrapper::FinishRegistrationOnIO,
                 this,
                 scope,
                 client,
                 callback));
}

void ServiceWorkerContextWrapper::UnregisterServiceWorker(
    const GURL& scope,
    const ResultCallback& callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&ServiceWorkerContextWrapper::UnregisterServiceWorker,
                   this,
                   scope,
                   callback));
    return;
  }

  context()->UnregisterServiceWorker(
      scope, base::Bind(&PostResultToUIFromStatusOnIO, callback));
}

void ServiceWorkerContextWrapper::GetServiceWorkerHost(
    const Scope& scope,
    ServiceWorkerHostClient* client,
    const ResultCallback& callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&ServiceWorkerContextWrapper::UnregisterServiceWorker,
                   this,
                   scope,
                   callback));
    return;
  }

  context()->storage()->FindRegistrationForPattern(
      scope,
      base::Bind(&ServiceWorkerContextWrapper::FinishGetServiceWorkerHostOnIO,
                 this,
                 scope,
                 client,
                 callback));
}

void ServiceWorkerContextWrapper::Terminate() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  process_manager_->Shutdown();
}

void ServiceWorkerContextWrapper::AddObserver(
    ServiceWorkerContextObserver* observer) {
  observer_list_->AddObserver(observer);
}

void ServiceWorkerContextWrapper::RemoveObserver(
    ServiceWorkerContextObserver* observer) {
  observer_list_->RemoveObserver(observer);
}

void ServiceWorkerContextWrapper::InitInternal(
    const base::FilePath& user_data_directory,
    base::SequencedTaskRunner* database_task_runner,
    base::MessageLoopProxy* disk_cache_thread,
    quota::QuotaManagerProxy* quota_manager_proxy) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&ServiceWorkerContextWrapper::InitInternal,
                   this,
                   user_data_directory,
                   make_scoped_refptr(database_task_runner),
                   make_scoped_refptr(disk_cache_thread),
                   make_scoped_refptr(quota_manager_proxy)));
    return;
  }
  DCHECK(!context_core_);
  context_core_.reset(new ServiceWorkerContextCore(user_data_directory,
                                                   database_task_runner,
                                                   disk_cache_thread,
                                                   quota_manager_proxy,
                                                   observer_list_,
                                                   this));
}

void ServiceWorkerContextWrapper::ShutdownOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  context_core_.reset();
}

void ServiceWorkerContextWrapper::DidDeleteAndStartOver(
    ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status != SERVICE_WORKER_OK) {
    context_core_.reset();
    return;
  }
  context_core_.reset(new ServiceWorkerContextCore(context_core_.get(), this));
  DVLOG(1) << "Restarted ServiceWorkerContextCore successfully.";
}

void ServiceWorkerContextWrapper::FinishRegistrationOnIO(
    const Scope& scope,
    ServiceWorkerHostClient* client,
    const ResultCallback& callback,
    ServiceWorkerStatusCode status,
    int64 registration_id,
    int64 version_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status == SERVICE_WORKER_OK) {
    GetServiceWorkerHost(scope, client, callback);
  } else {
    PostResultToUIFromStatusOnIO(callback, status);
  }
}

void ServiceWorkerContextWrapper::FinishGetServiceWorkerHostOnIO(
    const Scope& scope,
    ServiceWorkerHostClient* client,
    const ServiceWorkerContext::ResultCallback& callback,
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status == SERVICE_WORKER_OK && client) {
    client->set_service_worker_host(
        new ServiceWorkerHostImpl(scope, this, registration, client));
  }
  PostResultToUIFromStatusOnIO(callback, status);
}

}  // namespace content
