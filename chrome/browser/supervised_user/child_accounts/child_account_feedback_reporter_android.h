// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_CHILD_ACCOUNT_FEEDBACK_REPORTER_ANDROID_H_
#define CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_CHILD_ACCOUNT_FEEDBACK_REPORTER_ANDROID_H_

#include <jni.h>
#include <string>

#include "base/macros.h"

namespace content {
class WebContents;
}

class GURL;

void ReportChildAccountFeedback(content::WebContents* web_contents,
                                const std::string& description,
                                const GURL& url);

// Register native methods
bool RegisterChildAccountFeedbackReporter(JNIEnv* env);

#endif  // CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_CHILD_ACCOUNT_FEEDBACK_REPORTER_ANDROID_H_
