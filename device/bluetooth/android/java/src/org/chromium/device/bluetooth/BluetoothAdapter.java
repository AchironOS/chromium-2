// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth;

import android.Manifest;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanResult;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.pm.PackageManager;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.Log;

import java.util.List;

/**
 * Exposes android.bluetooth.BluetoothAdapter as necessary for C++
 * device::BluetoothAdapterAndroid.
 */
@JNINamespace("device")
final class BluetoothAdapter {
    private static final String TAG = Log.makeTag("Bluetooth");

    private final boolean mHasBluetoothCapability;
    private android.bluetooth.BluetoothAdapter mAdapter;
    private ScanCallback mLeScanCallback;

    // ---------------------------------------------------------------------------------------------
    // Construction

    @CalledByNative
    private static BluetoothAdapter create(Context context) {
        return new BluetoothAdapter(context);
    }

    @CalledByNative
    private static BluetoothAdapter createWithoutPermissionForTesting(Context context) {
        Context contextWithoutPermission = new ContextWrapper(context) {
            @Override
            public int checkCallingOrSelfPermission(String permission) {
                return PackageManager.PERMISSION_DENIED;
            }
        };
        return new BluetoothAdapter(contextWithoutPermission);
    }

    // Constructs a BluetoothAdapter.
    private BluetoothAdapter(Context context) {
        final boolean hasPermissions =
                context.checkCallingOrSelfPermission(Manifest.permission.BLUETOOTH)
                        == PackageManager.PERMISSION_GRANTED
                && context.checkCallingOrSelfPermission(Manifest.permission.BLUETOOTH_ADMIN)
                        == PackageManager.PERMISSION_GRANTED;
        final boolean hasLowEnergyFeature =
                context.getPackageManager().hasSystemFeature(PackageManager.FEATURE_BLUETOOTH_LE);
        mHasBluetoothCapability = hasPermissions && hasLowEnergyFeature;
        if (!mHasBluetoothCapability) {
            if (!hasPermissions)
                Log.w(TAG,
                        "Bluetooth API disabled; BLUETOOTH and BLUETOOTH_ADMIN permissions required.");
            if (!hasLowEnergyFeature)
                Log.i(TAG, "Bluetooth API disabled; Low Energy not supported on system.");
            return;
        }

        mAdapter = android.bluetooth.BluetoothAdapter.getDefaultAdapter();
        if (mAdapter == null)
            Log.i(TAG, "No adapter found.");
        else
            Log.i(TAG, "BluetoothAdapter successfully constructed.");
    }

    // ---------------------------------------------------------------------------------------------
    // Accessors @CalledByNative for BluetoothAdapterAndroid:

    @CalledByNative
    private boolean hasBluetoothCapability() {
        return mHasBluetoothCapability;
    }

    // ---------------------------------------------------------------------------------------------
    // BluetoothAdapterAndroid interface @CalledByNative for BluetoothAdapterAndroid:

    @CalledByNative
    private String getAddress() {
        if (isPresent()) {
            return mAdapter.getAddress();
        } else {
            return "";
        }
    }

    @CalledByNative
    private String getName() {
        if (isPresent()) {
            return mAdapter.getName();
        } else {
            return "";
        }
    }

    @CalledByNative
    private boolean isPresent() {
        return mAdapter != null;
    }

    @CalledByNative
    private boolean isPowered() {
        return isPresent() && mAdapter.isEnabled();
    }

    @CalledByNative
    private boolean isDiscoverable() {
        return isPresent()
                && mAdapter.getScanMode()
                == android.bluetooth.BluetoothAdapter.SCAN_MODE_CONNECTABLE_DISCOVERABLE;
    }

    @CalledByNative
    private boolean isDiscovering() {
        return isPresent() && mAdapter.isDiscovering();
    }

    @CalledByNative
    private void addDiscoverySession() {
        Log.i(TAG, "addDiscoverySession");
        if (!isPowered()) {
            Log.i(TAG, "addDiscoverySession: Fails: !isPowered");
            // TODO error callbacks.
            return;
        }

        if (mLeScanCallback != null) {
            Log.i(TAG, "addDiscoverySession: Already scanning.");
            // TODO call callbacks?
            return;
        }

        mLeScanCallback = new ScanCallback() {
            @Override
            public void onBatchScanResults(List<ScanResult> results) {
                Log.i(TAG, "onBatchScanResults");
                for (ScanResult result : results) {
                    // mAdapter.add(result);
                }
                // mAdapter.notifyDataSetChanged();
            }

            @Override
            public void onScanResult(int callbackType, ScanResult result) {
                Log.i(TAG, "onScanResult");
                // mAdapter.add(result);
                // mAdapter.notifyDataSetChanged();
            }

            @Override
            public void onScanFailed(int errorCode) {
                Log.i(TAG, "onScanFailed: %d", errorCode);
                // TODO
            }
        };
        mAdapter.getBluetoothLeScanner().startScan(mLeScanCallback);
    }

    // ---------------------------------------------------------------------------------------------
}
