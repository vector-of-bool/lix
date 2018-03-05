#ifndef LIX_EXEC_EVAL_HPP_INCLUDED
#define LIX_EXEC_EVAL_HPP_INCLUDED

#include <string_view>

#include <lix/value.hpp>

namespace lix {

namespace ast {

class node;

} // namespace ast

namespace exec {

class context;

} // namespace exec

value eval(std::string_view);
value eval(std::string_view, exec::context&);
value eval(const ast::node&);
value eval(const ast::node&, exec::context&);
value eval(const lix::exec::function& fn, exec::context&, const lix::tuple&);
value eval(const lix::exec::closure& fn, exec::context&, const lix::tuple&);

template <typename Callable, typename... Args>
lix::value call(exec::context& ctx, const Callable& c, Args&&... args) {
    return eval(c, ctx, lix::tuple({lix::value(std::forward<Args>(args))...}));
}

lix::value
call_mfa_tup(exec::context& ctx, lix::symbol mod, lix::symbol fn, const lix::tuple& args);

template <typename... Args>
lix::value call_mfa(exec::context& ctx, lix::symbol mod, lix::symbol fn, Args&&... args) {
    return call_mfa_tup(ctx, mod, fn, lix::tuple({lix::value(std::forward<Args>(args))...}));
}

} // namespace lix

#endif // LIX_EXEC_EVAL_HPP_INCLUDED