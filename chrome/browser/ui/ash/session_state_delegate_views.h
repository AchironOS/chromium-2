// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SESSION_STATE_DELEGATE_VIEWS_H_
#define CHROME_BROWSER_UI_ASH_SESSION_STATE_DELEGATE_VIEWS_H_

#include "ash/session/session_state_delegate.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {
class SessionStateObserver;
}  // namespace ash

class SessionStateDelegate : public ash::SessionStateDelegate {
 public:
  SessionStateDelegate();
  ~SessionStateDelegate() override;

  // ash::SessionStateDelegate:
  content::BrowserContext* GetBrowserContextByIndex(
      ash::MultiProfileIndex index) override;
  content::BrowserContext* GetBrowserContextForWindow(
      aura::Window* window) override;
  content::BrowserContext* GetUserPresentingBrowserContextForWindow(
      aura::Window* window) override;
  int GetMaximumNumberOfLoggedInUsers() const override;
  int NumberOfLoggedInUsers() const override;
  bool IsActiveUserSessionStarted() const override;
  bool CanLockScreen() const override;
  bool IsScreenLocked() const override;
  bool ShouldLockScreenBeforeSuspending() const override;
  void LockScreen() override;
  void UnlockScreen() override;
  bool IsUserSessionBlocked() const override;
  SessionState GetSessionState() const override;
  const user_manager::UserInfo* GetUserInfo(
      ash::MultiProfileIndex index) const override;
  const user_manager::UserInfo* GetUserInfo(
      content::BrowserContext* context) const override;
  bool ShouldShowAvatar(aura::Window* window) const override;
  void SwitchActiveUser(const std::string& user_id) override;
  void CycleActiveUser(CycleUser cycle_user) override;
  bool IsMultiProfileAllowedByPrimaryUserPolicy() const override;
  void AddSessionStateObserver(ash::SessionStateObserver* observer) override;
  void RemoveSessionStateObserver(ash::SessionStateObserver* observer) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionStateDelegate);
};

#endif  // CHROME_BROWSER_UI_ASH_SESSION_STATE_DELEGATE_VIEWS_H_
