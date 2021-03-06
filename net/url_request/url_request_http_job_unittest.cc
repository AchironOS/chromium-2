// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_http_job.h"

#include <cstddef>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "net/base/auth.h"
#include "net/base/request_priority.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/http_transaction_test_util.h"
#include "net/socket/socket_test_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "net/websockets/websocket_handshake_stream_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {

using ::testing::Return;

// Inherit from URLRequestHttpJob to expose the priority and some
// other hidden functions.
class TestURLRequestHttpJob : public URLRequestHttpJob {
 public:
  explicit TestURLRequestHttpJob(URLRequest* request)
      : URLRequestHttpJob(request, request->context()->network_delegate(),
                          request->context()->http_user_agent_settings()) {}

  using URLRequestHttpJob::SetPriority;
  using URLRequestHttpJob::Start;
  using URLRequestHttpJob::Kill;
  using URLRequestHttpJob::priority;

 protected:
  ~TestURLRequestHttpJob() override {}
};

class URLRequestHttpJobTest : public ::testing::Test {
 protected:
  URLRequestHttpJobTest()
      : req_(context_.CreateRequest(GURL("http://www.example.com"),
                                    DEFAULT_PRIORITY,
                                    &delegate_)) {
    context_.set_http_transaction_factory(&network_layer_);
  }

  bool TransactionAcceptsSdchEncoding() {
    base::WeakPtr<MockNetworkTransaction> transaction(
        network_layer_.last_transaction());
    EXPECT_TRUE(transaction);
    if (!transaction) return false;

    const HttpRequestInfo* request_info = transaction->request();
    EXPECT_TRUE(request_info);
    if (!request_info) return false;

    std::string encoding_headers;
    bool get_success = request_info->extra_headers.GetHeader(
        "Accept-Encoding", &encoding_headers);
    EXPECT_TRUE(get_success);
    if (!get_success) return false;

    // This check isn't wrapped with EXPECT* macros because different
    // results from this function may be expected in different tests.
    std::vector<std::string> tokens;
    size_t num_tokens = Tokenize(encoding_headers, ", ", &tokens);
    for (size_t i = 0; i < num_tokens; i++) {
      if (!base::strncasecmp(tokens[i].data(), "sdch", tokens[i].length()))
        return true;
    }
    return false;
  }

  void EnableSdch() {
    context_.SetSdchManager(scoped_ptr<SdchManager>(new SdchManager).Pass());
  }

  MockNetworkLayer network_layer_;
  TestURLRequestContext context_;
  TestDelegate delegate_;
  scoped_ptr<URLRequest> req_;
};

// Make sure that SetPriority actually sets the URLRequestHttpJob's
// priority, both before and after start.
TEST_F(URLRequestHttpJobTest, SetPriorityBasic) {
  scoped_refptr<TestURLRequestHttpJob> job(
      new TestURLRequestHttpJob(req_.get()));
  EXPECT_EQ(DEFAULT_PRIORITY, job->priority());

  job->SetPriority(LOWEST);
  EXPECT_EQ(LOWEST, job->priority());

  job->SetPriority(LOW);
  EXPECT_EQ(LOW, job->priority());

  job->Start();
  EXPECT_EQ(LOW, job->priority());

  job->SetPriority(MEDIUM);
  EXPECT_EQ(MEDIUM, job->priority());
}

// Make sure that URLRequestHttpJob passes on its priority to its
// transaction on start.
TEST_F(URLRequestHttpJobTest, SetTransactionPriorityOnStart) {
  scoped_refptr<TestURLRequestHttpJob> job(
      new TestURLRequestHttpJob(req_.get()));
  job->SetPriority(LOW);

  EXPECT_FALSE(network_layer_.last_transaction());

  job->Start();

  ASSERT_TRUE(network_layer_.last_transaction());
  EXPECT_EQ(LOW, network_layer_.last_transaction()->priority());
}

// Make sure that URLRequestHttpJob passes on its priority updates to
// its transaction.
TEST_F(URLRequestHttpJobTest, SetTransactionPriority) {
  scoped_refptr<TestURLRequestHttpJob> job(
      new TestURLRequestHttpJob(req_.get()));
  job->SetPriority(LOW);
  job->Start();
  ASSERT_TRUE(network_layer_.last_transaction());
  EXPECT_EQ(LOW, network_layer_.last_transaction()->priority());

  job->SetPriority(HIGHEST);
  EXPECT_EQ(HIGHEST, network_layer_.last_transaction()->priority());
}

// Make sure that URLRequestHttpJob passes on its priority updates to
// newly-created transactions after the first one.
TEST_F(URLRequestHttpJobTest, SetSubsequentTransactionPriority) {
  scoped_refptr<TestURLRequestHttpJob> job(
      new TestURLRequestHttpJob(req_.get()));
  job->Start();

  job->SetPriority(LOW);
  ASSERT_TRUE(network_layer_.last_transaction());
  EXPECT_EQ(LOW, network_layer_.last_transaction()->priority());

  job->Kill();
  network_layer_.ClearLastTransaction();

  // Creates a second transaction.
  job->Start();
  ASSERT_TRUE(network_layer_.last_transaction());
  EXPECT_EQ(LOW, network_layer_.last_transaction()->priority());
}

// Confirm we do advertise SDCH encoding in the case of a GET.
TEST_F(URLRequestHttpJobTest, SdchAdvertisementGet) {
  EnableSdch();
  req_->set_method("GET");  // Redundant with default.
  scoped_refptr<TestURLRequestHttpJob> job(
      new TestURLRequestHttpJob(req_.get()));
  job->Start();
  EXPECT_TRUE(TransactionAcceptsSdchEncoding());
}

// Confirm we don't advertise SDCH encoding in the case of a POST.
TEST_F(URLRequestHttpJobTest, SdchAdvertisementPost) {
  EnableSdch();
  req_->set_method("POST");
  scoped_refptr<TestURLRequestHttpJob> job(
      new TestURLRequestHttpJob(req_.get()));
  job->Start();
  EXPECT_FALSE(TransactionAcceptsSdchEncoding());
}

// This base class just serves to set up some things before the TestURLRequest
// constructor is called.
class URLRequestHttpJobWebSocketTestBase : public ::testing::Test {
 protected:
  URLRequestHttpJobWebSocketTestBase() : socket_data_(nullptr, 0, nullptr, 0),
                                         context_(true) {
    // A Network Delegate is required for the WebSocketHandshakeStreamBase
    // object to be passed on to the HttpNetworkTransaction.
    context_.set_network_delegate(&network_delegate_);

    // Attempting to create real ClientSocketHandles is not going to work out so
    // well. Set up a fake socket factory.
    socket_factory_.AddSocketDataProvider(&socket_data_);
    context_.set_client_socket_factory(&socket_factory_);
    context_.Init();
  }

  StaticSocketDataProvider socket_data_;
  TestNetworkDelegate network_delegate_;
  MockClientSocketFactory socket_factory_;
  TestURLRequestContext context_;
};

class URLRequestHttpJobWebSocketTest
    : public URLRequestHttpJobWebSocketTestBase {
 protected:
  URLRequestHttpJobWebSocketTest()
      : req_(context_.CreateRequest(GURL("ws://www.example.com"),
                                    DEFAULT_PRIORITY,
                                    &delegate_)) {
    // The TestNetworkDelegate expects a call to NotifyBeforeURLRequest before
    // anything else happens.
    GURL url("ws://localhost/");
    TestCompletionCallback dummy;
    network_delegate_.NotifyBeforeURLRequest(
        req_.get(), dummy.callback(), &url);
  }

  TestDelegate delegate_;
  scoped_ptr<URLRequest> req_;
};

class MockCreateHelper : public WebSocketHandshakeStreamBase::CreateHelper {
 public:
  // GoogleMock does not appear to play nicely with move-only types like
  // scoped_ptr, so this forwarding method acts as a workaround.
  WebSocketHandshakeStreamBase* CreateBasicStream(
      scoped_ptr<ClientSocketHandle> connection,
      bool using_proxy) override {
    // Discard the arguments since we don't need them anyway.
    return CreateBasicStreamMock();
  }

  MOCK_METHOD0(CreateBasicStreamMock,
               WebSocketHandshakeStreamBase*());

  MOCK_METHOD2(CreateSpdyStream,
               WebSocketHandshakeStreamBase*(const base::WeakPtr<SpdySession>&,
                                             bool));
};

class FakeWebSocketHandshakeStream : public WebSocketHandshakeStreamBase {
 public:
  FakeWebSocketHandshakeStream() : initialize_stream_was_called_(false) {}

  bool initialize_stream_was_called() const {
    return initialize_stream_was_called_;
  }

  // Fake implementation of HttpStreamBase methods.
  int InitializeStream(const HttpRequestInfo* request_info,
                       RequestPriority priority,
                       const BoundNetLog& net_log,
                       const CompletionCallback& callback) override {
    initialize_stream_was_called_ = true;
    return ERR_IO_PENDING;
  }

  int SendRequest(const HttpRequestHeaders& request_headers,
                  HttpResponseInfo* response,
                  const CompletionCallback& callback) override {
    return ERR_IO_PENDING;
  }

  int ReadResponseHeaders(const CompletionCallback& callback) override {
    return ERR_IO_PENDING;
  }

  int ReadResponseBody(IOBuffer* buf,
                       int buf_len,
                       const CompletionCallback& callback) override {
    return ERR_IO_PENDING;
  }

  void Close(bool not_reusable) override {}

  bool IsResponseBodyComplete() const override { return false; }

  bool CanFindEndOfResponse() const override { return false; }

  bool IsConnectionReused() const override { return false; }
  void SetConnectionReused() override {}

  bool IsConnectionReusable() const override { return false; }

  int64 GetTotalReceivedBytes() const override { return 0; }

  bool GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const override {
    return false;
  }

  void GetSSLInfo(SSLInfo* ssl_info) override {}

  void GetSSLCertRequestInfo(SSLCertRequestInfo* cert_request_info) override {}

  bool IsSpdyHttpStream() const override { return false; }

  void Drain(HttpNetworkSession* session) override {}

  void SetPriority(RequestPriority priority) override {}

  UploadProgress GetUploadProgress() const override {
    return UploadProgress();
  }

  HttpStream* RenewStreamForAuth() override { return nullptr; }

  // Fake implementation of WebSocketHandshakeStreamBase method(s)
  scoped_ptr<WebSocketStream> Upgrade() override {
    return scoped_ptr<WebSocketStream>();
  }

 private:
  bool initialize_stream_was_called_;
};

TEST_F(URLRequestHttpJobWebSocketTest, RejectedWithoutCreateHelper) {
  scoped_refptr<TestURLRequestHttpJob> job(
      new TestURLRequestHttpJob(req_.get()));
  job->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(URLRequestStatus::FAILED, req_->status().status());
  EXPECT_EQ(ERR_DISALLOWED_URL_SCHEME, req_->status().error());
}

TEST_F(URLRequestHttpJobWebSocketTest, CreateHelperPassedThrough) {
  scoped_refptr<TestURLRequestHttpJob> job(
      new TestURLRequestHttpJob(req_.get()));
  scoped_ptr<MockCreateHelper> create_helper(
      new ::testing::StrictMock<MockCreateHelper>());
  FakeWebSocketHandshakeStream* fake_handshake_stream(
      new FakeWebSocketHandshakeStream);
  // Ownership of fake_handshake_stream is transferred when CreateBasicStream()
  // is called.
  EXPECT_CALL(*create_helper, CreateBasicStreamMock())
      .WillOnce(Return(fake_handshake_stream));
  req_->SetUserData(WebSocketHandshakeStreamBase::CreateHelper::DataKey(),
                    create_helper.release());
  req_->SetLoadFlags(LOAD_DISABLE_CACHE);
  job->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(URLRequestStatus::IO_PENDING, req_->status().status());
  EXPECT_TRUE(fake_handshake_stream->initialize_stream_was_called());
}

}  // namespace

}  // namespace net
