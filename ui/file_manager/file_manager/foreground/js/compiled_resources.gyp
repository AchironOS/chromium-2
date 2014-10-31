# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'main',
      'variables': {
        'depends': [
          '../../../../../third_party/jstemplate/compiled_resources.gyp:jstemplate',
          '../../../../../ui/webui/resources/js/load_time_data.js',
          '../../../../../ui/webui/resources/js/cr.js',
          '../../../../../ui/webui/resources/js/util.js',
          '../../../../../ui/webui/resources/js/i18n_template_no_process.js',
          '../../../../../ui/webui/resources/js/event_tracker.js',
          '../../../../../ui/webui/resources/js/cr/ui.js',
          '../../../../../ui/webui/resources/js/cr/event_target.js',
          '../../../../../ui/webui/resources/js/cr/ui/touch_handler.js',
          '../../../../../ui/webui/resources/js/cr/ui/array_data_model.js',
          '../../../../../ui/webui/resources/js/cr/ui/dialogs.js',
          '../../../../../ui/webui/resources/js/cr/ui/list_item.js',
          '../../../../../ui/webui/resources/js/cr/ui/list_selection_model.js',
          '../../../../../ui/webui/resources/js/cr/ui/list_single_selection_model.js',
          '../../../../../ui/webui/resources/js/cr/ui/list_selection_controller.js',
          '../../../../../ui/webui/resources/js/cr/ui/list.js',
          '../../../../../ui/webui/resources/js/cr/ui/tree.js',
          '../../../../../ui/webui/resources/js/cr/ui/autocomplete_list.js',
          '../../../../../ui/webui/resources/js/cr/ui/splitter.js',
          '../../../../../ui/webui/resources/js/cr/ui/table/table_splitter.js',
          '../../../../../ui/webui/resources/js/cr/ui/table/table_column.js',
          '../../../../../ui/webui/resources/js/cr/ui/table/table_column_model.js',
          '../../../../../ui/webui/resources/js/cr/ui/table/table_header.js',
          '../../../../../ui/webui/resources/js/cr/ui/table/table_list.js',
          '../../../../../ui/webui/resources/js/cr/ui/table.js',
          '../../../../../ui/webui/resources/js/cr/ui/grid.js',
          '../../../../../ui/webui/resources/js/cr/ui/command.js',
          '../../../../../ui/webui/resources/js/cr/ui/position_util.js',
          '../../../../../ui/webui/resources/js/cr/ui/menu_item.js',
          '../../../../../ui/webui/resources/js/cr/ui/menu.js',
          '../../../../../ui/webui/resources/js/cr/ui/menu_button.js',
          '../../../../../ui/webui/resources/js/cr/ui/context_menu_handler.js',
          '../../common/js/error_util.js',
          '../../common/js/async_util.js',
          '../../common/js/volume_manager_common.js',
          '../../common/js/util.js',
          '../../common/js/progress_center_common.js',
          '../../background/js/file_operation_manager.js',
          '../../background/js/file_operation_handler.js',
          '../../background/js/device_handler.js',
          '../../background/js/drive_sync_handler.js',
          '../../background/js/volume_manager.js',
          '../../background/js/progress_center.js',
          '../../background/js/app_window_wrapper.js',
          '../../background/js/background_base.js',
          '../../background/js/background.js',
          '../../../image_loader/image_loader_client.js',
          './metrics.js',
          './ui/combobutton.js',
          './ui/commandbutton.js',
          './ui/file_manager_dialog_base.js',
          './app_installer.js',
          './cws_container_client.js',
          './directory_contents.js',
          './directory_model.js',
          './file_manager.js',
          './file_manager_commands.js',
          './file_selection.js',
          './file_tasks.js',
          './file_transfer_controller.js',
          './file_type.js',
          './file_watcher.js',
          './folder_shortcuts_data_model.js',
          './metadata/metadata_cache.js',
          './naming_controller.js',
          './navigation_list_model.js',
          './progress_center_item_group.js',
          './search_controller.js',
          './share_client.js',
          './thumbnail_loader.js',
          './ui/conflict_dialog.js',
          './ui/default_action_dialog.js',
          './ui/dialog_footer.js',
          './ui/directory_tree.js',
          './ui/drag_selector.js',
          './ui/drive_banners.js',
          './ui/error_dialog.js',
          './ui/file_grid.js',
          './ui/file_manager_ui.js',
          './ui/file_table.js',
          './ui/location_line.js',
          './ui/multi_profile_share_dialog.js',
          './ui/preview_panel.js',
          './ui/progress_center_panel.js',
          './ui/scrollbar.js',
          './ui/search_box.js',
          './ui/share_dialog.js',
          './ui/suggest_apps_dialog.js',
          './ui/tree.css.js',
          './volume_manager_wrapper.js',
          './metadata/byte_reader.js',
        ],
        'externs': [
          '<(CLOSURE_DIR)/externs/chrome_send_externs.js',
          '<(CLOSURE_DIR)/externs/chrome_extensions.js',
          '<(CLOSURE_DIR)/externs/file_manager_private.js',
          '<(CLOSURE_DIR)/externs/metrics_private.js',
          '../../../externs/chrome_echo_private.js',
          '../../../externs/chrome_file_browser_handler.js',
          '../../../externs/chrome_test.js',
          '../../../externs/connection.js',
          '../../../externs/css_rule.js',
          '../../../externs/html_menu_item_element.js',
          '../../../externs/webview_tag.js',
          '../../common/js/externs.js',
        ],
      },
      'includes': [
        '../../../../../third_party/closure_compiler/compile_js.gypi'
      ],
    }
  ],
}
