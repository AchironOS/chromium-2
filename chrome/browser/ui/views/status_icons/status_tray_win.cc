// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/status_icons/status_tray_win.h"

#include <commctrl.h>

#include "base/bind.h"
#include "base/profiler/scoped_tracker.h"
#include "base/threading/non_thread_safe.h"
#include "base/threading/thread.h"
#include "base/win/wrapped_window_proc.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/views/status_icons/status_icon_win.h"
#include "chrome/browser/ui/views/status_icons/status_tray_state_changer_win.h"
#include "chrome/common/chrome_constants.h"
#include "components/browser_watcher/exit_funnel_win.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/win/hwnd_util.h"

static const UINT kStatusIconMessage = WM_APP + 1;

namespace {
// |kBaseIconId| is 2 to avoid conflicts with plugins that hard-code id 1.
const UINT kBaseIconId = 2;

UINT ReservedIconId(StatusTray::StatusIconType type) {
  return kBaseIconId + static_cast<UINT>(type);
}

// See http://crbug.com/412384.
void TraceSessionEnding(LPARAM lparam) {
  browser_watcher::ExitFunnel funnel;
  if (!funnel.Init(chrome::kBrowserExitCodesRegistryPath,
                   base::GetCurrentProcessHandle())) {
    return;
  }

  // This exit path is the prime suspect for most our unclean shutdowns.
  // Trace all the possible options to WM_ENDSESSION. This may result in
  // multiple events for a single shutdown, but that's fine.
  funnel.RecordEvent(L"TraybarEndSession");

  if (lparam & ENDSESSION_CLOSEAPP)
    funnel.RecordEvent(L"ES_CloseApp");
  if (lparam & ENDSESSION_CRITICAL)
    funnel.RecordEvent(L"ES_Critical");
  if (lparam & ENDSESSION_LOGOFF)
    funnel.RecordEvent(L"ES_Logoff");
  const LPARAM kKnownBits =
      ENDSESSION_CLOSEAPP | ENDSESSION_CRITICAL | ENDSESSION_LOGOFF;
  if (lparam & ~kKnownBits)
    funnel.RecordEvent(L"ES_Other");
}

}  // namespace

// Default implementation for StatusTrayStateChanger that communicates to
// Exporer.exe via COM.  It spawns a background thread with a fresh COM
// apartment and requests that the visibility be increased unless the user
// has explicitly set the icon to be hidden.
class StatusTrayStateChangerProxyImpl : public StatusTrayStateChangerProxy,
                                        public base::NonThreadSafe {
 public:
  StatusTrayStateChangerProxyImpl()
      : pending_requests_(0),
        worker_thread_("StatusIconCOMWorkerThread"),
        weak_factory_(this) {
    worker_thread_.init_com_with_mta(false);
  }

  void EnqueueChange(UINT icon_id, HWND window) override {
    DCHECK(CalledOnValidThread());
    if (pending_requests_ == 0)
      worker_thread_.Start();

    ++pending_requests_;
    worker_thread_.message_loop_proxy()->PostTaskAndReply(
        FROM_HERE,
        base::Bind(
            &StatusTrayStateChangerProxyImpl::EnqueueChangeOnWorkerThread,
            icon_id,
            window),
        base::Bind(&StatusTrayStateChangerProxyImpl::ChangeDone,
                   weak_factory_.GetWeakPtr()));
  }

 private:
  // Must be called only on |worker_thread_|, to ensure the correct COM
  // apartment.
  static void EnqueueChangeOnWorkerThread(UINT icon_id, HWND window) {
    // It appears that IUnknowns are coincidentally compatible with
    // scoped_refptr.  Normally I wouldn't depend on that but it seems that
    // base::win::IUnknownImpl itself depends on that coincidence so it's
    // already being assumed elsewhere.
    scoped_refptr<StatusTrayStateChangerWin> status_tray_state_changer(
        new StatusTrayStateChangerWin(icon_id, window));
    status_tray_state_changer->EnsureTrayIconVisible();
  }

  // Called on UI thread.
  void ChangeDone() {
    DCHECK(CalledOnValidThread());
    DCHECK_GT(pending_requests_, 0);

    if (--pending_requests_ == 0)
      worker_thread_.Stop();
  }

 private:
  int pending_requests_;
  base::Thread worker_thread_;
  base::WeakPtrFactory<StatusTrayStateChangerProxyImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(StatusTrayStateChangerProxyImpl);
};

StatusTrayWin::StatusTrayWin()
    : next_icon_id_(1),
      atom_(0),
      instance_(NULL),
      window_(NULL) {
  // Register our window class
  WNDCLASSEX window_class;
  base::win::InitializeWindowClass(
      chrome::kStatusTrayWindowClass,
      &base::win::WrappedWindowProc<StatusTrayWin::WndProcStatic>,
      0, 0, 0, NULL, NULL, NULL, NULL, NULL,
      &window_class);
  instance_ = window_class.hInstance;
  atom_ = RegisterClassEx(&window_class);
  CHECK(atom_);

  // If the taskbar is re-created after we start up, we have to rebuild all of
  // our icons.
  taskbar_created_message_ = RegisterWindowMessage(TEXT("TaskbarCreated"));

  // Create an offscreen window for handling messages for the status icons. We
  // create a hidden WS_POPUP window instead of an HWND_MESSAGE window, because
  // only top-level windows such as popups can receive broadcast messages like
  // "TaskbarCreated".
  window_ = CreateWindow(MAKEINTATOM(atom_),
                         0, WS_POPUP, 0, 0, 0, 0, 0, 0, instance_, 0);
  gfx::CheckWindowCreated(window_);
  gfx::SetWindowUserData(window_, this);
}

StatusTrayWin::~StatusTrayWin() {
  if (window_)
    DestroyWindow(window_);

  if (atom_)
    UnregisterClass(MAKEINTATOM(atom_), instance_);
}

void StatusTrayWin::UpdateIconVisibilityInBackground(
    StatusIconWin* status_icon) {
  if (!state_changer_proxy_.get())
    state_changer_proxy_.reset(new StatusTrayStateChangerProxyImpl);

  state_changer_proxy_->EnqueueChange(status_icon->icon_id(),
                                      status_icon->window());
}

LRESULT CALLBACK StatusTrayWin::WndProcStatic(HWND hwnd,
                                              UINT message,
                                              WPARAM wparam,
                                              LPARAM lparam) {
  StatusTrayWin* msg_wnd = reinterpret_cast<StatusTrayWin*>(
      GetWindowLongPtr(hwnd, GWLP_USERDATA));
  if (msg_wnd)
    return msg_wnd->WndProc(hwnd, message, wparam, lparam);
  else
    return ::DefWindowProc(hwnd, message, wparam, lparam);
}

LRESULT CALLBACK StatusTrayWin::WndProc(HWND hwnd,
                                        UINT message,
                                        WPARAM wparam,
                                        LPARAM lparam) {
  if (message == taskbar_created_message_) {
    // We need to reset all of our icons because the taskbar went away.
    for (StatusIcons::const_iterator i(status_icons().begin());
         i != status_icons().end(); ++i) {
      StatusIconWin* win_icon = static_cast<StatusIconWin*>(*i);
      win_icon->ResetIcon();
    }
    return TRUE;
  } else if (message == kStatusIconMessage) {
    StatusIconWin* win_icon = NULL;

    // Find the selected status icon.
    for (StatusIcons::const_iterator i(status_icons().begin());
         i != status_icons().end();
         ++i) {
      StatusIconWin* current_win_icon = static_cast<StatusIconWin*>(*i);
      if (current_win_icon->icon_id() == wparam) {
        win_icon = current_win_icon;
        break;
      }
    }

    // It is possible for this procedure to be called with an obsolete icon
    // id.  In that case we should just return early before handling any
    // actions.
    if (!win_icon)
      return TRUE;

    switch (lparam) {
      case TB_INDETERMINATE:
        win_icon->HandleBalloonClickEvent();
        return TRUE;

      case WM_LBUTTONDOWN:
      case WM_RBUTTONDOWN:
      case WM_CONTEXTMENU:
        // Walk our icons, find which one was clicked on, and invoke its
        // HandleClickEvent() method.
        gfx::Point cursor_pos(
            gfx::Screen::GetNativeScreen()->GetCursorScreenPoint());
        win_icon->HandleClickEvent(cursor_pos, lparam == WM_LBUTTONDOWN);
        return TRUE;
    }
  } else if (message == WM_ENDSESSION) {
    // If Chrome is in background-only mode, this is the only notification
    // it gets that Windows is exiting. Make sure we shutdown in an orderly
    // fashion.
    TraceSessionEnding(lparam);
    chrome::SessionEnding();
  }
  return ::DefWindowProc(hwnd, message, wparam, lparam);
}

StatusIcon* StatusTrayWin::CreatePlatformStatusIcon(
    StatusTray::StatusIconType type,
    const gfx::ImageSkia& image,
    const base::string16& tool_tip) {
  UINT next_icon_id;
  if (type == StatusTray::OTHER_ICON)
    next_icon_id = NextIconId();
  else
    next_icon_id = ReservedIconId(type);

  StatusIcon* icon =
      new StatusIconWin(this, next_icon_id, window_, kStatusIconMessage);

  icon->SetImage(image);
  icon->SetToolTip(tool_tip);
  return icon;
}

UINT StatusTrayWin::NextIconId() {
  UINT icon_id = next_icon_id_++;
  return kBaseIconId + static_cast<UINT>(NAMED_STATUS_ICON_COUNT) + icon_id;
}

void StatusTrayWin::SetStatusTrayStateChangerProxyForTest(
    scoped_ptr<StatusTrayStateChangerProxy> proxy) {
  state_changer_proxy_ = proxy.Pass();
}

StatusTray* StatusTray::Create() {
  return new StatusTrayWin();
}
