#include "Derived.h"

#include <spdlog/spdlog.h>

#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/memory.hpp>

#include <filesystem>
#include <fstream>
#include <memory>
#include <ranges>
#include <sstream>

#include <fmt/std.h>

void test_instance()
{

    DerivedNode d1("A", {1, 2, 3});

    spdlog::debug("d1: {}", d1);

    std::stringstream ss;
    {
        cereal::JSONOutputArchive ar(ss);
        ar(d1);
    }

    spdlog::info("serialized result:\n{}", ss.str());

    DerivedNode d2;
    {
        cereal::JSONInputArchive ar(ss);
        ar(d2);
    }
    spdlog::debug("d2: {}", d2);
}

void test_shared_ptr()
{
    std::shared_ptr<BaseNode> node1 = std::make_shared<BaseNode>("Node1");
    std::shared_ptr<BaseNode> node2 =
        std::make_shared<DerivedNode>("Node2", std::array<float, 3>{2, 2, 2});
    node1->AddChild(node2);

    SPDLOG_INFO("node1: {}", *node1);

    std::stringstream ss;
    {
        cereal::BinaryOutputArchive ar(ss);
        ar(node1);
    }

    spdlog::info("serialized result:\n{}", ss.str());
    node1.reset();
    {
        cereal::BinaryInputArchive ar(ss);
        ar(node1);
    }
    SPDLOG_INFO("node1: {}", *node1);
}

struct B { 
    virtual ~B() = default;
};

struct D : public B { 
};

int main()
{
    spdlog::set_level(spdlog::level::debug);
    constexpr auto n = std::is_empty_v<D>;
    constexpr auto n2 = sizeof(D);
    test_shared_ptr();
    return 0;
}