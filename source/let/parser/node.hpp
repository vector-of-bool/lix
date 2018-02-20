#ifndef LET_PARSER_NODE_HPP_INCLUDED
#define LET_PARSER_NODE_HPP_INCLUDED

#include <let/string.hpp>
#include <let/symbol.hpp>
#include <let/util.hpp>
#include <let/util/opt_ref.hpp>
#include <let/value_fwd.hpp>
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
using string   = let::string;

class meta {
    std::optional<std::pair<string, string>> _fn_details;

public:
    meta() = default;

    void set_fn_details(std::string module, std::string name) { _fn_details.emplace(module, name); }

    auto& fn_details() const noexcept { return _fn_details; }
};

class call {
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
    using node_var = std::variant<list, tuple, integer, floating, symbol, string, call>;
    std::shared_ptr<const node_var> _var;

public:
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
    explicit node(string s)
        : _var(std::make_unique<node_var>(std::move(s))) {}

#define DEF_OBS(type)                                                                              \
    opt_ref<const type> as_##type() const& {                                                       \
        if (auto ptr = std::get_if<type>(_var.get())) {                                            \
            return *ptr;                                                                           \
        } else {                                                                                   \
            return nullopt;                                                                        \
        }                                                                                          \
    }                                                                                              \
    opt_ref<const type> as(tag<type>) const& noexcept { return as_##type(); }                      \
    static_assert(true)

    DEF_OBS(list);
    DEF_OBS(tuple);
    DEF_OBS(integer);
    DEF_OBS(floating);
    DEF_OBS(symbol);
    DEF_OBS(string);
    DEF_OBS(call);
#undef DEF_OBS

    template <typename Fun, typename... Args>
    decltype(auto) visit(Fun&& fn, Args&&... args) const {
        return std::visit(
            [&](auto&& item) -> decltype(auto) {
                return std::forward<Fun>(fn)(item, std::forward<Args>(args)...);
            },
            *_var);
    }

    let::value  to_value() const;
    static node from_value(const let::value&);
};

std::ostream& operator<<(std::ostream& o, const node& n);
std::string   to_string(const node& n);

call::call(node el, let::ast::meta m, node args)
    : _target(std::make_shared<node>(std::move(el)))
    , _meta(m)
    , _arguments(std::make_shared<node>(std::move(args))) {}

inline ast::node make_variable(const std::string_view& s) {
    return ast::call(symbol(s), {}, symbol("Var"));
}

template <typename... Args>
ast::list make_list(Args&&... args) {
    return ast::list({ast::node(std::forward<Args>(args))...});
}

inline ast::node make_assignment(const std::string_view& varname, ast::node rhs) {
    return ast::call(symbol("="), {}, ast::list({make_variable(varname), std::move(rhs)}));
}

template <typename... Args>
inline ast::node
make_call(const std::string_view& modname, const std::string_view& funcname, Args&&... args) {
    auto dot
        = ast::call(symbol("."), {}, ast::list({node(symbol(modname)), node(symbol(funcname))}));
    return ast::call(dot, {}, ast::list({node(std::forward<Args>(args))...}));
}

}  // namespace let::ast

#endif  // LET_PARSER_NODE_HPP_INCLUDED