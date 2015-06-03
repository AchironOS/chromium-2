// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.findinpage;

import android.app.Activity;
import android.view.ActionMode;
import android.view.View;
import android.view.ViewStub;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * Manages the interactions with the find toolbar.
 */
public class FindToolbarManager {
    private FindToolbar mFindToolbar;
    private final Activity mActivity;
    private final TabModelSelector mTabModelSelector;
    private final ActionMode.Callback mCallback;
    private final ObserverList<FindToolbarObserver> mObservers;

    /**
     * Creates an instance of a {@link FindToolbarManager}.
     * @param activity An Activity that represents a parent of the {@link android.view.ViewStub}.
     * @param selector The {@link TabModelSelector} that will interact with the {@link FindToolbar}.
     * @param callback The ActionMode.Callback that will be used when selection occurs on the
     *         {@link FindToolbar}.
     */
    public FindToolbarManager(
            Activity activity, TabModelSelector selector, ActionMode.Callback callback) {
        mActivity = activity;
        mTabModelSelector = selector;
        mCallback = callback;
        mObservers = new ObserverList<FindToolbarObserver>();
    }

    /**
     * @return Whether the find toolbar is currently showing.
     */
    public boolean isShowing() {
        return mFindToolbar != null && mFindToolbar.getVisibility() == View.VISIBLE;
    }

    /**
     * Hides the toolbar.
     */
    public void hideToolbar() {
        if (mFindToolbar == null) return;

        mFindToolbar.deactivate();
    }

    /**
     * Shows the toolbar if it's not already visible otherwise activates.
     */
    public void showToolbar() {
        if (mFindToolbar == null) {
            int stubId = com.google.android.apps.chrome.R.id.find_toolbar_stub;
            if (DeviceFormFactor.isTablet(mActivity)) {
                stubId = com.google.android.apps.chrome.R.id.find_toolbar_tablet_stub;
            }
            mFindToolbar = (FindToolbar) ((ViewStub) mActivity.findViewById(stubId)).inflate();
            mFindToolbar.setTabModelSelector(mTabModelSelector);
            mFindToolbar.setActionModeCallbackForTextEdit(mCallback);
            mFindToolbar.setObserver(new FindToolbarObserver() {
                @Override
                public void onFindToolbarShown() {
                    for (FindToolbarObserver observer : mObservers) {
                        observer.onFindToolbarShown();
                    }
                }

                @Override
                public void onFindToolbarHidden() {
                    for (FindToolbarObserver observer : mObservers) {
                        observer.onFindToolbarHidden();
                    }
                }
            });
        }

        mFindToolbar.activate();
    }

    /**
     * Add an observer for find in page changes.
     */
    public void addObserver(FindToolbarObserver observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Remove an observer for find in page changes.
     */
    public void removeObserver(FindToolbarObserver observer) {
        mObservers.removeObserver(observer);
    }
}