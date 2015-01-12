// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/json_web_key.h"

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class JSONWebKeyTest : public testing::Test {
 public:
  JSONWebKeyTest() {}

 protected:
  void ExtractJWKKeysAndExpect(const std::string& jwk,
                               bool expected_result,
                               size_t expected_number_of_keys) {
    DCHECK(!jwk.empty());
    KeyIdAndKeyPairs keys;
    MediaKeys::SessionType session_type;
    EXPECT_EQ(expected_result,
              ExtractKeysFromJWKSet(jwk, &keys, &session_type));
    EXPECT_EQ(expected_number_of_keys, keys.size());
  }

  void ExtractSessionTypeAndExpect(const std::string& jwk,
                                   bool expected_result,
                                   MediaKeys::SessionType expected_type) {
    DCHECK(!jwk.empty());
    KeyIdAndKeyPairs keys;
    MediaKeys::SessionType session_type;
    EXPECT_EQ(expected_result,
              ExtractKeysFromJWKSet(jwk, &keys, &session_type));
    if (expected_result) {
      // Only check if successful.
      EXPECT_EQ(expected_type, session_type);
    }
  }

  void CreateLicenseAndExpect(const uint8* key_id,
                              int key_id_length,
                              MediaKeys::SessionType session_type,
                              const std::string& expected_result) {
    std::vector<uint8> result;
    CreateLicenseRequest(key_id, key_id_length, session_type, &result);
    std::string s(result.begin(), result.end());
    EXPECT_EQ(expected_result, s);
  }

  void ExtractKeyFromLicenseAndExpect(const std::string& license,
                                      bool expected_result,
                                      const uint8* expected_key,
                                      int expected_key_length) {
    std::vector<uint8> license_vector(license.begin(), license.end());
    std::vector<uint8> key;
    EXPECT_EQ(expected_result,
              ExtractFirstKeyIdFromLicenseRequest(license_vector, &key));
    if (expected_result) {
      std::vector<uint8> key_result(expected_key,
                                    expected_key + expected_key_length);
      EXPECT_EQ(key_result, key);
    }
  }
};

TEST_F(JSONWebKeyTest, GenerateJWKSet) {
  const uint8 data1[] = { 0x01, 0x02 };
  const uint8 data2[] = { 0x01, 0x02, 0x03, 0x04 };
  const uint8 data3[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                          0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10 };

  EXPECT_EQ(
      "{\"keys\":[{\"alg\":\"A128KW\",\"k\":\"AQI\",\"kid\":\"AQI\",\"kty\":"
      "\"oct\"}]}",
      GenerateJWKSet(data1, arraysize(data1), data1, arraysize(data1)));
  EXPECT_EQ(
      "{\"keys\":[{\"alg\":\"A128KW\",\"k\":\"AQIDBA\",\"kid\":\"AQIDBA\","
      "\"kty\":\"oct\"}]}",
      GenerateJWKSet(data2, arraysize(data2), data2, arraysize(data2)));
  EXPECT_EQ(
      "{\"keys\":[{\"alg\":\"A128KW\",\"k\":\"AQI\",\"kid\":\"AQIDBA\",\"kty\":"
      "\"oct\"}]}",
      GenerateJWKSet(data1, arraysize(data1), data2, arraysize(data2)));
  EXPECT_EQ(
      "{\"keys\":[{\"alg\":\"A128KW\",\"k\":\"AQIDBA\",\"kid\":\"AQI\",\"kty\":"
      "\"oct\"}]}",
      GenerateJWKSet(data2, arraysize(data2), data1, arraysize(data1)));
  EXPECT_EQ(
      "{\"keys\":[{\"alg\":\"A128KW\",\"k\":\"AQIDBAUGBwgJCgsMDQ4PEA\",\"kid\":"
      "\"AQIDBAUGBwgJCgsMDQ4PEA\",\"kty\":\"oct\"}]}",
      GenerateJWKSet(data3, arraysize(data3), data3, arraysize(data3)));
}

TEST_F(JSONWebKeyTest, ExtractValidJWKKeys) {
  // Try an empty 'keys' dictionary.
  ExtractJWKKeysAndExpect("{ \"keys\": [] }", true, 0);

  // Try a key list with one entry.
  const std::string kJwksOneEntry =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"alg\": \"A128KW\","
      "      \"kid\": \"AAECAwQFBgcICQoLDA0ODxAREhM\","
      "      \"k\": \"FBUWFxgZGhscHR4fICEiIw\""
      "    }"
      "  ]"
      "}";
  ExtractJWKKeysAndExpect(kJwksOneEntry, true, 1);

  // Try a key list with multiple entries.
  const std::string kJwksMultipleEntries =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"alg\": \"A128KW\","
      "      \"kid\": \"AAECAwQFBgcICQoLDA0ODxAREhM\","
      "      \"k\": \"FBUWFxgZGhscHR4fICEiIw\""
      "    },"
      "    {"
      "      \"kty\": \"oct\","
      "      \"alg\": \"A128KW\","
      "      \"kid\": \"JCUmJygpKissLS4vMA\","
      "      \"k\":\"MTIzNDU2Nzg5Ojs8PT4/QA\""
      "    }"
      "  ]"
      "}";
  ExtractJWKKeysAndExpect(kJwksMultipleEntries, true, 2);

  // Try a key with no spaces and some \n plus additional fields.
  const std::string kJwksNoSpaces =
      "\n\n{\"something\":1,\"keys\":[{\n\n\"kty\":\"oct\",\"alg\":\"A128KW\","
      "\"kid\":\"AAECAwQFBgcICQoLDA0ODxAREhM\",\"k\":\"GawgguFyGrWKav7AX4VKUg"
      "\",\"foo\":\"bar\"}]}\n\n";
  ExtractJWKKeysAndExpect(kJwksNoSpaces, true, 1);

  // Try a list with multiple keys with the same kid.
  const std::string kJwksDuplicateKids =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"alg\": \"A128KW\","
      "      \"kid\": \"JCUmJygpKissLS4vMA\","
      "      \"k\": \"FBUWFxgZGhscHR4fICEiIw\""
      "    },"
      "    {"
      "      \"kty\": \"oct\","
      "      \"alg\": \"A128KW\","
      "      \"kid\": \"JCUmJygpKissLS4vMA\","
      "      \"k\":\"MTIzNDU2Nzg5Ojs8PT4/QA\""
      "    }"
      "  ]"
      "}";
  ExtractJWKKeysAndExpect(kJwksDuplicateKids, true, 2);
}

TEST_F(JSONWebKeyTest, ExtractInvalidJWKKeys) {
  // Try a simple JWK key (i.e. not in a set)
  const std::string kJwkSimple =
      "{"
      "  \"kty\": \"oct\","
      "  \"alg\": \"A128KW\","
      "  \"kid\": \"AAECAwQFBgcICQoLDA0ODxAREhM\","
      "  \"k\": \"FBUWFxgZGhscHR4fICEiIw\""
      "}";
  ExtractJWKKeysAndExpect(kJwkSimple, false, 0);

  // Try some non-ASCII characters.
  ExtractJWKKeysAndExpect(
      "This is not ASCII due to \xff\xfe\xfd in it.", false, 0);

  // Try some non-ASCII characters in an otherwise valid JWK.
  const std::string kJwksInvalidCharacters =
      "\n\n{\"something\":1,\"keys\":[{\n\n\"kty\":\"oct\",\"alg\":\"A128KW\","
      "\"kid\":\"AAECAwQFBgcICQoLDA0ODxAREhM\",\"k\":\"\xff\xfe\xfd"
      "\",\"foo\":\"bar\"}]}\n\n";
  ExtractJWKKeysAndExpect(kJwksInvalidCharacters, false, 0);

  // Try a badly formatted key. Assume that the JSON parser is fully tested,
  // so we won't try a lot of combinations. However, need a test to ensure
  // that the code doesn't crash if invalid JSON received.
  ExtractJWKKeysAndExpect("This is not a JSON key.", false, 0);

  // Try passing some valid JSON that is not a dictionary at the top level.
  ExtractJWKKeysAndExpect("40", false, 0);

  // Try an empty dictionary.
  ExtractJWKKeysAndExpect("{ }", false, 0);

  // Try with 'keys' not a dictionary.
  ExtractJWKKeysAndExpect("{ \"keys\":\"1\" }", false, 0);

  // Try with 'keys' a list of integers.
  ExtractJWKKeysAndExpect("{ \"keys\": [ 1, 2, 3 ] }", false, 0);

  // Try padding(=) at end of 'k' base64 string.
  const std::string kJwksWithPaddedKey =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"alg\": \"A128KW\","
      "      \"kid\": \"AAECAw\","
      "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw==\""
      "    }"
      "  ]"
      "}";
  ExtractJWKKeysAndExpect(kJwksWithPaddedKey, false, 0);

  // Try padding(=) at end of 'kid' base64 string.
  const std::string kJwksWithPaddedKeyId =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"alg\": \"A128KW\","
      "      \"kid\": \"AAECAw==\","
      "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw\""
      "    }"
      "  ]"
      "}";
  ExtractJWKKeysAndExpect(kJwksWithPaddedKeyId, false, 0);

  // Try a key with invalid base64 encoding.
  const std::string kJwksWithInvalidBase64 =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"alg\": \"A128KW\","
      "      \"kid\": \"!@#$%^&*()\","
      "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw\""
      "    }"
      "  ]"
      "}";
  ExtractJWKKeysAndExpect(kJwksWithInvalidBase64, false, 0);

  // Empty key id.
  const std::string kJwksWithEmptyKeyId =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"alg\": \"A128KW\","
      "      \"kid\": \"\","
      "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw\""
      "    }"
      "  ]"
      "}";
  ExtractJWKKeysAndExpect(kJwksWithEmptyKeyId, false, 0);
}

TEST_F(JSONWebKeyTest, KeyType) {
  // Valid key type.
  const std::string kJwksWithValidKeyType =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"alg\": \"A128KW\","
      "      \"kid\": \"AAECAwQFBgcICQoLDA0ODxAREhM\","
      "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw\""
      "    }"
      "  ]"
      "}";
  ExtractJWKKeysAndExpect(kJwksWithValidKeyType, true, 1);

  // Empty key type.
  const std::string kJwksWithEmptyKeyType =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"\","
      "      \"alg\": \"A128KW\","
      "      \"kid\": \"AAECAwQFBgcICQoLDA0ODxAREhM\","
      "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw\""
      "    }"
      "  ]"
      "}";
  ExtractJWKKeysAndExpect(kJwksWithEmptyKeyType, false, 0);

  // Key type is case sensitive.
  const std::string kJwksWithUppercaseKeyType =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"OCT\","
      "      \"alg\": \"A128KW\","
      "      \"kid\": \"AAECAwQFBgcICQoLDA0ODxAREhM\","
      "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw\""
      "    }"
      "  ]"
      "}";
  ExtractJWKKeysAndExpect(kJwksWithUppercaseKeyType, false, 0);

  // Wrong key type.
  const std::string kJwksWithWrongKeyType =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"RSA\","
      "      \"alg\": \"A128KW\","
      "      \"kid\": \"AAECAwQFBgcICQoLDA0ODxAREhM\","
      "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw\""
      "    }"
      "  ]"
      "}";
  ExtractJWKKeysAndExpect(kJwksWithWrongKeyType, false, 0);
}

TEST_F(JSONWebKeyTest, Alg) {
  // Valid alg.
  const std::string kJwksWithValidAlg =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"alg\": \"A128KW\","
      "      \"kid\": \"AAECAwQFBgcICQoLDA0ODxAREhM\","
      "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw\""
      "    }"
      "  ]"
      "}";
  ExtractJWKKeysAndExpect(kJwksWithValidAlg, true, 1);

  // Empty alg.
  const std::string kJwksWithEmptyAlg =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"alg\": \"\","
      "      \"kid\": \"AAECAwQFBgcICQoLDA0ODxAREhM\","
      "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw\""
      "    }"
      "  ]"
      "}";
  ExtractJWKKeysAndExpect(kJwksWithEmptyAlg, false, 0);

  // Alg is case sensitive.
  const std::string kJwksWithLowercaseAlg =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"alg\": \"a128kw\","
      "      \"kid\": \"AAECAwQFBgcICQoLDA0ODxAREhM\","
      "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw\""
      "    }"
      "  ]"
      "}";
  ExtractJWKKeysAndExpect(kJwksWithLowercaseAlg, false, 0);

  // Wrong alg.
  const std::string kJwksWithWrongAlg =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"alg\": \"RS256\","
      "      \"kid\": \"AAECAwQFBgcICQoLDA0ODxAREhM\","
      "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw\""
      "    }"
      "  ]"
      "}";
  ExtractJWKKeysAndExpect(kJwksWithWrongAlg, false, 0);
}

TEST_F(JSONWebKeyTest, SessionType) {
  ExtractSessionTypeAndExpect(
      "{\"keys\":[{\"alg\": "
      "\"A128KW\",\"k\":\"AQI\",\"kid\":\"AQI\",\"kty\":\"oct\"}]}",
      true, MediaKeys::TEMPORARY_SESSION);
  ExtractSessionTypeAndExpect(
      "{\"keys\":[{\"alg\": "
      "\"A128KW\",\"k\":\"AQI\",\"kid\":\"AQI\",\"kty\":\"oct\"}],\"type\":"
      "\"temporary\"}",
      true, MediaKeys::TEMPORARY_SESSION);
  ExtractSessionTypeAndExpect(
      "{\"keys\":[{\"alg\": "
      "\"A128KW\",\"k\":\"AQI\",\"kid\":\"AQI\",\"kty\":\"oct\"}],\"type\":"
      "\"persistent-license\"}",
      true, MediaKeys::PERSISTENT_LICENSE_SESSION);
  ExtractSessionTypeAndExpect(
      "{\"keys\":[{\"alg\": "
      "\"A128KW\",\"k\":\"AQI\",\"kid\":\"AQI\",\"kty\":\"oct\"}],\"type\":"
      "\"persistent-release-message\"}",
      true, MediaKeys::PERSISTENT_RELEASE_MESSAGE_SESSION);
  ExtractSessionTypeAndExpect(
      "{\"keys\":[{\"alg\": "
      "\"A128KW\",\"k\":\"AQI\",\"kid\":\"AQI\",\"kty\":\"oct\"}],\"type\":"
      "\"unknown\"}",
      false, MediaKeys::TEMPORARY_SESSION);
  ExtractSessionTypeAndExpect(
      "{\"keys\":[{\"alg\": "
      "\"A128KW\",\"k\":\"AQI\",\"kid\":\"AQI\",\"kty\":\"oct\"}],\"type\":3}",
      false, MediaKeys::TEMPORARY_SESSION);
}

TEST_F(JSONWebKeyTest, CreateLicense) {
  const uint8 data1[] = { 0x01, 0x02 };
  const uint8 data2[] = { 0x01, 0x02, 0x03, 0x04 };
  const uint8 data3[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                          0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10 };

  CreateLicenseAndExpect(data1,
                         arraysize(data1),
                         MediaKeys::TEMPORARY_SESSION,
                         "{\"kids\":[\"AQI\"],\"type\":\"temporary\"}");
  CreateLicenseAndExpect(
      data1, arraysize(data1), MediaKeys::PERSISTENT_LICENSE_SESSION,
      "{\"kids\":[\"AQI\"],\"type\":\"persistent-license\"}");
  CreateLicenseAndExpect(
      data1, arraysize(data1), MediaKeys::PERSISTENT_RELEASE_MESSAGE_SESSION,
      "{\"kids\":[\"AQI\"],\"type\":\"persistent-release-message\"}");
  CreateLicenseAndExpect(data2,
                         arraysize(data2),
                         MediaKeys::TEMPORARY_SESSION,
                         "{\"kids\":[\"AQIDBA\"],\"type\":\"temporary\"}");
  CreateLicenseAndExpect(data3, arraysize(data3),
                         MediaKeys::PERSISTENT_LICENSE_SESSION,
                         "{\"kids\":[\"AQIDBAUGBwgJCgsMDQ4PEA\"],\"type\":"
                         "\"persistent-license\"}");
}

TEST_F(JSONWebKeyTest, ExtractLicense) {
  const uint8 data1[] = { 0x01, 0x02 };
  const uint8 data2[] = { 0x01, 0x02, 0x03, 0x04 };
  const uint8 data3[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                          0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10 };

  ExtractKeyFromLicenseAndExpect(
      "{\"kids\":[\"AQI\"],\"type\":\"temporary\"}",
      true,
      data1,
      arraysize(data1));
  ExtractKeyFromLicenseAndExpect(
      "{\"kids\":[\"AQIDBA\"],\"type\":\"temporary\"}",
      true,
      data2,
      arraysize(data2));
  ExtractKeyFromLicenseAndExpect(
      "{\"kids\":[\"AQIDBAUGBwgJCgsMDQ4PEA\"],\"type\":\"persistent\"}",
      true,
      data3,
      arraysize(data3));

  // Try some incorrect JSON.
  ExtractKeyFromLicenseAndExpect("", false, NULL, 0);
  ExtractKeyFromLicenseAndExpect("!@#$%^&*()", false, NULL, 0);

  // Valid JSON, but not a dictionary.
  ExtractKeyFromLicenseAndExpect("6", false, NULL, 0);
  ExtractKeyFromLicenseAndExpect("[\"AQI\"]", false, NULL, 0);

  // Dictionary, but missing expected tag.
  ExtractKeyFromLicenseAndExpect("{\"kid\":[\"AQI\"]}", false, NULL, 0);

  // Correct tag, but empty list.
  ExtractKeyFromLicenseAndExpect("{\"kids\":[]}", false, NULL, 0);

  // Correct tag, but list doesn't contain a string.
  ExtractKeyFromLicenseAndExpect("{\"kids\":[[\"AQI\"]]}", false, NULL, 0);

  // Correct tag, but invalid base64 encoding.
  ExtractKeyFromLicenseAndExpect("{\"kids\":[\"!@#$%^&*()\"]}", false, NULL, 0);
}

}  // namespace media

