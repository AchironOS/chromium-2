# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //chrome/browser/devtools
      'target_name': 'debugger',
      'type': 'static_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'dependencies': [
        '../base/base.gyp:base',
        '../content/content.gyp:content_browser',
        '../net/net.gyp:net',
        'browser/devtools/webrtc_device_provider_resources.gyp:webrtc_device_provider_resources',
        'browser/devtools/devtools_protocol_constants.gyp:devtools_protocol_constants',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # These files are generated by devtools_protocol_constants_generator.py.
        '<(SHARED_INTERMEDIATE_DIR)/chrome/browser/devtools/devtools_protocol_constants.cc',
        '<(SHARED_INTERMEDIATE_DIR)/chrome/browser/devtools/devtools_protocol_constants.h',
        'browser/devtools/devtools_network_conditions.cc',
        'browser/devtools/devtools_network_conditions.h',
        'browser/devtools/devtools_network_controller.cc',
        'browser/devtools/devtools_network_controller.h',
        'browser/devtools/devtools_network_interceptor.cc',
        'browser/devtools/devtools_network_interceptor.h',
        'browser/devtools/devtools_network_protocol_handler.cc',
        'browser/devtools/devtools_network_protocol_handler.h',
        'browser/devtools/devtools_network_transaction.cc',
        'browser/devtools/devtools_network_transaction.h',
        'browser/devtools/devtools_network_transaction_factory.cc',
        'browser/devtools/devtools_network_transaction_factory.h',
        'browser/devtools/devtools_protocol.cc',
        'browser/devtools/devtools_protocol.h',
      ],
      'conditions': [
        ['OS!="android"', {
          'dependencies': [
            '../components/components.gyp:devtools_http_handler',
            '../net/net.gyp:http_server',
            '../skia/skia.gyp:skia',
            '../third_party/icu/icu.gyp:icui18n',
            '../third_party/icu/icu.gyp:icuuc',
            '../third_party/leveldatabase/leveldatabase.gyp:leveldatabase',
            '../third_party/libusb/libusb.gyp:libusb',
            'chrome_resources.gyp:chrome_extra_resources',
            'chrome_resources.gyp:chrome_resources',
            'chrome_resources.gyp:chrome_strings',
            'chrome_resources.gyp:theme_resources',
            'common/extensions/api/api.gyp:chrome_api',
          ],
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome/grit/webrtc_device_provider_resources_map.cc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/grit/webrtc_device_provider_resources_map.h',
            'browser/devtools/chrome_devtools_manager_delegate.cc',
            'browser/devtools/chrome_devtools_manager_delegate.h',
            'browser/devtools/device/adb/adb_client_socket.cc',
            'browser/devtools/device/adb/adb_client_socket.h',
            'browser/devtools/device/adb/adb_device_provider.cc',
            'browser/devtools/device/adb/adb_device_provider.h',
            'browser/devtools/device/android_device_info_query.cc',
            'browser/devtools/device/android_device_manager.cc',
            'browser/devtools/device/android_device_manager.h',
            'browser/devtools/device/android_web_socket.cc',
            'browser/devtools/device/devtools_android_bridge.cc',
            'browser/devtools/device/devtools_android_bridge.h',
            'browser/devtools/device/port_forwarding_controller.cc',
            'browser/devtools/device/port_forwarding_controller.h',
            'browser/devtools/device/self_device_provider.cc',
            'browser/devtools/device/self_device_provider.h',
            'browser/devtools/device/usb/android_rsa.cc',
            'browser/devtools/device/usb/android_rsa.h',
            'browser/devtools/device/usb/android_usb_device.cc',
            'browser/devtools/device/usb/android_usb_device.h',
            'browser/devtools/device/usb/android_usb_socket.cc',
            'browser/devtools/device/usb/android_usb_socket.h',
            'browser/devtools/device/usb/usb_device_provider.cc',
            'browser/devtools/device/usb/usb_device_provider.h',
            'browser/devtools/device/webrtc/devtools_bridge_client.cc',
            'browser/devtools/device/webrtc/devtools_bridge_client.h',
            'browser/devtools/device/webrtc/devtools_bridge_instances_request.cc',
            'browser/devtools/device/webrtc/devtools_bridge_instances_request.h',
            'browser/devtools/device/webrtc/send_command_request.cc',
            'browser/devtools/device/webrtc/send_command_request.h',
            'browser/devtools/device/webrtc/webrtc_device_provider.cc',
            'browser/devtools/device/webrtc/webrtc_device_provider.h',
            'browser/devtools/devtools_contents_resizing_strategy.cc',
            'browser/devtools/devtools_contents_resizing_strategy.h',
            'browser/devtools/devtools_embedder_message_dispatcher.cc',
            'browser/devtools/devtools_embedder_message_dispatcher.h',
            'browser/devtools/devtools_file_helper.cc',
            'browser/devtools/devtools_file_helper.h',
            'browser/devtools/devtools_file_system_indexer.cc',
            'browser/devtools/devtools_file_system_indexer.h',
            'browser/devtools/devtools_target_impl.cc',
            'browser/devtools/devtools_target_impl.h',
            'browser/devtools/devtools_targets_ui.cc',
            'browser/devtools/devtools_targets_ui.h',
            'browser/devtools/devtools_toggle_action.cc',
            'browser/devtools/devtools_toggle_action.h',
            'browser/devtools/devtools_ui_bindings.cc',
            'browser/devtools/devtools_ui_bindings.h',
            'browser/devtools/devtools_window.cc',
            'browser/devtools/devtools_window.h',
            'browser/devtools/remote_debugging_server.cc',
            'browser/devtools/remote_debugging_server.h',
          ],
        }], # OS!="android"
        ['debug_devtools==1', {
          'defines': [
            'DEBUG_DEVTOOLS=1',
           ],
        }],
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
  ],
}
