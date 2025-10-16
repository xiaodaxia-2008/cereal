#pragma once
#include <dummy_lib_base_export.h>

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <fmt/ranges.h>

class DUMMY_LIB_BASE_EXPORT BaseNode
    : public std::enable_shared_from_this<BaseNode>
{

public:
    BaseNode() = default;

    BaseNode(std::string name) : m_name(std::move(name)) {}

    virtual ~BaseNode();

    std::shared_ptr<BaseNode> GetParent() const { return m_parent.lock(); }

    void SetName(std::string name) { m_name = std::move(name); }

    std::string_view GetName() const { return m_name; }

    void AddChild(std::shared_ptr<BaseNode> child)
    {
        if (std::ranges::find(m_children, child) == m_children.end()) {
            child->m_parent = shared_from_this();
            m_children.push_back(std::move(child));
        }
    }

    void RemoveChild(const std::shared_ptr<BaseNode> &child)
    {
        auto it = std::ranges::find(m_children, child);
        if (it != m_children.end()) {
            m_children.erase(it);
            child->m_parent.reset();
        }
    }

    std::shared_ptr<BaseNode> GetChild(std::size_t index)
    {
        return m_children.at(index);
    }

    std::size_t GetChildrenCount() const { return m_children.size(); }

    void serialize(auto &ar, const unsigned int version);

    virtual void format(fmt::format_context &ctx) const
    {
        fmt::format_to(ctx.out(), "BaseNode: Addr={}, Name={}", fmt::ptr(this),
                       GetName());
        fmt::format_to(ctx.out(), ", Children=[");
        for (auto &child : m_children) {
            child->format(ctx);
        }
        fmt::format_to(ctx.out(), "]");
        if (auto parent = GetParent()) {
            fmt::format_to(ctx.out(), ", Parent={}", parent->GetName());
        }
    }

protected:
    std::string m_name{"BaseNode"};

    std::vector<std::shared_ptr<BaseNode>> m_children;

    std::weak_ptr<BaseNode> m_parent;
};

template <std::derived_from<BaseNode> T>
struct fmt::formatter<T> {
    constexpr auto parse(format_parse_context &ctx) { return ctx.begin(); }

    auto format(const BaseNode &b, format_context &ctx) const
    {
        b.format(ctx);
        return ctx.out();
    }
};