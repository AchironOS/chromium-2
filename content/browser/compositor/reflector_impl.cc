// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/reflector_impl.h"

#include "base/bind.h"
#include "base/location.h"
#include "content/browser/compositor/browser_compositor_output_surface.h"
#include "content/browser/compositor/owned_mailbox.h"
#include "content/common/gpu/client/gl_helper.h"
#include "ui/compositor/layer.h"

namespace content {

struct ReflectorImpl::LayerData {
  LayerData(ui::Layer* layer) : layer(layer) {}

  ui::Layer* layer;
  bool needs_set_mailbox = false;
};

ReflectorImpl::ReflectorImpl(ui::Compositor* mirrored_compositor,
                             ui::Layer* mirroring_layer)
    : mirrored_compositor_(mirrored_compositor),
      mirrored_compositor_gl_helper_texture_id_(0),
      flip_texture_(false),
      output_surface_(nullptr) {
  if (mirroring_layer)
    mirroring_layers_.push_back(new LayerData(mirroring_layer));
}

ReflectorImpl::~ReflectorImpl() {
}

void ReflectorImpl::Shutdown() {
  if (output_surface_)
    DetachFromOutputSurface();
  // Prevent the ReflectorImpl from picking up a new output surface.
  mirroring_layers_.clear();
}

void ReflectorImpl::DetachFromOutputSurface() {
  DCHECK(output_surface_);
  output_surface_->SetReflector(nullptr);
  DCHECK(mailbox_.get());
  mailbox_ = nullptr;
  output_surface_ = nullptr;
  mirrored_compositor_gl_helper_->DeleteTexture(
      mirrored_compositor_gl_helper_texture_id_);
  mirrored_compositor_gl_helper_texture_id_ = 0;
  mirrored_compositor_gl_helper_ = nullptr;
  for (LayerData* layer_data : mirroring_layers_)
    layer_data->layer->SetShowSolidColorContent();
}

void ReflectorImpl::OnSourceSurfaceReady(
    BrowserCompositorOutputSurface* output_surface) {
  if (mirroring_layers_.empty())
    return;  // Was already Shutdown().
  if (output_surface == output_surface_)
    return;  // Is already attached.
  if (output_surface_)
    DetachFromOutputSurface();

  // Use the GLHelper from the ImageTransportFactory for our OwnedMailbox so we
  // don't have to manage the lifetime of the GLHelper relative to the lifetime
  // of the mailbox.
  GLHelper* shared_helper = ImageTransportFactory::GetInstance()->GetGLHelper();
  mailbox_ = new OwnedMailbox(shared_helper);
  for (LayerData* layer_data : mirroring_layers_)
    layer_data->needs_set_mailbox = true;

  // Create a GLHelper attached to the mirrored compositor's output surface for
  // copying the output of the mirrored compositor.
  mirrored_compositor_gl_helper_.reset(
      new GLHelper(output_surface->context_provider()->ContextGL(),
                   output_surface->context_provider()->ContextSupport()));
  // Create a texture id in the name space of the new GLHelper to update the
  // mailbox being held by the |mirroring_layer_|.
  mirrored_compositor_gl_helper_texture_id_ =
      mirrored_compositor_gl_helper_->ConsumeMailboxToTexture(
          mailbox_->mailbox(), mailbox_->sync_point());

  flip_texture_ = !output_surface->capabilities().flipped_output_surface;

  // The texture doesn't have the data. Request full redraw on mirrored
  // compositor so that the full content will be copied to mirroring compositor.
  // This full redraw should land us in OnSourceSwapBuffers() to resize the
  // texture appropriately.
  mirrored_compositor_->ScheduleFullRedraw();

  output_surface_ = output_surface;
  output_surface_->SetReflector(this);
}

void ReflectorImpl::OnMirroringCompositorResized() {
  for (LayerData* layer_data : mirroring_layers_)
    layer_data->layer->SchedulePaint(layer_data->layer->bounds());
}

void ReflectorImpl::AddMirroringLayer(ui::Layer* layer) {
  DCHECK(mirroring_layers_.end() == FindLayerData(layer));
  LayerData* layer_data = new LayerData(layer);
  if (mailbox_)
    layer_data->needs_set_mailbox = true;
  mirroring_layers_.push_back(layer_data);
  mirrored_compositor_->ScheduleFullRedraw();
}

void ReflectorImpl::RemoveMirroringLayer(ui::Layer* layer) {
  ScopedVector<LayerData>::iterator iter = FindLayerData(layer);
  DCHECK(iter != mirroring_layers_.end());
  (*iter)->layer->SetShowSolidColorContent();
  mirroring_layers_.erase(iter);

  if (mirroring_layers_.empty() && output_surface_)
    DetachFromOutputSurface();
}

void ReflectorImpl::OnSourceSwapBuffers() {
  if (mirroring_layers_.empty())
    return;
  // Should be attached to the source output surface already.
  DCHECK(mailbox_.get());

  gfx::Size size = output_surface_->SurfaceSize();
  mirrored_compositor_gl_helper_->CopyTextureFullImage(
      mirrored_compositor_gl_helper_texture_id_, size);
  // Insert a barrier to make the copy show up in the mirroring compositor's
  // mailbox. Since the the compositor contexts and the ImageTransportFactory's
  // GLHelper are all on the same GPU channel, this is sufficient instead of
  // plumbing through a sync point.
  mirrored_compositor_gl_helper_->InsertOrderingBarrier();

  // Request full redraw on mirroring compositor.
  for (LayerData* layer_data : mirroring_layers_)
    UpdateTexture(layer_data, size, layer_data->layer->bounds());
}

void ReflectorImpl::OnSourcePostSubBuffer(const gfx::Rect& rect) {
  if (mirroring_layers_.empty())
    return;
  // Should be attached to the source output surface already.
  DCHECK(mailbox_.get());

  gfx::Size size = output_surface_->SurfaceSize();
  mirrored_compositor_gl_helper_->CopyTextureSubImage(
      mirrored_compositor_gl_helper_texture_id_, rect);
  // Insert a barrier to make the copy show up in the mirroring compositor's
  // mailbox. Since the the compositor contexts and the ImageTransportFactory's
  // GLHelper are all on the same GPU channel, this is sufficient instead of
  // plumbing through a sync point.
  mirrored_compositor_gl_helper_->InsertOrderingBarrier();

  int y = rect.y();
  // Flip the coordinates to compositor's one.
  if (flip_texture_)
    y = size.height() - rect.y() - rect.height();
  gfx::Rect mirroring_rect(rect.x(), y, rect.width(), rect.height());

  // Request redraw of the dirty portion in mirroring compositor.
  for (LayerData* layer_data : mirroring_layers_)
    UpdateTexture(layer_data, size, mirroring_rect);
}

static void ReleaseMailbox(scoped_refptr<OwnedMailbox> mailbox,
                           unsigned int sync_point,
                           bool is_lost) {
  mailbox->UpdateSyncPoint(sync_point);
}

ScopedVector<ReflectorImpl::LayerData>::iterator ReflectorImpl::FindLayerData(
    ui::Layer* layer) {
  return std::find_if(mirroring_layers_.begin(), mirroring_layers_.end(),
                      [layer](const LayerData* layer_data) {
                        return layer_data->layer == layer;
                      });
}

void ReflectorImpl::UpdateTexture(ReflectorImpl::LayerData* layer_data,
                                  const gfx::Size& source_size,
                                  const gfx::Rect& redraw_rect) {
  if (layer_data->needs_set_mailbox) {
    layer_data->layer->SetTextureMailbox(
        cc::TextureMailbox(mailbox_->holder()),
        cc::SingleReleaseCallback::Create(base::Bind(ReleaseMailbox, mailbox_)),
        source_size);
    layer_data->needs_set_mailbox = false;
  } else {
    layer_data->layer->SetTextureSize(source_size);
  }
  layer_data->layer->SetBounds(gfx::Rect(source_size));
  layer_data->layer->SetTextureFlipped(flip_texture_);
  layer_data->layer->SchedulePaint(redraw_rect);
}

}  // namespace content
