// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CRYPTO_RENDER_CDM_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_CRYPTO_RENDER_CDM_FACTORY_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "content/public/renderer/render_frame_observer.h"
#include "media/base/cdm_factory.h"
#include "media/base/media_keys.h"

#if defined(ENABLE_PEPPER_CDMS)
#include "content/renderer/media/crypto/pepper_cdm_wrapper.h"
#endif

class GURL;

namespace content {

#if defined(ENABLE_BROWSER_CDMS)
class RendererCdmManager;
#endif

// CdmFactory implementation in content/renderer. This class is not thread safe
// and should only be used on one thread.
class RenderCdmFactory : public media::CdmFactory, public RenderFrameObserver {
 public:
  RenderCdmFactory(
#if defined(ENABLE_PEPPER_CDMS)
      const CreatePepperCdmCB& create_pepper_cdm_cb,
#elif defined(ENABLE_BROWSER_CDMS)
      RendererCdmManager* manager,
#endif  // defined(ENABLE_PEPPER_CDMS)
      RenderFrame* render_frame);

  ~RenderCdmFactory() override;

  // CdmFactory implementation.
  void Create(
      const std::string& key_system,
      bool allow_distinctive_identifier,
      bool allow_persistent_state,
      const GURL& security_origin,
      const media::SessionMessageCB& session_message_cb,
      const media::SessionClosedCB& session_closed_cb,
      const media::LegacySessionErrorCB& legacy_session_error_cb,
      const media::SessionKeysChangeCB& session_keys_change_cb,
      const media::SessionExpirationUpdateCB& session_expiration_update_cb,
      const CdmCreatedCB& cdm_created_cb) override;

 private:
#if defined(ENABLE_PEPPER_CDMS)
  CreatePepperCdmCB create_pepper_cdm_cb_;
#elif defined(ENABLE_BROWSER_CDMS)
  // The |manager_| is a per render frame object owned by RenderFrameImpl.
  RendererCdmManager* manager_;
#endif

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(RenderCdmFactory);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_CRYPTO_RENDER_CDM_FACTORY_H_
