#ifndef LIX_PARSER_NODE_HPP_INCLUDED
#define LIX_PARSER_NODE_HPP_INCLUDED

#include <lix/string.hpp>
#include <lix/symbol.hpp>
#include <lix/util.hpp>
#include <lix/util/opt_ref.hpp>
#include <lix/value_fwd.hpp>
#include <lix/variant.hpp>

#include <ostream>
#include <string>
#include <vector>

namespace lix::ast {

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

using integer = std::int64_t;
using real    = double;
using symbol  = lix::symbol;
using string  = lix::string;

class meta {
    std::optional<std::pair<string, string>> _fn_details;
    // File location details:
    int _line   = -1;
    int _column = -1;

public:
    meta() = default;

    void set_fn_details(std::string module, std::string name) { _fn_details.emplace(module, name); }
    auto& fn_details() const noexcept { return _fn_details; }

    void set_line(int line) { _line = line; }
    auto line() const { return _line; }
    void set_column(int column) { _column = column; }
    auto column() const { return _column; }

    value to_value() const;
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
    using node_var = std::variant<list, tuple, integer, real, symbol, string, call>;
    std::shared_ptr<const node_var> _var;

public:
    node(list l)
        : _var(std::make_unique<node_var>(std::move(l))) {}
    node(tuple t)
        : _var(std::make_unique<node_var>(std::move(t))) {}
    explicit node(integer i)
        : _var(std::make_unique<node_var>(std::move(i))) {}
    explicit node(real f)
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
    DEF_OBS(real);
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

    lix::value  to_value() const;
    static node from_value(const lix::value&);
};

std::ostream& operator<<(std::ostream& o, const node& n);
std::string   to_string(const node& n);

call::call(node el, lix::ast::meta m, node args)
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

}  // namespace lix::ast

#endif  // LIX_PARSER_NODE_HPP_INCLUDED