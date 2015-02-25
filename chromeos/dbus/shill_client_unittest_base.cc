// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/shill_client_unittest_base.h"

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chromeos/network/shill_property_util.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/values_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace chromeos {

namespace {

// Pops a string-to-string dictionary from the reader.
base::DictionaryValue* PopStringToStringDictionary(
    dbus::MessageReader* reader) {
  dbus::MessageReader array_reader(NULL);
  if (!reader->PopArray(&array_reader))
    return NULL;
  scoped_ptr<base::DictionaryValue> result(new base::DictionaryValue);
  while (array_reader.HasMoreData()) {
    dbus::MessageReader entry_reader(NULL);
    std::string key;
    std::string value;
    if (!array_reader.PopDictEntry(&entry_reader) ||
        !entry_reader.PopString(&key) ||
        !entry_reader.PopString(&value))
      return NULL;
    result->SetWithoutPathExpansion(key, new base::StringValue(value));
  }
  return result.release();
}

}  // namespace

ValueMatcher::ValueMatcher(const base::Value& value)
  : expected_value_(value.DeepCopy()) {}

bool ValueMatcher::MatchAndExplain(const base::Value& value,
                             MatchResultListener* listener) const {
  return expected_value_->Equals(&value);
}

void ValueMatcher::DescribeTo(::std::ostream* os) const {
  std::string expected_value_str;
  base::JSONWriter::WriteWithOptions(expected_value_.get(),
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &expected_value_str);
  *os << "value equals " << expected_value_str;
}

void ValueMatcher::DescribeNegationTo(::std::ostream* os) const {
  std::string expected_value_str;
  base::JSONWriter::WriteWithOptions(expected_value_.get(),
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &expected_value_str);
  *os << "value does not equal " << expected_value_str;
}


ShillClientUnittestBase::MockClosure::MockClosure() {}

ShillClientUnittestBase::MockClosure::~MockClosure() {}

base::Closure ShillClientUnittestBase::MockClosure::GetCallback() {
  return base::Bind(&MockClosure::Run, base::Unretained(this));
}


ShillClientUnittestBase::MockListValueCallback::MockListValueCallback() {}

ShillClientUnittestBase::MockListValueCallback::~MockListValueCallback() {}

ShillClientHelper::ListValueCallback
ShillClientUnittestBase::MockListValueCallback::GetCallback() {
  return base::Bind(&MockListValueCallback::Run, base::Unretained(this));
}


ShillClientUnittestBase::MockErrorCallback::MockErrorCallback() {}

ShillClientUnittestBase::MockErrorCallback::~MockErrorCallback() {}

ShillClientUnittestBase::MockPropertyChangeObserver::
  MockPropertyChangeObserver() {}

ShillClientUnittestBase::MockPropertyChangeObserver::
  ~MockPropertyChangeObserver() {}

ShillClientHelper::ErrorCallback
ShillClientUnittestBase::MockErrorCallback::GetCallback() {
  return base::Bind(&MockErrorCallback::Run, base::Unretained(this));
}


ShillClientUnittestBase::ShillClientUnittestBase(
    const std::string& interface_name,
    const dbus::ObjectPath& object_path)
    : interface_name_(interface_name),
      object_path_(object_path),
      response_(NULL) {
}

ShillClientUnittestBase::~ShillClientUnittestBase() {
}

void ShillClientUnittestBase::SetUp() {
  // Create a mock bus.
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  mock_bus_ = new dbus::MockBus(options);

  // Create a mock proxy.
  mock_proxy_ = new dbus::MockObjectProxy(
      mock_bus_.get(),
      shill::kFlimflamServiceName,
      object_path_);

  // Set an expectation so mock_proxy's CallMethod() will use OnCallMethod()
  // to return responses.
  EXPECT_CALL(*mock_proxy_.get(), CallMethod(_, _, _))
      .WillRepeatedly(Invoke(this, &ShillClientUnittestBase::OnCallMethod));

  // Set an expectation so mock_proxy's CallMethodWithErrorCallback() will use
  // OnCallMethodWithErrorCallback() to return responses.
  EXPECT_CALL(*mock_proxy_.get(), CallMethodWithErrorCallback(_, _, _, _))
      .WillRepeatedly(Invoke(
           this, &ShillClientUnittestBase::OnCallMethodWithErrorCallback));

  // Set an expectation so mock_proxy's ConnectToSignal() will use
  // OnConnectToPropertyChanged() to run the callback.
  EXPECT_CALL(
      *mock_proxy_.get(),
      ConnectToSignal(interface_name_, shill::kMonitorPropertyChanged, _, _))
      .WillRepeatedly(
           Invoke(this, &ShillClientUnittestBase::OnConnectToPropertyChanged));

  EXPECT_CALL(
      *mock_proxy_.get(),
      ConnectToSignal(interface_name_, shill::kOnPlatformMessageFunction, _, _))
      .WillRepeatedly(
          Invoke(this, &ShillClientUnittestBase::OnConnectToPlatformMessage));

  EXPECT_CALL(
      *mock_proxy_.get(),
      ConnectToSignal(interface_name_, shill::kOnPacketReceivedFunction, _, _))
      .WillRepeatedly(
          Invoke(this, &ShillClientUnittestBase::OnConnectToPacketReceived));

  // Set an expectation so mock_bus's GetObjectProxy() for the given
  // service name and the object path will return mock_proxy_.
  EXPECT_CALL(*mock_bus_.get(),
              GetObjectProxy(shill::kFlimflamServiceName, object_path_))
      .WillOnce(Return(mock_proxy_.get()));

  // Set an expectation so mock_bus's GetDBusTaskRunner will return the current
  // task runner.
  EXPECT_CALL(*mock_bus_.get(), GetDBusTaskRunner())
      .WillRepeatedly(Return(message_loop_.message_loop_proxy().get()));

  // ShutdownAndBlock() will be called in TearDown().
  EXPECT_CALL(*mock_bus_.get(), ShutdownAndBlock()).WillOnce(Return());
}

void ShillClientUnittestBase::TearDown() {
  mock_bus_->ShutdownAndBlock();
}

void ShillClientUnittestBase::PrepareForMethodCall(
    const std::string& method_name,
    const ArgumentCheckCallback& argument_checker,
    dbus::Response* response) {
  expected_method_name_ = method_name;
  argument_checker_ = argument_checker;
  response_ = response;
}

void ShillClientUnittestBase::SendPlatformMessageSignal(
    dbus::Signal* signal) {
  ASSERT_FALSE(platform_message_handler_.is_null());
  platform_message_handler_.Run(signal);
}

void ShillClientUnittestBase::SendPacketReceievedSignal(
    dbus::Signal* signal) {
  ASSERT_FALSE(packet_receieved__handler_.is_null());
  packet_receieved__handler_.Run(signal);
}

void ShillClientUnittestBase::SendPropertyChangedSignal(
    dbus::Signal* signal) {
  ASSERT_FALSE(property_changed_handler_.is_null());
  property_changed_handler_.Run(signal);
}

// static
void ShillClientUnittestBase::ExpectPropertyChanged(
    const std::string& expected_name,
    const base::Value* expected_value,
    const std::string& name,
    const base::Value& value) {
  EXPECT_EQ(expected_name, name);
  EXPECT_TRUE(expected_value->Equals(&value));
}

// static
void ShillClientUnittestBase::ExpectNoArgument(dbus::MessageReader* reader) {
  EXPECT_FALSE(reader->HasMoreData());
}

// static
void ShillClientUnittestBase::ExpectUint32Argument(
    uint32_t expected_value,
    dbus::MessageReader* reader) {
  uint32_t value;
  ASSERT_TRUE(reader->PopUint32(&value));
  EXPECT_EQ(expected_value, value);
  EXPECT_FALSE(reader->HasMoreData());
}

// static
void ShillClientUnittestBase::ExpectArrayOfBytesArgument(
    const std::string& expected_bytes,
    dbus::MessageReader* reader) {
  const uint8_t* bytes = nullptr;
  size_t size = 0;
  ASSERT_TRUE(reader->PopArrayOfBytes(&bytes, &size));
  EXPECT_EQ(expected_bytes.size(), size);
  for (size_t i = 0; i < size; ++i) {
    EXPECT_EQ(expected_bytes[i], bytes[i]);
  }
  EXPECT_FALSE(reader->HasMoreData());
}

// static
void ShillClientUnittestBase::ExpectStringArgument(
    const std::string& expected_string,
    dbus::MessageReader* reader) {
  std::string str;
  ASSERT_TRUE(reader->PopString(&str));
  EXPECT_EQ(expected_string, str);
  EXPECT_FALSE(reader->HasMoreData());
}

// static
void ShillClientUnittestBase::ExpectArrayOfStringsArgument(
    const std::vector<std::string>& expected_strings,
    dbus::MessageReader* reader) {
  std::vector<std::string> strs;
  ASSERT_TRUE(reader->PopArrayOfStrings(&strs));
  EXPECT_EQ(expected_strings, strs);
  EXPECT_FALSE(reader->HasMoreData());
}

// static
void ShillClientUnittestBase::ExpectValueArgument(
    const base::Value* expected_value,
    dbus::MessageReader* reader) {
  scoped_ptr<base::Value> value(dbus::PopDataAsValue(reader));
  ASSERT_TRUE(value.get());
  EXPECT_TRUE(value->Equals(expected_value));
  EXPECT_FALSE(reader->HasMoreData());
}

// static
void ShillClientUnittestBase::ExpectStringAndValueArguments(
    const std::string& expected_string,
    const base::Value* expected_value,
    dbus::MessageReader* reader) {
  std::string str;
  ASSERT_TRUE(reader->PopString(&str));
  EXPECT_EQ(expected_string, str);
  scoped_ptr<base::Value> value(dbus::PopDataAsValue(reader));
  ASSERT_TRUE(value.get());
  EXPECT_TRUE(value->Equals(expected_value));
  EXPECT_FALSE(reader->HasMoreData());
}

// static
void ShillClientUnittestBase::ExpectDictionaryValueArgument(
    const base::DictionaryValue* expected_dictionary,
    bool string_valued,
    dbus::MessageReader* reader) {
  dbus::MessageReader array_reader(NULL);
  ASSERT_TRUE(reader->PopArray(&array_reader));
  while (array_reader.HasMoreData()) {
    dbus::MessageReader entry_reader(NULL);
    ASSERT_TRUE(array_reader.PopDictEntry(&entry_reader));
    std::string key;
    ASSERT_TRUE(entry_reader.PopString(&key));
    if (string_valued) {
      std::string value;
      std::string expected_value;
      ASSERT_TRUE(entry_reader.PopString(&value));
      EXPECT_TRUE(expected_dictionary->GetStringWithoutPathExpansion(
          key, &expected_value));
      EXPECT_EQ(expected_value, value);
      continue;
    }
    dbus::MessageReader variant_reader(NULL);
    ASSERT_TRUE(entry_reader.PopVariant(&variant_reader));
    scoped_ptr<base::Value> value;
    // Variants in the dictionary can be basic types or string-to-string
    // dictinoary.
    switch (variant_reader.GetDataType()) {
      case dbus::Message::ARRAY:
        value.reset(PopStringToStringDictionary(&variant_reader));
        break;
      case dbus::Message::BOOL:
      case dbus::Message::INT32:
      case dbus::Message::STRING:
        value.reset(dbus::PopDataAsValue(&variant_reader));
        break;
      default:
        NOTREACHED();
    }
    ASSERT_TRUE(value.get());
    const base::Value* expected_value = NULL;
    EXPECT_TRUE(expected_dictionary->GetWithoutPathExpansion(key,
                                                             &expected_value));
    EXPECT_TRUE(value->Equals(expected_value));
  }
}

// static
base::DictionaryValue*
ShillClientUnittestBase::CreateExampleServiceProperties() {
  base::DictionaryValue* properties = new base::DictionaryValue;
  properties->SetWithoutPathExpansion(
      shill::kGuidProperty,
      new base::StringValue("00000000-0000-0000-0000-000000000000"));
  properties->SetWithoutPathExpansion(
      shill::kModeProperty, new base::StringValue(shill::kModeManaged));
  properties->SetWithoutPathExpansion(shill::kTypeProperty,
                                      new base::StringValue(shill::kTypeWifi));
  shill_property_util::SetSSID("testssid", properties);
  properties->SetWithoutPathExpansion(
      shill::kSecurityClassProperty,
      new base::StringValue(shill::kSecurityPsk));
  return properties;
}


// static
void ShillClientUnittestBase::ExpectNoResultValue(
    DBusMethodCallStatus call_status) {
  EXPECT_EQ(DBUS_METHOD_CALL_SUCCESS, call_status);
}

// static
void ShillClientUnittestBase::ExpectObjectPathResult(
    const dbus::ObjectPath& expected_result,
    DBusMethodCallStatus call_status,
    const dbus::ObjectPath& result) {
  EXPECT_EQ(DBUS_METHOD_CALL_SUCCESS, call_status);
  EXPECT_EQ(expected_result, result);
}

// static
void ShillClientUnittestBase::ExpectObjectPathResultWithoutStatus(
    const dbus::ObjectPath& expected_result,
    const dbus::ObjectPath& result) {
  EXPECT_EQ(expected_result, result);
}

// static
void ShillClientUnittestBase::ExpectBoolResultWithoutStatus(
    bool expected_result,
    bool result) {
  EXPECT_EQ(expected_result, result);
}

// static
void ShillClientUnittestBase::ExpectStringResultWithoutStatus(
    const std::string& expected_result,
    const std::string& result) {
  EXPECT_EQ(expected_result, result);
}

// static
void ShillClientUnittestBase::ExpectDictionaryValueResultWithoutStatus(
    const base::DictionaryValue* expected_result,
    const base::DictionaryValue& result) {
  std::string expected_result_string;
  base::JSONWriter::Write(expected_result, &expected_result_string);
  std::string result_string;
  base::JSONWriter::Write(&result, &result_string);
  EXPECT_EQ(expected_result_string, result_string);
}

// static
void ShillClientUnittestBase::ExpectDictionaryValueResult(
    const base::DictionaryValue* expected_result,
    DBusMethodCallStatus call_status,
    const base::DictionaryValue& result) {
  EXPECT_EQ(DBUS_METHOD_CALL_SUCCESS, call_status);
  ExpectDictionaryValueResultWithoutStatus(expected_result, result);
}

void ShillClientUnittestBase::OnConnectToPlatformMessage(
    const std::string& interface_name,
    const std::string& signal_name,
    const dbus::ObjectProxy::SignalCallback& signal_callback,
    const dbus::ObjectProxy::OnConnectedCallback& on_connected_callback) {
  platform_message_handler_ = signal_callback;
  const bool success = true;
  message_loop_.PostTask(FROM_HERE,
                         base::Bind(on_connected_callback,
                                    interface_name,
                                    signal_name,
                                    success));
}

void ShillClientUnittestBase::OnConnectToPacketReceived(
    const std::string& interface_name,
    const std::string& signal_name,
    const dbus::ObjectProxy::SignalCallback& signal_callback,
    const dbus::ObjectProxy::OnConnectedCallback& on_connected_callback) {
  packet_receieved__handler_ = signal_callback;
  const bool success = true;
  message_loop_.PostTask(FROM_HERE,
                         base::Bind(on_connected_callback,
                                    interface_name,
                                    signal_name,
                                    success));
}

void ShillClientUnittestBase::OnConnectToPropertyChanged(
    const std::string& interface_name,
    const std::string& signal_name,
    const dbus::ObjectProxy::SignalCallback& signal_callback,
    const dbus::ObjectProxy::OnConnectedCallback& on_connected_callback) {
  property_changed_handler_ = signal_callback;
  const bool success = true;
  message_loop_.PostTask(FROM_HERE,
                         base::Bind(on_connected_callback,
                                    interface_name,
                                    signal_name,
                                    success));
}

void ShillClientUnittestBase::OnCallMethod(
    dbus::MethodCall* method_call,
    int timeout_ms,
    const dbus::ObjectProxy::ResponseCallback& response_callback) {
  EXPECT_EQ(interface_name_, method_call->GetInterface());
  EXPECT_EQ(expected_method_name_, method_call->GetMember());
  dbus::MessageReader reader(method_call);
  argument_checker_.Run(&reader);
  message_loop_.PostTask(FROM_HERE,
                         base::Bind(response_callback, response_));
}

void ShillClientUnittestBase::OnCallMethodWithErrorCallback(
    dbus::MethodCall* method_call,
    int timeout_ms,
    const dbus::ObjectProxy::ResponseCallback& response_callback,
    const dbus::ObjectProxy::ErrorCallback& error_callback) {
  OnCallMethod(method_call, timeout_ms, response_callback);
}

}  // namespace chromeos
