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
#include <cereal/types/pfr.hpp>

// Aggregate types for testing -- no serialize/save/load needed
struct EmptyAggregate {};

struct SimpleAggregate
{
    int x;
    double y;
    std::string name;
    bool flag;

    bool operator==(const SimpleAggregate& other) const
    {
        return x == other.x && y == other.y && name == other.name && flag == other.flag;
    }
};

struct NestedAggregate
{
    SimpleAggregate inner;
    float scale;
    std::vector<int> values;

    bool operator==(const NestedAggregate& other) const
    {
        return inner == other.inner && scale == other.scale && values == other.values;
    }
};

// An aggregate that has its own serialize -- PFR should NOT interfere
struct OwnSerializeAggregate
{
    int a, b;
    bool pfr_used = false;

    template <class Archive>
    void serialize(Archive& ar)
    {
        ar(a, b);
        pfr_used = false;
    }

    bool operator==(const OwnSerializeAggregate& other) const
    {
        return a == other.a && b == other.b;
    }
};

// An aggregate that has a non-member (ADL) serialize -- PFR should NOT interfere
struct NonMemberSerializeAggregate
{
    int a, b;

    bool operator==(const NonMemberSerializeAggregate& other) const
    {
        return a == other.a && b == other.b;
    }
};

template <class Archive>
void serialize(Archive& ar, NonMemberSerializeAggregate& v)
{
    ar(cereal::make_nvp("a", v.a), cereal::make_nvp("b", v.b));
}

// An aggregate that uses free save/load split -- PFR should NOT interfere
struct FreeSaveLoadAggregate
{
    int a, b;
    std::string s;

    bool operator==(const FreeSaveLoadAggregate& other) const
    {
        return a == other.a && b == other.b && s == other.s;
    }
};

template <class Archive>
void save(Archive& ar, const FreeSaveLoadAggregate& v)
{
    ar(cereal::make_nvp("a", v.a), cereal::make_nvp("b", v.b), cereal::make_nvp("s", v.s));
}

template <class Archive>
void load(Archive& ar, FreeSaveLoadAggregate& v)
{
    ar(cereal::make_nvp("a", v.a), cereal::make_nvp("b", v.b), cereal::make_nvp("s", v.s));
}

// An aggregate that uses free save_minimal/load_minimal split -- PFR should NOT interfere
struct FreeSaveLoadMinimalAggregate
{
    int a, b;
    std::string s;

    bool operator==(const FreeSaveLoadMinimalAggregate& other) const
    {
        return a == other.a && b == other.b && s == other.s;
    }
};

template <class Archive>
std::string save_minimal(Archive const&, FreeSaveLoadMinimalAggregate const& v)
{
    return std::to_string(v.a) + "," + std::to_string(v.b) + "," + v.s;
}

template <class Archive>
void load_minimal(Archive const&, FreeSaveLoadMinimalAggregate& v, std::string const& s)
{
    auto p1 = s.find(',');
    auto p2 = s.find(',', p1 == std::string::npos ? 0 : p1 + 1);
    v.a = std::stoi(s.substr(0, p1));
    v.b = std::stoi(s.substr(p1 + 1, p2 - p1 - 1));
    v.s = s.substr(p2 + 1);
}

std::ostream& operator<<(std::ostream& os, const SimpleAggregate& s)
{
    os << "[" << s.x << ", " << s.y << ", " << s.name << ", " << s.flag << "]";
    return os;
}

std::ostream& operator<<(std::ostream& os, const NestedAggregate& n)
{
    os << "[inner=" << n.inner << ", scale=" << n.scale << ", values=" << n.values.size() << "]";
    return os;
}

std::ostream& operator<<(std::ostream& os, const OwnSerializeAggregate& o)
{
    os << "[" << o.a << ", " << o.b << "]";
    return os;
}

std::ostream& operator<<(std::ostream& os, const NonMemberSerializeAggregate& o)
{
    os << "[" << o.a << ", " << o.b << "]";
    return os;
}

std::ostream& operator<<(std::ostream& os, const FreeSaveLoadAggregate& o)
{
    os << "[" << o.a << ", " << o.b << ", " << o.s << "]";
    return os;
}

std::ostream& operator<<(std::ostream& os, const FreeSaveLoadMinimalAggregate& o)
{
    os << "[" << o.a << ", " << o.b << ", " << o.s << "]";
    return os;
}

template <class IArchive, class OArchive>
void test_pfr_simple()
{
    std::random_device rd;
    std::mt19937 gen(rd());

    for (int ii = 0; ii < 100; ++ii)
    {
        SimpleAggregate o_data = {
            random_value<int>(gen),
            random_value<double>(gen),
            random_value<std::string>(gen),
            random_value<int>(gen) % 2 == 0
        };

        std::ostringstream os;
        {
            OArchive oar(os);
            oar(o_data);
        }

        SimpleAggregate i_data = {};
        std::istringstream is(os.str());
        {
            IArchive iar(is);
            iar(i_data);
        }

        CHECK_EQ(i_data, o_data);
    }
}

template <class IArchive, class OArchive>
void test_pfr_nested()
{
    std::random_device rd;
    std::mt19937 gen(rd());

    for (int ii = 0; ii < 100; ++ii)
    {
        SimpleAggregate inner = {
            random_value<int>(gen),
            random_value<double>(gen),
            random_value<std::string>(gen),
            random_value<int>(gen) % 2 == 0
        };

        std::vector<int> vals(random_index(0, 5, gen));
        for (auto& v : vals) v = random_value<int>(gen);

        NestedAggregate o_data = { inner, random_value<float>(gen), vals };

        std::ostringstream os;
        {
            OArchive oar(os);
            oar(o_data);
        }

        NestedAggregate i_data;
        std::istringstream is(os.str());
        {
            IArchive iar(is);
            iar(i_data);
        }

        CHECK_EQ(i_data.inner, o_data.inner);
        CHECK_EQ(i_data.scale, doctest::Approx(o_data.scale));
        CHECK_EQ(i_data.values.size(), o_data.values.size());
        for (size_t i = 0; i < o_data.values.size(); ++i)
            CHECK_EQ(i_data.values[i], o_data.values[i]);
    }
}

template <class IArchive, class OArchive>
void test_pfr_empty()
{
    EmptyAggregate o_data{};
    std::ostringstream os;
    {
        OArchive oar(os);
        oar(o_data);
    }

    EmptyAggregate i_data{};
    std::istringstream is(os.str());
    {
        IArchive iar(is);
        iar(i_data);
    }
}

template <class IArchive, class OArchive>
void test_pfr_no_interference()
{
    std::random_device rd;
    std::mt19937 gen(rd());

    for (int ii = 0; ii < 100; ++ii)
    {
        OwnSerializeAggregate o_data = { random_value<int>(gen), random_value<int>(gen), true };

        std::ostringstream os;
        {
            OArchive oar(os);
            oar(o_data);
        }

        OwnSerializeAggregate i_data = {};
        std::istringstream is(os.str());
        {
            IArchive iar(is);
            iar(i_data);
        }

        CHECK_EQ(i_data, o_data);
    }
}

template <class IArchive, class OArchive>
void test_pfr_no_interference_non_member()
{
    std::random_device rd;
    std::mt19937 gen(rd());

    for (int ii = 0; ii < 100; ++ii)
    {
        NonMemberSerializeAggregate o_data = {
            random_value<int>(gen), random_value<int>(gen)
        };

        std::ostringstream os;
        {
            OArchive oar(os);
            oar(o_data);
        }

        NonMemberSerializeAggregate i_data = {};
        std::istringstream is(os.str());
        {
            IArchive iar(is);
            iar(i_data);
        }

        CHECK_EQ(i_data, o_data);
    }
}

template <class IArchive, class OArchive>
void test_pfr_no_interference_free_save_load()
{
    std::random_device rd;
    std::mt19937 gen(rd());

    for (int ii = 0; ii < 100; ++ii)
    {
        FreeSaveLoadAggregate o_data = {
            random_value<int>(gen), random_value<int>(gen),
            random_value<std::string>(gen)
        };

        std::ostringstream os;
        {
            OArchive oar(os);
            oar(o_data);
        }

        FreeSaveLoadAggregate i_data = {};
        std::istringstream is(os.str());
        {
            IArchive iar(is);
            iar(i_data);
        }

        CHECK_EQ(i_data, o_data);
    }
}

template <class IArchive, class OArchive>
void test_pfr_no_interference_free_save_load_minimal()
{
    std::random_device rd;
    std::mt19937 gen(rd());

    for (int ii = 0; ii < 100; ++ii)
    {
        FreeSaveLoadMinimalAggregate o_data = {
            random_value<int>(gen), random_value<int>(gen),
            random_value<std::string>(gen)
        };

        std::ostringstream os;
        {
            OArchive oar(os);
            oar(o_data);
        }

        FreeSaveLoadMinimalAggregate i_data = {};
        std::istringstream is(os.str());
        {
            IArchive iar(is);
            iar(i_data);
        }

        CHECK_EQ(i_data, o_data);
    }
}

TEST_SUITE_BEGIN("pfr");

TEST_CASE("binary_pfr_simple")
{
    test_pfr_simple<cereal::BinaryInputArchive, cereal::BinaryOutputArchive>();
}

TEST_CASE("portable_binary_pfr_simple")
{
    test_pfr_simple<cereal::PortableBinaryInputArchive, cereal::PortableBinaryOutputArchive>();
}

TEST_CASE("xml_pfr_simple")
{
    test_pfr_simple<cereal::XMLInputArchive, cereal::XMLOutputArchive>();
}

TEST_CASE("json_pfr_simple")
{
    test_pfr_simple<cereal::JSONInputArchive, cereal::JSONOutputArchive>();
}

TEST_CASE("cbor_pfr_simple")
{
    test_pfr_simple<cereal::CborInputArchive, cereal::CborOutputArchive>();
}

TEST_CASE("binary_pfr_nested")
{
    test_pfr_nested<cereal::BinaryInputArchive, cereal::BinaryOutputArchive>();
}

TEST_CASE("xml_pfr_nested")
{
    test_pfr_nested<cereal::XMLInputArchive, cereal::XMLOutputArchive>();
}

TEST_CASE("json_pfr_nested")
{
    test_pfr_nested<cereal::JSONInputArchive, cereal::JSONOutputArchive>();
}

TEST_CASE("cbor_pfr_nested")
{
    test_pfr_nested<cereal::CborInputArchive, cereal::CborOutputArchive>();
}

TEST_CASE("binary_pfr_empty")
{
    test_pfr_empty<cereal::BinaryInputArchive, cereal::BinaryOutputArchive>();
}

TEST_CASE("json_pfr_empty")
{
    test_pfr_empty<cereal::JSONInputArchive, cereal::JSONOutputArchive>();
}

TEST_CASE("binary_pfr_no_interference")
{
    test_pfr_no_interference<cereal::BinaryInputArchive, cereal::BinaryOutputArchive>();
}

TEST_CASE("json_pfr_no_interference")
{
    test_pfr_no_interference<cereal::JSONInputArchive, cereal::JSONOutputArchive>();
}

TEST_CASE("cbor_pfr_no_interference")
{
    test_pfr_no_interference<cereal::CborInputArchive, cereal::CborOutputArchive>();
}

TEST_CASE("binary_pfr_no_interference_non_member")
{
    test_pfr_no_interference_non_member<cereal::BinaryInputArchive, cereal::BinaryOutputArchive>();
}

TEST_CASE("json_pfr_no_interference_non_member")
{
    test_pfr_no_interference_non_member<cereal::JSONInputArchive, cereal::JSONOutputArchive>();
}

TEST_CASE("cbor_pfr_no_interference_non_member")
{
    test_pfr_no_interference_non_member<cereal::CborInputArchive, cereal::CborOutputArchive>();
}

TEST_CASE("binary_pfr_no_interference_free_save_load")
{
    test_pfr_no_interference_free_save_load<cereal::BinaryInputArchive, cereal::BinaryOutputArchive>();
}

TEST_CASE("json_pfr_no_interference_free_save_load")
{
    test_pfr_no_interference_free_save_load<cereal::JSONInputArchive, cereal::JSONOutputArchive>();
}

TEST_CASE("cbor_pfr_no_interference_free_save_load")
{
    test_pfr_no_interference_free_save_load<cereal::CborInputArchive, cereal::CborOutputArchive>();
}

TEST_CASE("binary_pfr_no_interference_free_save_load_minimal")
{
    test_pfr_no_interference_free_save_load_minimal<cereal::BinaryInputArchive, cereal::BinaryOutputArchive>();
}

TEST_CASE("json_pfr_no_interference_free_save_load_minimal")
{
    test_pfr_no_interference_free_save_load_minimal<cereal::JSONInputArchive, cereal::JSONOutputArchive>();
}

TEST_CASE("cbor_pfr_no_interference_free_save_load_minimal")
{
    test_pfr_no_interference_free_save_load_minimal<cereal::CborInputArchive, cereal::CborOutputArchive>();
}

TEST_SUITE_END();
