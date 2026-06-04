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

struct Nested
{
    int val = 1;
    double d = 1.0;

    template <class Archive>
    void serialize(Archive & ar)
    {
        ar(cereal::make_nvp("val", val), cereal::make_nvp("d", d));
    }

    bool operator==(Nested const & other) const
    {
        return val == other.val && std::abs(d - other.d) < 1e-6;
    }
};

struct MyStruct
{
    int x = 2;
    std::string s = "original";
    Nested nested;
    std::vector<int> v = {10, 20};

    template <class Archive>
    void serialize(Archive & ar)
    {
        ar(cereal::make_nvp("x", x),
           cereal::make_nvp("s", s),
           cereal::make_nvp("nested", nested),
           cereal::make_nvp("v", v));
    }
};

struct FewerKeys
{
    int x = 42;

    template <class Archive>
    void serialize(Archive & ar)
    {
        ar(cereal::make_nvp("x", x));
    }
};

struct MoreKeys
{
    int x = 100;
    std::string s = "hello";
    Nested nested{10, 20.0};
    std::vector<int> v = {5, 6};
    int extra1 = 999;
    std::string extra2 = "ignored";
    Nested extra_nested{99, 99.0};

    template <class Archive>
    void serialize(Archive & ar)
    {
        ar(cereal::make_nvp("x", x),
           cereal::make_nvp("s", s),
           cereal::make_nvp("nested", nested),
           cereal::make_nvp("v", v),
           cereal::make_nvp("extra1", extra1),
           cereal::make_nvp("extra2", extra2),
           cereal::make_nvp("extra_nested", extra_nested));
    }
};

TEST_SUITE_BEGIN("missing_unused_keys");

TEST_CASE("cbor_missing_keys")
{
    std::ostringstream os;
    {
        cereal::CborOutputArchive oar(os);
        FewerKeys fk;
        oar(fk);
    }

    std::istringstream is(os.str());
    {
        cereal::CborInputArchive iar(is);
        MyStruct ms;
        // Verify original values before deserialization
        CHECK_EQ(ms.x, 2);
        CHECK_EQ(ms.s, "original");
        CHECK_EQ(ms.nested.val, 1);
        CHECK_EQ(ms.nested.d, 1.0);
        CHECK_EQ(ms.v, std::vector<int>({10, 20}));

        // Deserialize from Cbor that only has "x"
        iar(ms);

        // "x" should be updated to 42, others must remain untouched
        CHECK_EQ(ms.x, 42);
        CHECK_EQ(ms.s, "original");
        CHECK_EQ(ms.nested.val, 1);
        CHECK_EQ(ms.nested.d, 1.0);
        CHECK_EQ(ms.v, std::vector<int>({10, 20}));
    }
}

TEST_CASE("cbor_unused_keys")
{
    std::ostringstream os;
    {
        cereal::CborOutputArchive oar(os);
        MoreKeys mk;
        oar(mk);
    }

    std::istringstream is(os.str());
    {
        cereal::CborInputArchive iar(is);
        MyStruct ms;

        // Deserialize from Cbor that has extra/unused keys
        iar(ms);

        // "x", "s", "nested", "v" should be loaded correctly, extra keys ignored
        CHECK_EQ(ms.x, 100);
        CHECK_EQ(ms.s, "hello");
        CHECK_EQ(ms.nested.val, 10);
        CHECK_EQ(ms.nested.d, 20.0);
        CHECK_EQ(ms.v, std::vector<int>({5, 6}));
    }
}

TEST_SUITE_END();
