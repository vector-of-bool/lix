#include "eval.hpp"

#include <lix/compiler/compile.hpp>
#include <lix/compiler/macro.hpp>
#include <lix/exec/context.hpp>
#include <lix/exec/exec.hpp>
#include <lix/exec/kernel.hpp>
#include <lix/parser/parse.hpp>

namespace {

struct val_conv_visitor {};

}  // namespace

lix::value lix::eval(std::string_view str) {
    auto ctx = exec::build_kernel_context();
    return eval(str, ctx);
}

lix::value lix::eval(const lix::ast::node& node) {
    auto ctx = exec::build_kernel_context();
    return eval(node, ctx);
}

lix::value lix::eval(std::string_view str, lix::exec::context& ctx) {
    return eval(lix::ast::parse(str), ctx);
}

lix::value lix::eval(const lix::ast::node& node, lix::exec::context& ctx) {
    auto                expanded = expand_macros(ctx, node);
    auto                code     = lix::compile(expanded);
    lix::exec::executor exec{code};
    return exec.execute_all(ctx);
}

lix::value lix::eval(const lix::exec::function& fn, exec::context& ctx, const lix::tuple& args) {
    return fn.call_ll(ctx, args);
}

lix::value lix::eval(const lix::exec::closure& fn, exec::context& ctx, const lix::tuple& args) {
    lix::exec::executor ex{fn, args};
    return ex.execute_all(ctx);
}

lix::value
lix::call_mfa_tup(exec::context& ctx, lix::symbol mod_sym, lix::symbol fn_sym, const lix::tuple& args) {
    auto mod = ctx.get_module(mod_sym.string());
    if (!mod) {
        throw std::runtime_error{"No such module " + mod_sym.string()};
    }
    auto fn = mod->get_function(fn_sym.string());
    if (!fn) {
        throw std::runtime_error{"No such function " + fn_sym.string()};
    }
    return std::visit([&](auto&& func) { return lix::eval(func, ctx, args); }, *fn);
}
