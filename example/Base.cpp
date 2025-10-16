#include "Base.h"

#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

BaseNode::~BaseNode() {}

void BaseNode::serialize(auto &ar, const unsigned int /*version*/)
{
    ar(m_name, m_children, m_parent);
}

template DUMMY_LIB_BASE_EXPORT void
BaseNode::serialize<cereal::JSONOutputArchive>(cereal::JSONOutputArchive &,
                                               const unsigned int);

template DUMMY_LIB_BASE_EXPORT void
BaseNode::serialize<cereal::BinaryOutputArchive>(cereal::BinaryOutputArchive &,
                                                 const unsigned int);

template DUMMY_LIB_BASE_EXPORT void
BaseNode::serialize<cereal::JSONInputArchive>(cereal::JSONInputArchive &,
                                              const unsigned int);

template DUMMY_LIB_BASE_EXPORT void
BaseNode::serialize<cereal::BinaryInputArchive>(cereal::BinaryInputArchive &,
                                                const unsigned int);