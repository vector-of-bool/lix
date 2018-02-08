#ifndef LET_EXEC_EVAL_HPP_INCLUDED
#define LET_EXEC_EVAL_HPP_INCLUDED

#include <string_view>

#include <let/value.hpp>

namespace let {

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
value eval(const let::exec::function& fn, exec::context&, const let::tuple&);
value eval(const let::exec::closure& fn, exec::context&, const let::tuple&);

template <typename Callable, typename... Args>
let::value call(exec::context& ctx, const Callable& c, Args&&... args) {
    return eval(c, ctx, let::tuple({let::value(std::forward<Args>(args))...}));
}

} // namespace let

#endif // LET_EXEC_EVAL_HPP_INCLUDED