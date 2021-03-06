// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_creator.h"

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/proxy/proxy_server.h"

namespace {

scoped_ptr<base::Value> BuildDataReductionProxyEvent(
    net::NetLog::EventType type,
    const net::NetLog::Source& source,
    net::NetLog::EventPhase phase,
    const net::NetLog::ParametersCallback& parameters_callback) {
  base::TimeTicks ticks_now = base::TimeTicks::Now();
  net::NetLog::EntryData entry_data(type, source, phase, ticks_now,
                                    &parameters_callback);
  net::NetLog::Entry entry(&entry_data,
                           net::NetLogCaptureMode::IncludeSocketBytes());
  scoped_ptr<base::Value> entry_value(entry.ToValue());

  return entry_value;
}

int64 GetExpirationTicks(int bypass_seconds) {
  base::TimeTicks expiration_ticks =
      base::TimeTicks::Now() + base::TimeDelta::FromSeconds(bypass_seconds);
  return (expiration_ticks - base::TimeTicks()).InMilliseconds();
}

// The following method creates a string resembling the output of
// net::ProxyServer::ToURI().
std::string GetNormalizedProxyString(const std::string& proxy_origin) {
  net::ProxyServer proxy_server =
      net::ProxyServer::FromURI(proxy_origin, net::ProxyServer::SCHEME_HTTP);
  if (proxy_server.is_valid())
    return proxy_origin;

  return std::string();
}

// A callback which creates a base::Value containing information about enabling
// the Data Reduction Proxy. Ownership of the base::Value is passed to the
// caller.
base::Value* EnableDataReductionProxyCallback(
    bool primary_restricted,
    bool fallback_restricted,
    const std::string& primary_origin,
    const std::string& fallback_origin,
    const std::string& ssl_origin,
    net::NetLogCaptureMode /* capture_mode */) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetBoolean("enabled", true);
  dict->SetBoolean("primary_restricted", primary_restricted);
  dict->SetBoolean("fallback_restricted", fallback_restricted);
  dict->SetString("primary_origin", GetNormalizedProxyString(primary_origin));
  dict->SetString("fallback_origin", GetNormalizedProxyString(fallback_origin));
  dict->SetString("ssl_origin", GetNormalizedProxyString(ssl_origin));
  return dict;
}

// A callback which creates a base::Value containing information about disabling
// the Data Reduction Proxy. Ownership of the base::Value is passed to the
// caller.
base::Value* DisableDataReductionProxyCallback(
    net::NetLogCaptureMode /* capture_mode */) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetBoolean("enabled", false);
  return dict;
}

// A callback which creates a base::Value containing information about bypassing
// the Data Reduction Proxy. Ownership of the base::Value is passed to the
// caller.
base::Value* UrlBypassActionCallback(
    const std::string& action,
    const GURL& url,
    int bypass_seconds,
    int64 expiration_ticks,
    net::NetLogCaptureMode /* capture_mode */) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetString("action", action);
  dict->SetString("url", url.spec());
  dict->SetString("bypass_duration_seconds",
                  base::Int64ToString(bypass_seconds));
  dict->SetString("expiration", base::Int64ToString(expiration_ticks));
  return dict;
}

// A callback which creates a base::Value containing information about bypassing
// the Data Reduction Proxy. Ownership of the base::Value is passed to the
// caller.
base::Value* UrlBypassTypeCallback(
    data_reduction_proxy::DataReductionProxyBypassType bypass_type,
    const GURL& url,
    int bypass_seconds,
    int64 expiration_ticks,
    net::NetLogCaptureMode /* capture_mode */) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetInteger("bypass_type", bypass_type);
  dict->SetString("url", url.spec());
  dict->SetString("bypass_duration_seconds",
                  base::Int64ToString(bypass_seconds));
  dict->SetString("expiration", base::Int64ToString(expiration_ticks));
  return dict;
}

// A callback which creates a base::Value containing information about
// completing the Data Reduction Proxy secure proxy check. Ownership of the
// base::Value is passed to the caller.
base::Value* EndCanaryRequestCallback(
    int net_error,
    int http_response_code,
    bool succeeded,
    net::NetLogCaptureMode /* capture_mode */) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetInteger("net_error", net_error);
  dict->SetInteger("http_response_code", http_response_code);
  dict->SetBoolean("check_succeeded", succeeded);
  return dict;
}

// A callback that creates a base::Value containing information about
// completing the Data Reduction Proxy configuration request. Ownership of the
// base::Value is passed to the caller.
base::Value* EndConfigRequestCallback(
    int net_error,
    int http_response_code,
    int failure_count,
    int64 expiration_ticks,
    net::NetLogCaptureMode /* capture_mode */) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetInteger("net_error", net_error);
  dict->SetInteger("http_response_code", http_response_code);
  dict->SetInteger("failure_count", failure_count);
  dict->SetString("expiration", base::Int64ToString(expiration_ticks));
  return dict;
}

}  // namespace

namespace data_reduction_proxy {

DataReductionProxyEventCreator::DataReductionProxyEventCreator(
    DataReductionProxyEventStorageDelegate* storage_delegate)
    : storage_delegate_(storage_delegate) {
  DCHECK(storage_delegate);
  // Constructed on the UI thread, but should be checked on the IO thread.
  thread_checker_.DetachFromThread();
}

DataReductionProxyEventCreator::~DataReductionProxyEventCreator() {
}

void DataReductionProxyEventCreator::AddProxyEnabledEvent(
    net::NetLog* net_log,
    bool primary_restricted,
    bool fallback_restricted,
    const std::string& primary_origin,
    const std::string& fallback_origin,
    const std::string& ssl_origin) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const net::NetLog::ParametersCallback& parameters_callback = base::Bind(
      &EnableDataReductionProxyCallback, primary_restricted,
      fallback_restricted, primary_origin, fallback_origin, ssl_origin);
  PostEnabledEvent(net_log, net::NetLog::TYPE_DATA_REDUCTION_PROXY_ENABLED,
                   true, parameters_callback);
}

void DataReductionProxyEventCreator::AddProxyDisabledEvent(
    net::NetLog* net_log) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const net::NetLog::ParametersCallback& parameters_callback =
      base::Bind(&DisableDataReductionProxyCallback);
  PostEnabledEvent(net_log, net::NetLog::TYPE_DATA_REDUCTION_PROXY_ENABLED,
                   false, parameters_callback);
}

void DataReductionProxyEventCreator::AddBypassActionEvent(
    const net::BoundNetLog& net_log,
    const std::string& bypass_action,
    const GURL& url,
    const base::TimeDelta& bypass_duration) {
  DCHECK(thread_checker_.CalledOnValidThread());
  int64 expiration_ticks = GetExpirationTicks(bypass_duration.InSeconds());
  const net::NetLog::ParametersCallback& parameters_callback =
      base::Bind(&UrlBypassActionCallback, bypass_action, url,
                 bypass_duration.InSeconds(), expiration_ticks);
  PostBoundNetLogBypassEvent(
      net_log, net::NetLog::TYPE_DATA_REDUCTION_PROXY_BYPASS_REQUESTED,
      net::NetLog::PHASE_NONE, expiration_ticks, parameters_callback);
}

void DataReductionProxyEventCreator::AddBypassTypeEvent(
    const net::BoundNetLog& net_log,
    DataReductionProxyBypassType bypass_type,
    const GURL& url,
    const base::TimeDelta& bypass_duration) {
  DCHECK(thread_checker_.CalledOnValidThread());
  int64 expiration_ticks = GetExpirationTicks(bypass_duration.InSeconds());
  const net::NetLog::ParametersCallback& parameters_callback =
      base::Bind(&UrlBypassTypeCallback, bypass_type, url,
                 bypass_duration.InSeconds(), expiration_ticks);
  PostBoundNetLogBypassEvent(
      net_log, net::NetLog::TYPE_DATA_REDUCTION_PROXY_BYPASS_REQUESTED,
      net::NetLog::PHASE_NONE, expiration_ticks, parameters_callback);
}

void DataReductionProxyEventCreator::BeginSecureProxyCheck(
    const net::BoundNetLog& net_log,
    const GURL& url) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // This callback must be invoked synchronously
  const net::NetLog::ParametersCallback& parameters_callback =
      net::NetLog::StringCallback("url", &url.spec());
  PostBoundNetLogSecureProxyCheckEvent(
      net_log, net::NetLog::TYPE_DATA_REDUCTION_PROXY_CANARY_REQUEST,
      net::NetLog::PHASE_BEGIN,
      DataReductionProxyEventStorageDelegate::CHECK_PENDING,
      parameters_callback);
}

void DataReductionProxyEventCreator::EndSecureProxyCheck(
    const net::BoundNetLog& net_log,
    int net_error,
    int http_response_code,
    bool succeeded) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const net::NetLog::ParametersCallback& parameters_callback = base::Bind(
      &EndCanaryRequestCallback, net_error, http_response_code, succeeded);
  PostBoundNetLogSecureProxyCheckEvent(
      net_log, net::NetLog::TYPE_DATA_REDUCTION_PROXY_CANARY_REQUEST,
      net::NetLog::PHASE_END,
      net_error == 0 ? DataReductionProxyEventStorageDelegate::CHECK_SUCCESS
                     : DataReductionProxyEventStorageDelegate::CHECK_FAILED,
      parameters_callback);
}

void DataReductionProxyEventCreator::BeginConfigRequest(
    const net::BoundNetLog& net_log,
    const GURL& url) {
  // This callback must be invoked synchronously.
  const net::NetLog::ParametersCallback& parameters_callback =
      net::NetLog::StringCallback("url", &url.spec());
  PostBoundNetLogConfigRequestEvent(
      net_log, net::NetLog::TYPE_DATA_REDUCTION_PROXY_CONFIG_REQUEST,
      net::NetLog::PHASE_BEGIN, parameters_callback);
}

void DataReductionProxyEventCreator::EndConfigRequest(
    const net::BoundNetLog& net_log,
    int net_error,
    int http_response_code,
    int failure_count,
    const base::TimeDelta& retry_delay) {
  int64 expiration_ticks = GetExpirationTicks(retry_delay.InSeconds());
  const net::NetLog::ParametersCallback& parameters_callback =
      base::Bind(&EndConfigRequestCallback, net_error, http_response_code,
                 failure_count, expiration_ticks);
  PostBoundNetLogConfigRequestEvent(
      net_log, net::NetLog::TYPE_DATA_REDUCTION_PROXY_CONFIG_REQUEST,
      net::NetLog::PHASE_END, parameters_callback);
}

void DataReductionProxyEventCreator::PostEnabledEvent(
    net::NetLog* net_log,
    net::NetLog::EventType type,
    bool enabled,
    const net::NetLog::ParametersCallback& callback) {
  scoped_ptr<base::Value> event = BuildDataReductionProxyEvent(
      type, net::NetLog::Source(), net::NetLog::PHASE_NONE, callback);
  if (event)
    storage_delegate_->AddEnabledEvent(event.Pass(), enabled);

  if (net_log)
    net_log->AddGlobalEntry(type, callback);
}

void DataReductionProxyEventCreator::PostBoundNetLogBypassEvent(
    const net::BoundNetLog& net_log,
    net::NetLog::EventType type,
    net::NetLog::EventPhase phase,
    int64 expiration_ticks,
    const net::NetLog::ParametersCallback& callback) {
  scoped_ptr<base::Value> event =
      BuildDataReductionProxyEvent(type, net_log.source(), phase, callback);
  if (event)
    storage_delegate_->AddAndSetLastBypassEvent(event.Pass(), expiration_ticks);
  net_log.AddEntry(type, phase, callback);
}

void DataReductionProxyEventCreator::PostBoundNetLogSecureProxyCheckEvent(
    const net::BoundNetLog& net_log,
    net::NetLog::EventType type,
    net::NetLog::EventPhase phase,
    DataReductionProxyEventStorageDelegate::SecureProxyCheckState state,
    const net::NetLog::ParametersCallback& callback) {
  scoped_ptr<base::Value> event(
      BuildDataReductionProxyEvent(type, net_log.source(), phase, callback));
  if (event)
    storage_delegate_->AddEventAndSecureProxyCheckState(event.Pass(), state);
  net_log.AddEntry(type, phase, callback);
}

void DataReductionProxyEventCreator::PostBoundNetLogConfigRequestEvent(
    const net::BoundNetLog& net_log,
    net::NetLog::EventType type,
    net::NetLog::EventPhase phase,
    const net::NetLog::ParametersCallback& callback) {
  scoped_ptr<base::Value> event(
      BuildDataReductionProxyEvent(type, net_log.source(), phase, callback));
  if (event) {
    storage_delegate_->AddEvent(event.Pass());
    net_log.AddEntry(type, phase, callback);
  }
}

}  // namespace data_reduction_proxy
