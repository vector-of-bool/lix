#ifndef LET_PARSER_NODE_HPP_INCLUDED
#define LET_PARSER_NODE_HPP_INCLUDED

#include <let/symbol.hpp>
#include <let/variant.hpp>

#include <ostream>
#include <string>
#include <vector>

namespace let::ast {

class node;

struct list {
    std::vector<node> nodes;

    list() = default;

    explicit list(std::vector<node> ns)
        : nodes(std::move(ns)) {}
};

struct tuple {
    std::vector<node> nodes;

    tuple() = default;

    explicit tuple(std::vector<node> ns)
        : nodes(std::move(ns)) {}
};

using integer  = std::int64_t;
using floating = double;
using symbol   = let::symbol;

class meta {};

struct call {
    std::shared_ptr<node> _target;
    struct meta           _meta;
    std::shared_ptr<node> _arguments;

public:
    inline call(node el, meta m, node args);

    const node& target() const { return *_target; }

    const struct meta& meta() const { return _meta; }

    const node& arguments() const { return *_arguments; }
};

class node {
    using node_var = std::variant<list, tuple, integer, floating, symbol, call>;
    std::unique_ptr<node_var> _var;

public:
    node(node&&)  = default;
    node& operator=(node&&) = default;
    node(list l)
        : _var(std::make_unique<node_var>(std::move(l))) {}
    node(tuple t)
        : _var(std::make_unique<node_var>(std::move(t))) {}
    explicit node(integer i)
        : _var(std::make_unique<node_var>(std::move(i))) {}
    explicit node(floating f)
        : _var(std::make_unique<node_var>(std::move(f))) {}
    node(symbol s)
        : _var(std::make_unique<node_var>(std::move(s))) {}
    node(call c)
        : _var(std::make_unique<node_var>(std::move(c))) {}

#define DEF_OBS(type)                                                                              \
    std::optional<std::reference_wrapper<const type>> as_##type() const {                          \
        auto ptr = std::get_if<type>(_var.get());                                                  \
        if (ptr == nullptr)                                                                        \
            return std::nullopt;                                                                   \
        return *ptr;                                                                               \
    }                                                                                              \
    std::optional<std::reference_wrapper<type>> as_##type() {                                      \
        auto ptr = std::get_if<type>(_var.get());                                                  \
        if (ptr == nullptr)                                                                        \
            return std::nullopt;                                                                   \
        return *ptr;                                                                               \
    }                                                                                              \
    static_assert(true)

    DEF_OBS(list);
    DEF_OBS(tuple);
    DEF_OBS(integer);
    DEF_OBS(floating);
    DEF_OBS(symbol);
    DEF_OBS(call);
#undef DEF_OBS

    template <typename Fun>
    decltype(auto) visit(Fun&& fn) const {
        return std::visit(std::forward<Fun>(fn), *_var);
    }
};

std::ostream& operator<<(std::ostream& o, const node& n);
std::string to_string(const node& n);

call::call(node el, let::ast::meta m, node args)
    : _target(std::make_shared<node>(std::move(el)))
    , _meta(m)
    , _arguments(std::make_shared<node>(std::move(args))) {}

}  // namespace let::ast

#endif  // LET_PARSER_NODE_HPP_INCLUDED