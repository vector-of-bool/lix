#include "eval.hpp"

#include <let/compiler/compile.hpp>
#include <let/compiler/macro.hpp>
#include <let/exec/context.hpp>
#include <let/exec/exec.hpp>
#include <let/exec/kernel.hpp>
#include <let/parser/parse.hpp>

namespace {

struct val_conv_visitor {};

}  // namespace

let::value let::eval(std::string_view str) {
    auto ctx = exec::build_kernel_context();
    return eval(str, ctx);
}

let::value let::eval(const let::ast::node& node) {
    auto ctx = exec::build_kernel_context();
    return eval(node, ctx);
}

let::value let::eval(std::string_view str, let::exec::context& ctx) {
    return eval(let::ast::parse(str), ctx);
}

let::value let::eval(const let::ast::node& node, let::exec::context& ctx) {
    auto                expanded = expand_macros(ctx, node);
    auto                code     = let::compile(expanded);
    let::exec::executor exec{code};
    return exec.execute_all(ctx);
}

let::value let::eval(const let::exec::function& fn, exec::context& ctx, const let::tuple& args) {
    return fn.call_ll(ctx, args);
}

let::value let::eval(const let::exec::closure& fn, exec::context& ctx, const let::tuple& args) {
    let::exec::executor ex{fn, args};
    return ex.execute_all(ctx);
}