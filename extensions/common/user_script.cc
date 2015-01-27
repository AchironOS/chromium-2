// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/user_script.h"

#include "base/atomic_sequence_num.h"
#include "base/command_line.h"
#include "base/pickle.h"
#include "base/strings/string_util.h"
#include "extensions/common/switches.h"

namespace {

// This cannot be a plain int or int64 because we need to generate unique IDs
// from multiple threads.
base::StaticAtomicSequenceNumber g_user_script_id_generator;

bool UrlMatchesGlobs(const std::vector<std::string>* globs,
                     const GURL& url) {
  for (std::vector<std::string>::const_iterator glob = globs->begin();
       glob != globs->end(); ++glob) {
    if (MatchPattern(url.spec(), *glob))
      return true;
  }

  return false;
}

}  // namespace

namespace extensions {

// The bitmask for valid user script injectable schemes used by URLPattern.
enum {
  kValidUserScriptSchemes = URLPattern::SCHEME_CHROMEUI |
                            URLPattern::SCHEME_HTTP |
                            URLPattern::SCHEME_HTTPS |
                            URLPattern::SCHEME_FILE |
                            URLPattern::SCHEME_FTP
};

// static
const char UserScript::kFileExtension[] = ".user.js";


// static
int UserScript::GenerateUserScriptID() {
   return g_user_script_id_generator.GetNext();
}

bool UserScript::IsURLUserScript(const GURL& url,
                                 const std::string& mime_type) {
  return EndsWith(url.ExtractFileName(), kFileExtension, false) &&
      mime_type != "text/html";
}

// static
int UserScript::ValidUserScriptSchemes(bool canExecuteScriptEverywhere) {
  if (canExecuteScriptEverywhere)
    return URLPattern::SCHEME_ALL;
  int valid_schemes = kValidUserScriptSchemes;
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kExtensionsOnChromeURLs)) {
    valid_schemes &= ~URLPattern::SCHEME_CHROMEUI;
  }
  return valid_schemes;
}

UserScript::File::File(const base::FilePath& extension_root,
                       const base::FilePath& relative_path,
                       const GURL& url)
    : extension_root_(extension_root),
      relative_path_(relative_path),
      url_(url) {
}

UserScript::File::File() {}

UserScript::File::~File() {}

UserScript::UserScript()
    : run_location_(DOCUMENT_IDLE),
      user_script_id_(-1),
      emulate_greasemonkey_(false),
      match_all_frames_(false),
      match_about_blank_(false),
      incognito_enabled_(false) {}

UserScript::~UserScript() {
}

void UserScript::add_url_pattern(const URLPattern& pattern) {
  url_set_.AddPattern(pattern);
}

void UserScript::add_exclude_url_pattern(const URLPattern& pattern) {
  exclude_url_set_.AddPattern(pattern);
}

bool UserScript::MatchesURL(const GURL& url) const {
  if (!url_set_.is_empty()) {
    if (!url_set_.MatchesURL(url))
      return false;
  }

  if (!exclude_url_set_.is_empty()) {
    if (exclude_url_set_.MatchesURL(url))
      return false;
  }

  if (!globs_.empty()) {
    if (!UrlMatchesGlobs(&globs_, url))
      return false;
  }

  if (!exclude_globs_.empty()) {
    if (UrlMatchesGlobs(&exclude_globs_, url))
      return false;
  }

  return true;
}

void UserScript::File::Pickle(::Pickle* pickle) const {
  pickle->WriteString(url_.spec());
  // Do not write path. It's not needed in the renderer.
  // Do not write content. It will be serialized by other means.
}

void UserScript::File::Unpickle(const ::Pickle& pickle, PickleIterator* iter) {
  // Read the url from the pickle.
  std::string url;
  CHECK(iter->ReadString(&url));
  set_url(GURL(url));
}

void UserScript::Pickle(::Pickle* pickle) const {
  // Write the simple types to the pickle.
  pickle->WriteInt(run_location());
  pickle->WriteInt(user_script_id_);
  pickle->WriteBool(emulate_greasemonkey());
  pickle->WriteBool(match_all_frames());
  pickle->WriteBool(match_about_blank());
  pickle->WriteBool(is_incognito_enabled());

  PickleHostID(pickle, host_id_);
  PickleGlobs(pickle, globs_);
  PickleGlobs(pickle, exclude_globs_);
  PickleURLPatternSet(pickle, url_set_);
  PickleURLPatternSet(pickle, exclude_url_set_);
  PickleScripts(pickle, js_scripts_);
  PickleScripts(pickle, css_scripts_);
}

void UserScript::PickleGlobs(::Pickle* pickle,
                             const std::vector<std::string>& globs) const {
  pickle->WriteSizeT(globs.size());
  for (std::vector<std::string>::const_iterator glob = globs.begin();
       glob != globs.end(); ++glob) {
    pickle->WriteString(*glob);
  }
}

void UserScript::PickleHostID(::Pickle* pickle, const HostID& host_id) const {
  pickle->WriteInt(host_id.type());
  pickle->WriteString(host_id.id());
}

void UserScript::PickleURLPatternSet(::Pickle* pickle,
                                     const URLPatternSet& pattern_list) const {
  pickle->WriteSizeT(pattern_list.patterns().size());
  for (URLPatternSet::const_iterator pattern = pattern_list.begin();
       pattern != pattern_list.end(); ++pattern) {
    pickle->WriteInt(pattern->valid_schemes());
    pickle->WriteString(pattern->GetAsString());
  }
}

void UserScript::PickleScripts(::Pickle* pickle,
                               const FileList& scripts) const {
  pickle->WriteSizeT(scripts.size());
  for (FileList::const_iterator file = scripts.begin();
       file != scripts.end(); ++file) {
    file->Pickle(pickle);
  }
}

void UserScript::Unpickle(const ::Pickle& pickle, PickleIterator* iter) {
  // Read the run location.
  int run_location = 0;
  CHECK(iter->ReadInt(&run_location));
  CHECK(run_location >= 0 && run_location < RUN_LOCATION_LAST);
  run_location_ = static_cast<RunLocation>(run_location);

  CHECK(iter->ReadInt(&user_script_id_));
  CHECK(iter->ReadBool(&emulate_greasemonkey_));
  CHECK(iter->ReadBool(&match_all_frames_));
  CHECK(iter->ReadBool(&match_about_blank_));
  CHECK(iter->ReadBool(&incognito_enabled_));

  UnpickleHostID(pickle, iter, &host_id_);
  UnpickleGlobs(pickle, iter, &globs_);
  UnpickleGlobs(pickle, iter, &exclude_globs_);
  UnpickleURLPatternSet(pickle, iter, &url_set_);
  UnpickleURLPatternSet(pickle, iter, &exclude_url_set_);
  UnpickleScripts(pickle, iter, &js_scripts_);
  UnpickleScripts(pickle, iter, &css_scripts_);
}

void UserScript::UnpickleGlobs(const ::Pickle& pickle, PickleIterator* iter,
                               std::vector<std::string>* globs) {
  size_t num_globs = 0;
  CHECK(iter->ReadSizeT(&num_globs));
  globs->clear();
  for (size_t i = 0; i < num_globs; ++i) {
    std::string glob;
    CHECK(iter->ReadString(&glob));
    globs->push_back(glob);
  }
}

void UserScript::UnpickleHostID(const ::Pickle& pickle,
                                PickleIterator* iter,
                                HostID* host_id) {
  int type = 0;
  std::string id;
  CHECK(iter->ReadInt(&type));
  CHECK(iter->ReadString(&id));
  *host_id = HostID(static_cast<HostID::HostType>(type), id);
}

void UserScript::UnpickleURLPatternSet(const ::Pickle& pickle,
                                       PickleIterator* iter,
                                       URLPatternSet* pattern_list) {
  size_t num_patterns = 0;
  CHECK(iter->ReadSizeT(&num_patterns));

  pattern_list->ClearPatterns();
  for (size_t i = 0; i < num_patterns; ++i) {
    int valid_schemes;
    CHECK(iter->ReadInt(&valid_schemes));

    std::string pattern_str;
    CHECK(iter->ReadString(&pattern_str));

    URLPattern pattern(kValidUserScriptSchemes);
    URLPattern::ParseResult result = pattern.Parse(pattern_str);
    CHECK(URLPattern::PARSE_SUCCESS == result) <<
        URLPattern::GetParseResultString(result) << " " << pattern_str.c_str();

    pattern.SetValidSchemes(valid_schemes);
    pattern_list->AddPattern(pattern);
  }
}

void UserScript::UnpickleScripts(const ::Pickle& pickle, PickleIterator* iter,
                                 FileList* scripts) {
  size_t num_files = 0;
  CHECK(iter->ReadSizeT(&num_files));
  scripts->clear();
  for (size_t i = 0; i < num_files; ++i) {
    File file;
    file.Unpickle(pickle, iter);
    scripts->push_back(file);
  }
}

bool operator<(const UserScript& script1, const UserScript& script2) {
  // The only kind of script that should be compared is the kind that has its
  // IDs initialized to a meaningful value.
  DCHECK(script1.id() != -1 && script2.id() != -1);
  return script1.id() < script2.id();
}

}  // namespace extensions
