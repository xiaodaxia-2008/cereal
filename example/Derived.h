#pragma once
#include "Base.h"

#include <dummy_lib_derived_export.h>


#include <array>
#include <span>

class DUMMY_LIB_DERIVED_EXPORT DerivedNode : public BaseNode
{
public:
    DerivedNode() = default;

    DerivedNode(std::string name, std::array<float, 3> position)
        : BaseNode(std::move(name)), m_position(std::move(position))
    {
    }

    const std::array<float, 3> &GetPosition() const { return m_position; }

    void SetPosition(std::array<float, 3> position)
    {
        m_position = std::move(position);
    }

    ~DerivedNode();

    void serialize(auto &ar, const unsigned int version);

    virtual void format(fmt::format_context &ctx) const
    {
        BaseNode::format(ctx);
        fmt::format_to(ctx.out(), ", DerivedNode: position={}", m_position);
    }

protected:
    std::array<float, 3> m_position;
};
