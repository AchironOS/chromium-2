// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/ui/login/login_prompt.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

class ProxyAuthDialogWaiter : public content::WindowedNotificationObserver {
 public:
  ProxyAuthDialogWaiter()
      : WindowedNotificationObserver(
            chrome::NOTIFICATION_AUTH_NEEDED,
            base::Bind(&ProxyAuthDialogWaiter::SetLoginHandler,
                       base::Unretained(this))),
        login_handler_(nullptr) {}

  ~ProxyAuthDialogWaiter() override {}

  LoginHandler* login_handler() const { return login_handler_; }

 private:
  bool SetLoginHandler(const content::NotificationSource& /* source */,
                       const content::NotificationDetails& details) {
    login_handler_ =
        content::Details<LoginNotificationDetails>(details)->handler();
    return true;
  }

  LoginHandler* login_handler_;

  DISALLOW_COPY_AND_ASSIGN(ProxyAuthDialogWaiter);
};

}  // namespace

// Boolean parameter is used to run this test for webview (true) and for
// iframe (false) GAIA sign in.
class ProxyAuthOnUserBoardScreenTest
    : public LoginManagerTest,
      public testing::WithParamInterface<bool> {
 public:
  ProxyAuthOnUserBoardScreenTest()
      : LoginManagerTest(true /* should_launch_browser */),
        proxy_server_(net::SpawnedTestServer::TYPE_BASIC_AUTH_PROXY,
                      net::SpawnedTestServer::kLocalhost,
                      base::FilePath()) {
    set_use_webview(GetParam());
  }

  ~ProxyAuthOnUserBoardScreenTest() override {}

  void SetUp() override {
    ASSERT_TRUE(proxy_server_.Start());
    LoginManagerTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginManagerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(::switches::kProxyServer,
                                    proxy_server_.host_port_pair().ToString());
  }

 private:
  net::SpawnedTestServer proxy_server_;

  DISALLOW_COPY_AND_ASSIGN(ProxyAuthOnUserBoardScreenTest);
};

IN_PROC_BROWSER_TEST_P(ProxyAuthOnUserBoardScreenTest,
                       PRE_ProxyAuthDialogOnUserBoardScreen) {
  RegisterUser("test-user@gmail.com");
  StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_P(ProxyAuthOnUserBoardScreenTest,
                       ProxyAuthDialogOnUserBoardScreen) {
  LoginDisplayHost* login_display_host = LoginDisplayHostImpl::default_host();
  WebUILoginView* web_ui_login_view = login_display_host->GetWebUILoginView();
  OobeUI* oobe_ui =
      static_cast<OobeUI*>(web_ui_login_view->GetWebUI()->GetController());

  {
    OobeScreenWaiter screen_waiter(OobeDisplay::SCREEN_ACCOUNT_PICKER);
    ProxyAuthDialogWaiter auth_dialog_waiter;
    screen_waiter.Wait();
    auth_dialog_waiter.Wait();

    ASSERT_TRUE(auth_dialog_waiter.login_handler());
    auth_dialog_waiter.login_handler()->CancelAuth();
  }

  {
    OobeScreenWaiter screen_waiter(OobeDisplay::SCREEN_GAIA_SIGNIN);
    ProxyAuthDialogWaiter auth_dialog_waiter;
    ASSERT_TRUE(content::ExecuteScript(oobe_ui->web_ui()->GetWebContents(),
                                       "window.domAutomationController.send(!!("
                                       "$('add-user-button').click()"
                                       "));"));
    screen_waiter.Wait();
    auth_dialog_waiter.Wait();
    ASSERT_TRUE(auth_dialog_waiter.login_handler());
  }
}

INSTANTIATE_TEST_CASE_P(ProxyAuthOnUserBoardScreenTestSuite,
                        ProxyAuthOnUserBoardScreenTest,
// Times out under MSan: https://crbug.com/481651
#if defined(MEMORY_SANITIZER)
                        testing::Values(false));
#else
                        testing::Bool());
#endif

}  // namespace chromeos
