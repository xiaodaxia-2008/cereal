#include "Derived.h"

#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/array.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/details/polymorphic_impl.hpp>


DerivedNode::~DerivedNode() {}

void DerivedNode::serialize(auto &ar, const unsigned int)
{
    ar(cereal::base_class<BaseNode>(this), m_position);
}

template DUMMY_LIB_DERIVED_EXPORT void
DerivedNode::serialize<cereal::JSONOutputArchive>(cereal::JSONOutputArchive &ar,
                                                  const unsigned int);

template DUMMY_LIB_DERIVED_EXPORT void
DerivedNode::serialize<cereal::BinaryOutputArchive>(
    cereal::BinaryOutputArchive &ar, const unsigned int);

template DUMMY_LIB_DERIVED_EXPORT void
DerivedNode::serialize<cereal::JSONInputArchive>(cereal::JSONInputArchive &ar,
                                                 const unsigned int);

template DUMMY_LIB_DERIVED_EXPORT void
DerivedNode::serialize<cereal::BinaryInputArchive>(
    cereal::BinaryInputArchive &ar, const unsigned int);

namespace cereal
{
namespace detail
{
template <>
struct binding_name<DerivedNode> {
    static constexpr char const *name() { return "DerivedNode"; }
};

static int dummy = []() {
    polymorphic_serialization_support<BinaryOutputArchive, DerivedNode>()
        .instantiate();
    polymorphic_serialization_support<BinaryInputArchive, DerivedNode>()
        .instantiate();

    RegisterPolymorphicCaster<BaseNode, DerivedNode>::bind();
    return 0;
}();

} // namespace detail
} // namespace cereal
