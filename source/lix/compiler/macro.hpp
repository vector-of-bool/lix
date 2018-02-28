#ifndef LIX_COMPILER_MACRO_HPP_INCLUDED
#define LIX_COMPILER_MACRO_HPP_INCLUDED

#include <lix/parser/node.hpp>

#include <memory>

namespace lix {

class value;

namespace exec {

class context;

}  // namespace exec

namespace detail {

class macro_base {
public:
    ~macro_base() = default;
    virtual ast::node evaluate(exec::context& ctx, const ast::list& node) const = 0;
};

template <typename Func>
class macro_impl : public macro_base {
    Func _fn;

public:
    template <typename Arg>
    explicit macro_impl(Arg&& arg)
        : _fn(std::forward<Arg>(arg)) {}
    ast::node evaluate(exec::context& ctx, const ast::list& nodes) const override {
        return _fn(ctx, nodes);
    };
};

}  // namespace detail

class macro_function {
    std::unique_ptr<detail::macro_base> _impl;

public:
    template <typename Func,
              typename = std::enable_if_t<!std::is_same<std::decay_t<Func>, macro_function>::value>>
    macro_function(Func&& fn)
        : _impl(std::make_unique<detail::macro_impl<std::decay_t<Func>>>(std::forward<Func>(fn))) {}

    ast::node evaluate(exec::context& ctx, const ast::list& args) const {
        return _impl->evaluate(ctx, args);
    }
};

ast::node expand_macros(exec::context&, const ast::node&);

ast::node escape(const ast::node&);
ast::node escape(const lix::value&);

}  // namespace lix

#endif  // LIX_COMPILER_MACRO_HPP_INCLUDED