// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_browser_main_parts.h"

#if !defined(OS_ANDROID)
#include <signal.h>
#include <sys/prctl.h>
#endif

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/run_loop.h"
#include "cc/base/switches.h"
#include "chromecast/base/cast_paths.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/base/metrics/grouped_histogram.h"
#include "chromecast/browser/cast_browser_context.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/devtools/remote_debugging_server.h"
#include "chromecast/browser/metrics/cast_metrics_prefs.h"
#include "chromecast/browser/metrics/cast_metrics_service_client.h"
#include "chromecast/browser/pref_service_helper.h"
#include "chromecast/browser/service/cast_service.h"
#include "chromecast/browser/url_request_context_factory.h"
#include "chromecast/common/chromecast_switches.h"
#include "chromecast/common/platform_client_auth.h"
#include "chromecast/media/base/key_systems_common.h"
#include "chromecast/net/connectivity_checker.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "media/base/browser_cdm_factory.h"
#include "media/base/media_switches.h"

#if defined(OS_ANDROID)
#include "chromecast/browser/media/cast_media_client_android.h"
#include "chromecast/crash/android/crash_handler.h"
#include "components/crash/browser/crash_dump_manager_android.h"
#include "media/base/android/media_client_android.h"
#include "net/android/network_change_notifier_factory_android.h"
#else
#include "chromecast/browser/media/cast_browser_cdm_factory.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/test/test_screen.h"
#include "ui/gfx/screen.h"
#endif

namespace {

#if !defined(OS_ANDROID)
int kSignalsToRunClosure[] = { SIGTERM, SIGINT, };

// Closure to run on SIGTERM and SIGINT.
base::Closure* g_signal_closure = NULL;

void RunClosureOnSignal(int signum) {
  LOG(ERROR) << "Got signal " << signum;
  DCHECK(g_signal_closure);
  // Expect main thread got this signal. Otherwise, weak_ptr of run_loop will
  // crash the process.
  g_signal_closure->Run();
}

void RegisterClosureOnSignal(const base::Closure& closure) {
  DCHECK(!g_signal_closure);
  DCHECK_GT(arraysize(kSignalsToRunClosure), 0U);

  // Allow memory leak by intention.
  g_signal_closure = new base::Closure(closure);

  struct sigaction sa_new;
  memset(&sa_new, 0, sizeof(sa_new));
  sa_new.sa_handler = RunClosureOnSignal;
  sigfillset(&sa_new.sa_mask);
  sa_new.sa_flags = SA_RESTART;

  for (size_t i = 0; i < arraysize(kSignalsToRunClosure); i++) {
    struct sigaction sa_old;
    if (sigaction(kSignalsToRunClosure[i], &sa_new, &sa_old) == -1) {
      NOTREACHED();
    } else {
      DCHECK_EQ(sa_old.sa_handler, SIG_DFL);
    }
  }

  // Get the first signal to exit when the parent process dies.
  prctl(PR_SET_PDEATHSIG, kSignalsToRunClosure[0]);
}

const int kKillOnAlarmTimeoutSec = 5;  // 5 seconds

void KillOnAlarm(int signum) {
  LOG(ERROR) << "Got alarm signal for termination: " << signum;
  raise(SIGKILL);
}

void RegisterKillOnAlarm(int timeout_seconds) {
  struct sigaction sa_new;
  memset(&sa_new, 0, sizeof(sa_new));
  sa_new.sa_handler = KillOnAlarm;
  sigfillset(&sa_new.sa_mask);
  sa_new.sa_flags = SA_RESTART;

  struct sigaction sa_old;
  if (sigaction(SIGALRM, &sa_new, &sa_old) == -1) {
    NOTREACHED();
  } else {
    DCHECK_EQ(sa_old.sa_handler, SIG_DFL);
  }

  if (alarm(timeout_seconds) > 0)
    NOTREACHED() << "Previous alarm() was cancelled";
}

void DeregisterKillOnAlarm() {
  // Explicitly cancel any outstanding alarm() calls.
  alarm(0);

  struct sigaction sa_new;
  memset(&sa_new, 0, sizeof(sa_new));
  sa_new.sa_handler = SIG_DFL;
  sigfillset(&sa_new.sa_mask);
  sa_new.sa_flags = SA_RESTART;

  struct sigaction sa_old;
  if (sigaction(SIGALRM, &sa_new, &sa_old) == -1) {
    NOTREACHED();
  } else {
    DCHECK_EQ(sa_old.sa_handler, KillOnAlarm);
  }
}
#endif  // !defined(OS_ANDROID)

}  // namespace

namespace chromecast {
namespace shell {

namespace {

struct DefaultCommandLineSwitch {
  const char* const switch_name;
  const char* const switch_value;
};

DefaultCommandLineSwitch g_default_switches[] = {
#if defined(OS_ANDROID)
  // Disables Chromecast-specific WiFi-related features on ATV for now.
  { switches::kNoWifi, "" },
  { switches::kEnableOverlayFullscreenVideo, ""},
  { switches::kDisableInfobarForProtectedMediaIdentifier, ""},
  { switches::kDisableGestureRequirementForMediaPlayback, ""},
#endif
  // Always enable HTMLMediaElement logs.
  { switches::kBlinkPlatformLogChannels, "Media"},
#if defined(DISABLE_DISPLAY)
  { switches::kDisableGpu, "" },
#endif
#if defined(OS_LINUX)
#if defined(ARCH_CPU_X86_FAMILY)
  // This is needed for now to enable the egltest Ozone platform to work with
  // current Linux/NVidia OpenGL drivers.
  { switches::kIgnoreGpuBlacklist, ""},
#elif defined(ARCH_CPU_ARM_FAMILY) && !defined(DISABLE_DISPLAY)
  // On Linux arm, enable CMA pipeline by default.
  { switches::kEnableCmaMediaPipeline, "" },
#endif
#endif  // defined(OS_LINUX)
  // Needed to fix a bug where the raster thread doesn't get scheduled for a
  // substantial time (~5 seconds).  See https://crbug.com/441895.
  { switches::kUseNormalPriorityForTileTaskWorkerThreads, "" },
  { NULL, NULL },  // Termination
};

void AddDefaultCommandLineSwitches(base::CommandLine* command_line) {
  int i = 0;
  while (g_default_switches[i].switch_name != NULL) {
    command_line->AppendSwitchASCII(
        std::string(g_default_switches[i].switch_name),
        std::string(g_default_switches[i].switch_value));
    ++i;
  }
}

}  // namespace

CastBrowserMainParts::CastBrowserMainParts(
    const content::MainFunctionParams& parameters,
    URLRequestContextFactory* url_request_context_factory)
    : BrowserMainParts(),
      cast_browser_process_(new CastBrowserProcess()),
      parameters_(parameters),
      url_request_context_factory_(url_request_context_factory) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  AddDefaultCommandLineSwitches(command_line);
}

CastBrowserMainParts::~CastBrowserMainParts() {
}

void CastBrowserMainParts::PreMainMessageLoopStart() {
  // GroupedHistograms needs to be initialized before any threads are created
  // to prevent race conditions between calls to Preregister and those threads
  // attempting to collect metrics.
  // This call must also be before NetworkChangeNotifier, as it generates
  // Net/DNS metrics.
  metrics::PreregisterAllGroupedHistograms();

#if defined(OS_ANDROID)
  net::NetworkChangeNotifier::SetFactory(
      new net::NetworkChangeNotifierFactoryAndroid());
#endif  // defined(OS_ANDROID)
}

void CastBrowserMainParts::PostMainMessageLoopStart() {
  cast_browser_process_->SetMetricsHelper(make_scoped_ptr(
      new metrics::CastMetricsHelper(base::MessageLoopProxy::current())));

#if defined(OS_ANDROID)
  base::MessageLoopForUI::current()->Start();
#endif  // defined(OS_ANDROID)
}

int CastBrowserMainParts::PreCreateThreads() {
#if defined(OS_ANDROID)
  // GPU process is started immediately after threads are created, requiring
  // CrashDumpManager to be initialized beforehand.
  base::FilePath crash_dumps_dir;
  if (!chromecast::CrashHandler::GetCrashDumpLocation(&crash_dumps_dir)) {
    LOG(ERROR) << "Could not find crash dump location.";
  }
  cast_browser_process_->SetCrashDumpManager(
      make_scoped_ptr(new breakpad::CrashDumpManager(crash_dumps_dir)));
#else
  base::FilePath home_dir;
  CHECK(PathService::Get(DIR_CAST_HOME, &home_dir));
  if (!base::CreateDirectory(home_dir))
    return 1;
#endif

#if defined(USE_AURA)
  // Screen can (and should) exist even with no displays connected. Its presence
  // is assumed as an interface to access display information, e.g. from metrics
  // code.  See CastContentWindow::CreateWindowTree for update when resolution
  // is available.
  DCHECK(!gfx::Screen::GetScreenByType(gfx::SCREEN_TYPE_NATIVE));
  gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE,
                                 aura::TestScreen::Create(gfx::Size(0, 0)));
#endif
  return 0;
}

void CastBrowserMainParts::PreMainMessageLoopRun() {
  scoped_refptr<PrefRegistrySimple> pref_registry(new PrefRegistrySimple());
  metrics::RegisterPrefs(pref_registry.get());
  cast_browser_process_->SetPrefService(
      PrefServiceHelper::CreatePrefService(pref_registry.get()));

#if defined(OS_ANDROID)
  ::media::SetMediaClientAndroid(new media::CastMediaClientAndroid());
#else
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kEnableCmaMediaPipeline))
    ::media::SetBrowserCdmFactory(new media::CastBrowserCdmFactory());
#endif  // defined(OS_ANDROID)

  cast_browser_process_->SetConnectivityChecker(
      make_scoped_refptr(new ConnectivityChecker(
          content::BrowserThread::GetMessageLoopProxyForThread(
              content::BrowserThread::IO))));

  url_request_context_factory_->InitializeOnUIThread();

  cast_browser_process_->SetBrowserContext(
      make_scoped_ptr(new CastBrowserContext(url_request_context_factory_)));
  cast_browser_process_->SetMetricsServiceClient(
      metrics::CastMetricsServiceClient::Create(
          content::BrowserThread::GetBlockingPool(),
          cast_browser_process_->pref_service(),
          cast_browser_process_->browser_context()->GetRequestContext()));

  if (!PlatformClientAuth::Initialize())
    LOG(ERROR) << "PlatformClientAuth::Initialize failed.";

  cast_browser_process_->SetRemoteDebuggingServer(
      make_scoped_ptr(new RemoteDebuggingServer()));

  cast_browser_process_->SetCastService(CastService::Create(
      cast_browser_process_->browser_context(),
      cast_browser_process_->pref_service(),
      cast_browser_process_->metrics_service_client(),
      url_request_context_factory_->GetSystemGetter()));
  cast_browser_process_->cast_service()->Initialize();

  // Initializing metrics service and network delegates must happen after cast
  // service is intialized because CastMetricsServiceClient and
  // CastNetworkDelegate may use components initialized by cast service.
  cast_browser_process_->metrics_service_client()
      ->Initialize(cast_browser_process_->cast_service());
  url_request_context_factory_->InitializeNetworkDelegates();

  cast_browser_process_->cast_service()->Start();
}

bool CastBrowserMainParts::MainMessageLoopRun(int* result_code) {
#if defined(OS_ANDROID)
  // Android does not use native main MessageLoop.
  NOTREACHED();
  return true;
#else
  base::RunLoop run_loop;
  base::Closure quit_closure(run_loop.QuitClosure());
  RegisterClosureOnSignal(quit_closure);

  // If parameters_.ui_task is not NULL, we are running browser tests.
  if (parameters_.ui_task) {
    base::MessageLoop* message_loop = base::MessageLoopForUI::current();
    message_loop->PostTask(FROM_HERE, *parameters_.ui_task);
    message_loop->PostTask(FROM_HERE, quit_closure);
  }

  run_loop.Run();

  // Once the main loop has stopped running, we give the browser process a few
  // seconds to stop cast service and finalize all resources. If a hang occurs
  // and cast services refuse to terminate successfully, then we SIGKILL the
  // current process to avoid indefinte hangs.
  RegisterKillOnAlarm(kKillOnAlarmTimeoutSec);

  cast_browser_process_->cast_service()->Stop();
  return true;
#endif
}

void CastBrowserMainParts::PostMainMessageLoopRun() {
#if defined(OS_ANDROID)
  // Android does not use native main MessageLoop.
  NOTREACHED();
#else
  cast_browser_process_->cast_service()->Finalize();
  cast_browser_process_->metrics_service_client()->Finalize();
  cast_browser_process_.reset();
  DeregisterKillOnAlarm();
#endif
}

}  // namespace shell
}  // namespace chromecast
