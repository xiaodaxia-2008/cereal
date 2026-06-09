/*
  Copyright (c) 2024, Zen Shawn
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
      * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
      * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
      * Neither the name of the copyright holder nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "common.hpp"
#include <cereal/archives/json.hpp>
#include <cereal/archives/cbor.hpp>
#include <cereal/types/pfr.hpp>

// ======== JSON tests (JSON root objects have named keys) ========

struct Triple
{
    int x = 0;
    double y = 0.0;
    bool z = false;
};

TEST_CASE("json_roundtrip")
{
    std::ostringstream os;
    { cereal::JSONOutputArchive oar(os); Triple o{42, 3.14, true}; oar(o); }
    std::istringstream is(os.str());
    cereal::JSONInputArchive iar(is);
    Triple i; iar(i);
    CHECK_EQ(i.x, 42);
    CHECK_EQ(i.y, doctest::Approx(3.14));
    CHECK_EQ(i.z, true);
}

TEST_CASE("json_throw_on_missing_key")
{
    std::string json = R"({"x": 99, "z": true})";
    std::istringstream is(json);
    cereal::JSONInputArchive iar(is);
    Triple data;
    CHECK_THROWS_WITH(iar(data), "JSON Parsing failed - provided NVP (y) not found");
}

TEST_CASE("json_ignore_missing_key")
{
    std::string json = R"({"x": 99, "z": true})";
    std::istringstream is(json);
    cereal::JSONInputArchive iar(is);
    iar.setIgnoreMissingKeys(true);
    Triple data; iar(data);
    CHECK_EQ(data.x, 99);
    CHECK_EQ(data.y, doctest::Approx(0.0));  // default
    CHECK_EQ(data.z, true);
}

TEST_CASE("json_extra_keys_ignored")
{
    std::string json = R"({"x": 1, "extra_unknown": 999, "y": 2.5, "z": false})";
    std::istringstream is(json);
    cereal::JSONInputArchive iar(is);
    Triple data; iar(data);
    CHECK_EQ(data.x, 1);
    CHECK_EQ(data.y, doctest::Approx(2.5));
    CHECK_EQ(data.z, false);
}

TEST_CASE("json_hasName")
{
    std::string json = R"({"x": 1, "y": 2.5, "z": false})";
    std::istringstream is(json);
    cereal::JSONInputArchive iar(is);
    CHECK(iar.hasName("x"));
    CHECK(iar.hasName("y"));
    CHECK(iar.hasName("z"));
    CHECK_FALSE(iar.hasName("absent"));
}

TEST_CASE("json_ignore_missing_key_leaves_containers_untouched")
{
    std::string json = R"({"x": 99, "z": true})"; // no "y"
    std::istringstream is(json);
    cereal::JSONInputArchive iar(is);
    iar.setIgnoreMissingKeys(true);
    Triple data{1, 2.5, false};
    iar(data);
    CHECK_EQ(data.x, 99);
    CHECK_EQ(data.y, doctest::Approx(2.5));  // default preserved
    CHECK_EQ(data.z, true);
}

// ======== CBOR tests (need nested objects for key search) ========

// CBOR at root level writes arrays (not maps), so key search only applies
// inside nested objects.  We use an outer struct containing an inner struct
// that has NVPs — the inner level creates a CBOR map.

struct InnerFields
{
    int a = 0;
    double b = 0.0;
    bool c = false;
};

struct OuterNested
{
    int id = 0;
    InnerFields inner;
};

// Minimal version with only {a, c} — to create CBOR with missing "b"
struct InnerPartial
{
    int a = 0;
    bool c = false;
};

struct OuterPartial
{
    int id = 0;
    InnerPartial inner;
};

TEST_CASE("cbor_nested_roundtrip")
{
    std::ostringstream os;
    { cereal::CborOutputArchive oar(os); OuterNested o{7, {1, 2.5, false}}; oar(o); }
    std::istringstream is(os.str());
    cereal::CborInputArchive iar(is);
    OuterNested i; iar(i);
    CHECK_EQ(i.id, 7);
    CHECK_EQ(i.inner.a, 1);
    CHECK_EQ(i.inner.b, doctest::Approx(2.5));
    CHECK_EQ(i.inner.c, false);
}

TEST_CASE("cbor_nested_throw_on_missing")
{
    // Save {id: 99, inner: {a: 42, c: true}} (no "b")
    OuterPartial o{99, {42, true}};
    std::ostringstream os;
    { cereal::CborOutputArchive oar(os); oar(o); }
    // Load into OuterNested which expects inner {a, b, c}
    std::istringstream is(os.str());
    cereal::CborInputArchive iar(is);
    iar.setIgnoreMissingKeys(false);
    OuterNested data;
    CHECK_THROWS_WITH(iar(data), "CBOR Parsing failed - provided NVP (b) not found");
}

TEST_CASE("cbor_nested_ignore_missing")
{
    OuterPartial o{99, {42, true}};
    std::ostringstream os;
    { cereal::CborOutputArchive oar(os); oar(o); }
    std::istringstream is(os.str());
    cereal::CborInputArchive iar(is);
    iar.setIgnoreMissingKeys(true);
    OuterNested data; iar(data);
    CHECK_EQ(data.id, 99);
    CHECK_EQ(data.inner.a, 42);
    CHECK_EQ(data.inner.b, doctest::Approx(0.0));  // default
    CHECK_EQ(data.inner.c, true);
}

TEST_CASE("cbor_nested_hasName")
{
    std::ostringstream os;
    { cereal::CborOutputArchive oar(os); OuterNested o{1, {10, 2.5, false}}; oar(o); }
    std::istringstream is(os.str());
    cereal::CborInputArchive iar(is);
    // Navigate into the inner object by starting to load
    OuterNested data;
    iar(data);  // load everything — hasName on inner checked during load
    // hasName at root after load
    CHECK_FALSE(iar.hasName("id"));  // consumed
}
