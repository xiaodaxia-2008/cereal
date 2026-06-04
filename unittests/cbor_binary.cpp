/*
  Copyright (c) 2014, Randolph Voorhies, Shane Grant
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
#include <array>
#include <cstring>

// ---------------------------------------------------------------------------
// Helper: round-trip bytes through CBOR and verify they are identical.
// ---------------------------------------------------------------------------
static void roundtrip_bytes(const std::vector<uint8_t> &original)
{
    std::ostringstream os;
    {
        cereal::CborOutputArchive oar(os);
        oar(cereal::binary_data(original.data(), original.size()));
    }

    std::istringstream is(os.str());
    std::vector<uint8_t> loaded(original.size());
    {
        cereal::CborInputArchive iar(is);
        iar(cereal::binary_data(loaded.data(), loaded.size()));
    }

    CHECK_EQ(loaded, original);
}

// ---------------------------------------------------------------------------
// Struct that embeds a binary blob alongside normal fields.
// ---------------------------------------------------------------------------
struct BlobStruct
{
    int id = 0;
    std::vector<uint8_t> payload;

    template <class Archive>
    void serialize(Archive &ar)
    {
        ar(cereal::make_nvp("id", id));
        // We must store the size separately so the loader knows how many bytes to read.
        uint32_t sz = static_cast<uint32_t>(payload.size());
        ar(cereal::make_nvp("size", sz));
        if (sz > 0) {
            payload.resize(sz);
            ar(cereal::make_nvp("data", cereal::binary_data(payload.data(), sz)));
        }
    }
};

TEST_SUITE_BEGIN("cbor_binary");

// ---------------------------------------------------------------------------
// 1. Direct cereal::binary_data round-trip
// ---------------------------------------------------------------------------
TEST_CASE("cbor_binary_data_roundtrip")
{
    // Empty buffer
    roundtrip_bytes({});

    // Small buffer
    roundtrip_bytes({0x01, 0x02, 0x03, 0xFF});

    // Larger buffer (256 bytes, triggering the uint8_follows encoding branch)
    std::vector<uint8_t> large(256);
    for (size_t i = 0; i < 256; ++i)
        large[i] = static_cast<uint8_t>(i);
    roundtrip_bytes(large);
}

// ---------------------------------------------------------------------------
// 2. std::vector<int> uses the fast binary path (BinaryData is serializable).
// ---------------------------------------------------------------------------
TEST_CASE("cbor_vector_arithmetic_binary_path")
{
    static_assert(
        cereal::traits::is_output_serializable<
            cereal::BinaryData<int *>, cereal::CborOutputArchive>::value,
        "CborOutputArchive must support BinaryData for arithmetic types");

    const std::vector<int> orig = {10, 20, 30, -1, 0, 127};
    std::vector<int> loaded;

    std::ostringstream os;
    {
        cereal::CborOutputArchive oar(os);
        oar(orig);
    }

    std::istringstream is(os.str());
    {
        cereal::CborInputArchive iar(is);
        iar(loaded);
    }

    CHECK_EQ(loaded, orig);
}

// ---------------------------------------------------------------------------
// 3. std::array<float, N> uses the fast binary path.
// ---------------------------------------------------------------------------
TEST_CASE("cbor_array_float_binary_path")
{
    const std::array<float, 4> orig = {1.0f, 2.5f, -3.14f, 0.0f};
    std::array<float, 4> loaded{};

    std::ostringstream os;
    {
        cereal::CborOutputArchive oar(os);
        oar(orig);
    }

    std::istringstream is(os.str());
    {
        cereal::CborInputArchive iar(is);
        iar(loaded);
    }

    for (size_t i = 0; i < orig.size(); ++i)
        CHECK(std::abs(loaded[i] - orig[i]) < 1e-6f);
}

// ---------------------------------------------------------------------------
// 4. Struct with embedded binary blob round-trips correctly.
// ---------------------------------------------------------------------------
TEST_CASE("cbor_struct_with_blob")
{
    BlobStruct orig;
    orig.id = 42;
    orig.payload = {0xDE, 0xAD, 0xBE, 0xEF};

    std::ostringstream os;
    {
        cereal::CborOutputArchive oar(os);
        oar(orig);
    }

    BlobStruct loaded;
    std::istringstream is(os.str());
    {
        cereal::CborInputArchive iar(is);
        iar(loaded);
    }

    CHECK_EQ(loaded.id, orig.id);
    CHECK_EQ(loaded.payload, orig.payload);
}

// ---------------------------------------------------------------------------
// 5. Missing key for a binary_data NVP leaves the destination untouched.
// ---------------------------------------------------------------------------
struct WithBlob
{
    int x = 7;
    std::vector<uint8_t> blob = {0xAB, 0xCD};

    template <class Archive>
    void serialize(Archive &ar)
    {
        ar(cereal::make_nvp("x", x));
        uint32_t sz = static_cast<uint32_t>(blob.size());
        ar(cereal::make_nvp("size", sz));
        if (sz > 0) {
            blob.resize(sz);
            ar(cereal::make_nvp("blob", cereal::binary_data(blob.data(), sz)));
        }
    }
};

struct WithoutBlob
{
    int x = 99;

    template <class Archive>
    void serialize(Archive &ar)
    {
        ar(cereal::make_nvp("x", x));
    }
};

TEST_CASE("cbor_missing_binary_key")
{
    // Serialize only WithoutBlob (no "size" or "blob" key)
    std::ostringstream os;
    {
        cereal::CborOutputArchive oar(os);
        WithoutBlob wb;
        oar(wb);
    }

    // Deserialize into WithBlob - "size" is missing so we skip the blob load
    std::istringstream is(os.str());
    WithBlob wb;
    wb.x = 7;
    wb.blob = {0xAB, 0xCD};
    {
        cereal::CborInputArchive iar(is);
        iar(wb);
    }

    CHECK_EQ(wb.x, 99);               // Updated from CBOR
    CHECK_EQ(wb.blob, std::vector<uint8_t>({0xAB, 0xCD})); // Untouched
}

TEST_SUITE_END();
