# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'internal_ozone_platform_deps': [
      'ozone_platform_drm',
    ],
    'internal_ozone_platform_unittest_deps': [
      'ozone_platform_drm_unittests',
    ],
    'internal_ozone_platforms': [
      'dri',
      'drm',
    ],
  },
  'targets': [
    {
      'target_name': 'ozone_platform_drm',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../build/linux/system.gyp:libdrm',
        '../../skia/skia.gyp:skia',
        '../base/ui_base.gyp:ui_base',
        '../display/display.gyp:display_types',
        '../display/display.gyp:display_util',
        '../events/devices/events_devices.gyp:events_devices',
        '../events/events.gyp:events',
        '../events/ozone/events_ozone.gyp:events_ozone_evdev',
        '../events/ozone/events_ozone.gyp:events_ozone_layout',
        '../events/platform/events_platform.gyp:events_platform',
        '../gfx/gfx.gyp:gfx',
        '../gfx/gfx.gyp:gfx_geometry',
      ],
      'defines': [
        'OZONE_IMPLEMENTATION',
      ],
      'sources': [
        'common/drm_util.cc',
        'common/drm_util.h',
        'common/scoped_drm_types.cc',
        'common/scoped_drm_types.h',
        'gpu/crtc_controller.cc',
        'gpu/crtc_controller.h',
        'gpu/display_change_observer.h',
        'gpu/drm_buffer.cc',
        'gpu/drm_buffer.h',
        'gpu/drm_console_buffer.cc',
        'gpu/drm_console_buffer.h',
        'gpu/drm_device.cc',
        'gpu/drm_device.h',
        'gpu/drm_device_generator.cc',
        'gpu/drm_device_generator.h',
        'gpu/drm_device_manager.cc',
        'gpu/drm_device_manager.h',
        'gpu/drm_display.cc',
        'gpu/drm_display.h',
        'gpu/drm_gpu_display_manager.cc',
        'gpu/drm_gpu_display_manager.h',
        'gpu/drm_gpu_platform_support.cc',
        'gpu/drm_gpu_platform_support.h',
        'gpu/drm_surface.cc',
        'gpu/drm_surface.h',
        'gpu/drm_surface_factory.cc',
        'gpu/drm_surface_factory.h',
        'gpu/drm_vsync_provider.cc',
        'gpu/drm_vsync_provider.h',
        'gpu/drm_window.cc',
        'gpu/drm_window.h',
        'gpu/hardware_display_controller.cc',
        'gpu/hardware_display_controller.h',
        'gpu/hardware_display_plane.cc',
        'gpu/hardware_display_plane.h',
        'gpu/hardware_display_plane_manager.cc',
        'gpu/hardware_display_plane_manager.h',
        'gpu/hardware_display_plane_manager_legacy.cc',
        'gpu/hardware_display_plane_manager_legacy.h',
        'gpu/overlay_plane.cc',
        'gpu/overlay_plane.h',
        'gpu/page_flip_request.cc',
        'gpu/page_flip_request.h',
        'gpu/screen_manager.cc',
        'gpu/screen_manager.h',
        'host/channel_observer.h',
        'host/drm_cursor.cc',
        'host/drm_cursor.h',
        'host/drm_device_handle.cc',
        'host/drm_device_handle.h',
        'host/drm_display_host.cc',
        'host/drm_display_host.h',
        'host/drm_display_host_manager.cc',
        'host/drm_display_host_manager.h',
        'host/drm_gpu_platform_support_host.cc',
        'host/drm_gpu_platform_support_host.h',
        'host/drm_native_display_delegate.cc',
        'host/drm_native_display_delegate.h',
        'host/drm_overlay_manager.cc',
        'host/drm_overlay_manager.h',
        'host/drm_window_host.cc',
        'host/drm_window_host.h',
        'host/drm_window_host_manager.cc',
        'host/drm_window_host_manager.h',
        'ozone_platform_drm.cc',
        'ozone_platform_drm.h',
        'scanout_buffer.h',
      ],
    },
    {
      'target_name': 'ozone_platform_drm_unittests',
      'type': 'none',
      'dependencies': [
        '../../build/linux/system.gyp:libdrm',
        '../../skia/skia.gyp:skia',
        '../gfx/gfx.gyp:gfx',
        '../gfx/gfx.gyp:gfx_geometry',
        'ozone.gyp:ozone',
      ],
      'export_dependent_settings': [
        '../../build/linux/system.gyp:libdrm',
        '../../skia/skia.gyp:skia',
        '../gfx/gfx.gyp:gfx_geometry',
      ],
      'direct_dependent_settings': {
        'sources': [
          'gpu/drm_surface_unittest.cc',
          'gpu/drm_window_unittest.cc',
          'gpu/hardware_display_controller_unittest.cc',
          'gpu/hardware_display_plane_manager_unittest.cc',
          'gpu/screen_manager_unittest.cc',
          'test/mock_drm_device.cc',
          'test/mock_drm_device.h',
        ],
      },
    },
  ],
}
