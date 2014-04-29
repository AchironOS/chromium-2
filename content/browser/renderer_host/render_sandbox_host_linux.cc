// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_sandbox_host_linux.h"

#include <sys/socket.h>

#include "base/memory/singleton.h"
#include "base/posix/eintr_wrapper.h"
#include "content/browser/renderer_host/sandbox_ipc_linux.h"

namespace content {

// Runs on the main thread at startup.
RenderSandboxHostLinux::RenderSandboxHostLinux()
    : initialized_(false),
      renderer_socket_(0),
      childs_lifeline_fd_(0),
      pid_(0) {
}

// static
RenderSandboxHostLinux* RenderSandboxHostLinux::GetInstance() {
  return Singleton<RenderSandboxHostLinux>::get();
}

void RenderSandboxHostLinux::Init(const std::string& sandbox_path) {
  DCHECK(!initialized_);
  initialized_ = true;

  int fds[2];
  // We use SOCK_SEQPACKET rather than SOCK_DGRAM to prevent the renderer from
  // sending datagrams to other sockets on the system. The sandbox may prevent
  // the renderer from calling socket() to create new sockets, but it'll still
  // inherit some sockets. With AF_UNIX+SOCK_DGRAM, it can call sendmsg to send
  // a datagram to any (abstract) socket on the same system. With
  // SOCK_SEQPACKET, this is prevented.
  CHECK(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds) == 0);

  renderer_socket_ = fds[0];
  const int browser_socket = fds[1];

  int pipefds[2];
  CHECK(0 == pipe(pipefds));
  const int child_lifeline_fd = pipefds[0];
  childs_lifeline_fd_ = pipefds[1];

  // We need to be monothreaded before we fork().
#if !defined(THREAD_SANITIZER)
  DCHECK_EQ(1, base::GetNumberOfThreads(base::GetCurrentProcessHandle()));
#endif  // !defined(THREAD_SANITIZER)
  pid_ = fork();
  if (pid_ == 0) {
    if (IGNORE_EINTR(close(fds[0])) < 0)
      DPLOG(ERROR) << "close";
    if (IGNORE_EINTR(close(pipefds[1])) < 0)
      DPLOG(ERROR) << "close";

    SandboxIPCProcess handler(child_lifeline_fd, browser_socket, sandbox_path);
    handler.Run();
    _exit(0);
  }
}

RenderSandboxHostLinux::~RenderSandboxHostLinux() {
  if (initialized_) {
    if (IGNORE_EINTR(close(renderer_socket_)) < 0)
      PLOG(ERROR) << "close";
    if (IGNORE_EINTR(close(childs_lifeline_fd_)) < 0)
      PLOG(ERROR) << "close";
  }
}

}  // namespace content
