#include "Derived.h"

#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/archives/xml.hpp>
#include <cereal/archives/portable_binary.hpp>
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

CEREAL_REGISTER_TYPE(DerivedNode)
CEREAL_REGISTER_POLYMORPHIC_RELATION(BaseNode, DerivedNode)
