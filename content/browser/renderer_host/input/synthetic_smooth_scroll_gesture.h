// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_SMOOTH_SCROLL_GESTURE_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_SMOOTH_SCROLL_GESTURE_H_

#include "base/time/time.h"
#include "content/browser/renderer_host/input/synthetic_gesture.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target.h"
#include "content/browser/renderer_host/input/synthetic_web_input_event_builders.h"
#include "content/common/content_export.h"
#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace content {

class CONTENT_EXPORT SyntheticSmoothScrollGesture : public SyntheticGesture {
 public:
  explicit SyntheticSmoothScrollGesture(
      const SyntheticSmoothScrollGestureParams& params);
  virtual ~SyntheticSmoothScrollGesture();

  virtual SyntheticGesture::Result ForwardInputEvents(
      const base::TimeDelta& interval, SyntheticGestureTarget* target) OVERRIDE;

 private:
  SyntheticSmoothScrollGestureParams params_;
  float current_y_;
  SyntheticWebTouchEvent touch_event_;

  SyntheticGesture::Result ForwardTouchInputEvents(
      const base::TimeDelta& interval, SyntheticGestureTarget* target);
  SyntheticGesture::Result ForwardMouseInputEvents(
      const base::TimeDelta& interval, SyntheticGestureTarget* target);

  void ForwardTouchEvent(SyntheticGestureTarget* target);
  void ForwardMouseWheelEvent(SyntheticGestureTarget* target,
                              float delta);

  float GetPositionDelta(const base::TimeDelta& interval) const;
  float ComputeAbsoluteRemainingDistance() const;
  bool HasFinished() const;

  DISALLOW_COPY_AND_ASSIGN(SyntheticSmoothScrollGesture);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_SMOOTH_SCROLL_GESTURE_H_
