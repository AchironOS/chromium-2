// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/content_action.h"

#include "base/base64.h"
#include "base/run_loop.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#include "ipc/ipc_message_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/ipc/gfx_param_traits.h"

namespace extensions {
namespace {

using base::test::ParseJson;
using testing::HasSubstr;


scoped_ptr<base::DictionaryValue> SimpleManifest() {
  return DictionaryBuilder()
      .Set("name", "extension")
      .Set("manifest_version", 2)
      .Set("version", "1.0")
      .Build();
}

class RequestContentScriptTest : public ExtensionServiceTestBase {
 public:
  RequestContentScriptTest()
      : extension_(ExtensionBuilder().SetManifest(SimpleManifest()).Build()) {};

  // TODO(rdevlin.cronin): This should be SetUp(), but an issues with invoking
  // InitializeEmptyExtensionService() within SetUp() means that we have to
  // call this manually within every test. This can be cleaned up once said
  // issue is fixed.
  virtual void Init() {
    InitializeEmptyExtensionService();
    static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile()))->
        SetReady();
    base::RunLoop().RunUntilIdle();
  }

  Profile* profile() { return profile_.get(); }
  Extension* extension() { return extension_.get(); }

 private:
  scoped_refptr<Extension> extension_;
};

TEST(DeclarativeContentActionTest, InvalidCreation) {
  TestExtensionEnvironment env;
  std::string error;
  bool bad_message = false;
  scoped_refptr<const ContentAction> result;

  // Test wrong data type passed.
  error.clear();
  result = ContentAction::Create(
      NULL, HostID(), NULL, *ParseJson("[]"), &error, &bad_message);
  EXPECT_TRUE(bad_message);
  EXPECT_EQ("", error);
  EXPECT_FALSE(result.get());

  // Test missing instanceType element.
  error.clear();
  result = ContentAction::Create(
      NULL, HostID(), NULL, *ParseJson("{}"), &error, &bad_message);
  EXPECT_TRUE(bad_message);
  EXPECT_EQ("", error);
  EXPECT_FALSE(result.get());

  // Test wrong instanceType element.
  error.clear();
  result = ContentAction::Create(NULL, HostID(), NULL, *ParseJson(
      "{\n"
      "  \"instanceType\": \"declarativeContent.UnknownType\",\n"
      "}"),
                                 &error, &bad_message);
  EXPECT_THAT(error, HasSubstr("invalid instanceType"));
  EXPECT_FALSE(result.get());
}

TEST(DeclarativeContentActionTest, ShowPageActionWithoutPageAction) {
  TestExtensionEnvironment env;

  const Extension* extension = env.MakeExtension(base::DictionaryValue());
  std::string error;
  bool bad_message = false;
  scoped_refptr<const ContentAction> result = ContentAction::Create(
      NULL,
      HostID(HostID::EXTENSIONS, extension->id()),
      extension,
      *ParseJson(
           "{\n"
           "  \"instanceType\": \"declarativeContent.ShowPageAction\",\n"
           "}"),
      &error,
      &bad_message);
  EXPECT_THAT(error, testing::HasSubstr("without a page action"));
  EXPECT_FALSE(bad_message);
  ASSERT_FALSE(result.get());
}

TEST(DeclarativeContentActionTest, ShowPageAction) {
  TestExtensionEnvironment env;

  const Extension* extension = env.MakeExtension(
      *ParseJson("{\"page_action\": { \"default_title\": \"Extension\" } }"));
  std::string error;
  bool bad_message = false;
  scoped_refptr<const ContentAction> result = ContentAction::Create(
      NULL,
      HostID(HostID::EXTENSIONS, extension->id()),
      extension,
      *ParseJson(
           "{\n"
           "  \"instanceType\": \"declarativeContent.ShowPageAction\",\n"
           "}"),
      &error,
      &bad_message);
  EXPECT_EQ("", error);
  EXPECT_FALSE(bad_message);
  ASSERT_TRUE(result.get());
  EXPECT_EQ(ContentAction::ACTION_SHOW_PAGE_ACTION, result->GetType());

  ExtensionAction* page_action =
      ExtensionActionManager::Get(env.profile())->GetPageAction(*extension);
  scoped_ptr<content::WebContents> contents = env.MakeTab();
  const int tab_id = ExtensionTabUtil::GetTabId(contents.get());
  EXPECT_FALSE(page_action->GetIsVisible(tab_id));
  ContentAction::ApplyInfo apply_info = {
    env.profile(), contents.get()
  };
  result->Apply(extension->id(), base::Time(), &apply_info);
  EXPECT_TRUE(page_action->GetIsVisible(tab_id));
  result->Apply(extension->id(), base::Time(), &apply_info);
  EXPECT_TRUE(page_action->GetIsVisible(tab_id));
  result->Revert(extension->id(), base::Time(), &apply_info);
  EXPECT_TRUE(page_action->GetIsVisible(tab_id));
  result->Revert(extension->id(), base::Time(), &apply_info);
  EXPECT_FALSE(page_action->GetIsVisible(tab_id));
}

TEST(DeclarativeContentActionTest, SetIcon) {
  TestExtensionEnvironment env;

  // Simulate the process of passing ImageData to SetIcon::Create.
  SkBitmap bitmap;
  EXPECT_TRUE(bitmap.tryAllocN32Pixels(19, 19));
  bitmap.eraseARGB(0,0,0,0);
  uint32_t* pixels = bitmap.getAddr32(0, 0);
  for (int i = 0; i < 19 * 19; ++i)
    pixels[i] = i;
  IPC::Message bitmap_pickle;
  IPC::WriteParam(&bitmap_pickle, bitmap);
  std::string binary_data = std::string(
      static_cast<const char*>(bitmap_pickle.data()), bitmap_pickle.size());
  std::string data64;
  base::Base64Encode(binary_data, &data64);

  scoped_ptr<base::DictionaryValue> dict =
      DictionaryBuilder().Set("instanceType", "declarativeContent.SetIcon")
                         .Set("imageData",
                              DictionaryBuilder().Set("19", data64)).Build();

  const Extension* extension = env.MakeExtension(
      *ParseJson("{\"page_action\": { \"default_title\": \"Extension\" } }"));
  std::string error;
  bool bad_message = false;
  scoped_refptr<const ContentAction> result = ContentAction::Create(
      NULL,
      HostID(HostID::EXTENSIONS, extension->id()),
      extension,
      *dict,
      &error,
      &bad_message);
  EXPECT_EQ("", error);
  EXPECT_FALSE(bad_message);
  ASSERT_TRUE(result.get());
  EXPECT_EQ(ContentAction::ACTION_SET_ICON, result->GetType());

  ExtensionAction* page_action =
      ExtensionActionManager::Get(env.profile())->GetPageAction(*extension);
  scoped_ptr<content::WebContents> contents = env.MakeTab();
  const int tab_id = ExtensionTabUtil::GetTabId(contents.get());
  EXPECT_FALSE(page_action->GetIsVisible(tab_id));
  ContentAction::ApplyInfo apply_info = {
    env.profile(), contents.get()
  };

  // The declarative icon shouldn't exist unless the content action is applied.
  EXPECT_TRUE(page_action->GetDeclarativeIcon(tab_id).bitmap()->empty());
  result->Apply(extension->id(), base::Time(), &apply_info);
  EXPECT_FALSE(page_action->GetDeclarativeIcon(tab_id).bitmap()->empty());
  result->Revert(extension->id(), base::Time(), &apply_info);
  EXPECT_TRUE(page_action->GetDeclarativeIcon(tab_id).bitmap()->empty());
}

TEST_F(RequestContentScriptTest, MissingScripts) {
  Init();
  std::string error;
  bool bad_message = false;
  scoped_refptr<const ContentAction> result = ContentAction::Create(
      profile(),
      HostID(HostID::EXTENSIONS, extension()->id()),
      extension(),
      *ParseJson(
          "{\n"
          "  \"instanceType\": \"declarativeContent.RequestContentScript\",\n"
          "  \"allFrames\": true,\n"
          "  \"matchAboutBlank\": true\n"
          "}"),
      &error,
      &bad_message);
  EXPECT_THAT(error, testing::HasSubstr("Missing parameter is required"));
  EXPECT_FALSE(bad_message);
  ASSERT_FALSE(result.get());
}

TEST_F(RequestContentScriptTest, CSS) {
  Init();
  std::string error;
  bool bad_message = false;
  scoped_refptr<const ContentAction> result = ContentAction::Create(
      profile(),
      HostID(HostID::EXTENSIONS, extension()->id()),
      extension(),
      *ParseJson(
          "{\n"
          "  \"instanceType\": \"declarativeContent.RequestContentScript\",\n"
          "  \"css\": [\"style.css\"]\n"
          "}"),
      &error,
      &bad_message);
  EXPECT_EQ("", error);
  EXPECT_FALSE(bad_message);
  ASSERT_TRUE(result.get());
  EXPECT_EQ(ContentAction::ACTION_REQUEST_CONTENT_SCRIPT, result->GetType());
}

TEST_F(RequestContentScriptTest, JS) {
  Init();
  std::string error;
  bool bad_message = false;
  scoped_refptr<const ContentAction> result = ContentAction::Create(
      profile(),
      HostID(HostID::EXTENSIONS, extension()->id()),
      extension(),
      *ParseJson(
          "{\n"
          "  \"instanceType\": \"declarativeContent.RequestContentScript\",\n"
          "  \"js\": [\"script.js\"]\n"
          "}"),
      &error,
      &bad_message);
  EXPECT_EQ("", error);
  EXPECT_FALSE(bad_message);
  ASSERT_TRUE(result.get());
  EXPECT_EQ(ContentAction::ACTION_REQUEST_CONTENT_SCRIPT, result->GetType());
}

TEST_F(RequestContentScriptTest, CSSBadType) {
  Init();
  std::string error;
  bool bad_message = false;
  scoped_refptr<const ContentAction> result = ContentAction::Create(
      profile(),
      HostID(HostID::EXTENSIONS, extension()->id()),
      extension(),
      *ParseJson(
          "{\n"
          "  \"instanceType\": \"declarativeContent.RequestContentScript\",\n"
          "  \"css\": \"style.css\"\n"
          "}"),
      &error,
      &bad_message);
  EXPECT_TRUE(bad_message);
  ASSERT_FALSE(result.get());
}

TEST_F(RequestContentScriptTest, JSBadType) {
  Init();
  std::string error;
  bool bad_message = false;
  scoped_refptr<const ContentAction> result = ContentAction::Create(
      profile(),
      HostID(HostID::EXTENSIONS, extension()->id()),
      extension(),
      *ParseJson(
          "{\n"
          "  \"instanceType\": \"declarativeContent.RequestContentScript\",\n"
          "  \"js\": \"script.js\"\n"
          "}"),
      &error,
      &bad_message);
  EXPECT_TRUE(bad_message);
  ASSERT_FALSE(result.get());
}

TEST_F(RequestContentScriptTest, AllFrames) {
  Init();
  std::string error;
  bool bad_message = false;
  scoped_refptr<const ContentAction> result = ContentAction::Create(
      profile(),
      HostID(HostID::EXTENSIONS, extension()->id()),
      extension(),
      *ParseJson(
          "{\n"
          "  \"instanceType\": \"declarativeContent.RequestContentScript\",\n"
          "  \"js\": [\"script.js\"],\n"
          "  \"allFrames\": true\n"
          "}"),
      &error,
      &bad_message);
  EXPECT_EQ("", error);
  EXPECT_FALSE(bad_message);
  ASSERT_TRUE(result.get());
  EXPECT_EQ(ContentAction::ACTION_REQUEST_CONTENT_SCRIPT, result->GetType());
}

TEST_F(RequestContentScriptTest, MatchAboutBlank) {
  Init();
  std::string error;
  bool bad_message = false;
  scoped_refptr<const ContentAction> result = ContentAction::Create(
      profile(),
      HostID(HostID::EXTENSIONS, extension()->id()),
      extension(),
      *ParseJson(
          "{\n"
          "  \"instanceType\": \"declarativeContent.RequestContentScript\",\n"
          "  \"js\": [\"script.js\"],\n"
          "  \"matchAboutBlank\": true\n"
          "}"),
      &error,
      &bad_message);
  EXPECT_EQ("", error);
  EXPECT_FALSE(bad_message);
  ASSERT_TRUE(result.get());
  EXPECT_EQ(ContentAction::ACTION_REQUEST_CONTENT_SCRIPT, result->GetType());
}

TEST_F(RequestContentScriptTest, AllFramesBadType) {
  Init();
  std::string error;
  bool bad_message = false;
  scoped_refptr<const ContentAction> result = ContentAction::Create(
      profile(),
      HostID(HostID::EXTENSIONS, extension()->id()),
      extension(),
      *ParseJson(
          "{\n"
          "  \"instanceType\": \"declarativeContent.RequestContentScript\",\n"
          "  \"js\": [\"script.js\"],\n"
          "  \"allFrames\": null\n"
          "}"),
      &error,
      &bad_message);
  EXPECT_TRUE(bad_message);
  ASSERT_FALSE(result.get());
}

TEST_F(RequestContentScriptTest, MatchAboutBlankBadType) {
  Init();
  std::string error;
  bool bad_message = false;
  scoped_refptr<const ContentAction> result = ContentAction::Create(
      profile(),
      HostID(HostID::EXTENSIONS, extension()->id()),
      extension(),
      *ParseJson(
          "{\n"
          "  \"instanceType\": \"declarativeContent.RequestContentScript\",\n"
          "  \"js\": [\"script.js\"],\n"
          "  \"matchAboutBlank\": null\n"
          "}"),
      &error,
      &bad_message);
  EXPECT_TRUE(bad_message);
  ASSERT_FALSE(result.get());
}

}  // namespace
}  // namespace extensions
