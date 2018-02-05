#include "kernel.hpp"

#include <let/compiler/compile.hpp>
#include <let/exec/module.hpp>

#include <let/util/args.hpp>

#include <iostream>

using namespace let;
using namespace let::exec;

namespace {

struct function_def {
    ast::list arglist;
    ast::node body;

    function_def(ast::list&& arglist, ast::node&& body)
        : arglist(std::move(arglist))
        , body(std::move(body)) {}
};

struct function_def_acc {
    std::vector<function_def> defs;
};

struct function_accumulator {
    std::map<std::string, function_def_acc> fns;
};

}  // namespace

LET_BASIC_TYPEINFO(function_accumulator);

namespace {

value register_module(context& ctx, const value& args) {
    const auto& [sym] = unpack_arg_tuple<symbol>(args);
    module ret;
    ctx.register_module(sym.string(), ret);
    return let::boxed(ret);
}

value register_function(context&, const value& args) {
    const auto& [mod_, sym, fun] = unpack_arg_tuple<boxed, symbol, closure>(args);
    auto mod                     = mut_box_cast<module>(&mod_);
    if (!mod) {
        throw std::runtime_error{
            ":__let.register_function expectes a module object as first argument"};
    }
    mod->add_closure_function(sym.string(), fun);
    return symbol("ok");
}

value finalize_module(context& ctx, const function_accumulator& fns) {
    std::vector<ast::node> block;
    block.emplace_back(
        ast::make_assignment("__module",
                             ast::make_call("__let", "get_env", symbol("compiling_module"))));

    for (auto&& [name, defs] : fns.fns) {
        std::vector<ast::node> clauses;
        for (auto& def : defs.defs) {
            auto l2r_args = ast::list({def.arglist, def.body});
            auto l2r_call = ast::call(symbol("->"), {}, std::move(l2r_args));
            clauses.emplace_back(std::move(l2r_call));
        }

        auto anon_fn_ast = ast::call(symbol("fn"), {}, ast::list(std::move(clauses)));
        block.push_back(ast::make_call("__let",
                                       "register_function",
                                       ast::make_variable("__module"),
                                       ast::symbol(name),
                                       anon_fn_ast));
    }

    auto block_ast = ast::call(symbol("__block__"), {}, ast::list(std::move(block)));
    auto code      = let::compile(block_ast);
    return ctx.execute_frame(code);
}

value compile_module(context& ctx, const value& args) {
    auto arg_list = args.as_tuple();
    assert(arg_list);
    assert(arg_list->size() == 2);
    auto mod_sym = (*arg_list)[0].as_symbol();
    assert(mod_sym);
    auto   ast = ast::node::from_value((*arg_list)[1]);
    module mod;
    return ctx.push_environment([&] {
        ctx.set_environment_value("compiling_module", let::boxed(mod));
        ctx.set_environment_value("module_function_accumulator",
                                  let::boxed(function_accumulator()));
        auto inner_expanded = let::expand_macros(ctx, ast);
        auto inner_code     = let::compile(inner_expanded);
        // Execute the code that will accumulate our attributes and function definitions
        ctx.execute_frame(inner_code);
        // Now we compile and run some ethereal code that produce the actual
        // module code.
        auto acc_val = ctx.get_environment_value("module_function_accumulator");
        assert(acc_val);
        auto acc_box = acc_val->as_boxed();
        assert(acc_box);
        auto& mod_fn_acc = let::mut_box_cast<function_accumulator>(*acc_box);
        ctx.register_module(mod_sym->string(), mod);
        return finalize_module(ctx, mod_fn_acc);
    });
}

ast::node defmodule_macro(context&, const ast::list& args_) {
    macro_argument_parser args{args_};
    if (args.count() != 2) {
        throw std::runtime_error{"`defmodule` expects two arguments"};
    }

    auto modname = args.nth_as<symbol>(0);
    if (!modname) {
        throw std::runtime_error{"First argument to `defmodule` must be a symbol"};
    }

    auto block = args.keyword_get("do");
    if (!block) {
        throw std::runtime_error{"Expected 'do' block for `defmodule` call"};
    }

    auto      mod_ast = let::escape(*block);
    ast::call register_call{ast::call(ast::symbol("."),
                                      {},
                                      ast::list(
                                          {ast::symbol("__let"), ast::symbol("compile_module")})),
                            {},
                            ast::list({*modname, mod_ast})};
    return std::move(register_call);
}

std::pair<std::string, ast::list> extract_call_sig(const ast::call& call) {
    auto head_sym = call.target().as_symbol();
    if (!head_sym) {
        throw std::runtime_error{"`def` call signature must be named by an unqualified identifier"};
    }
    if (auto arg_list = call.arguments().as_list()) {
        return {head_sym->string(), *arg_list};
    } else if (auto arg_sym = call.arguments().as_symbol(); arg_sym && arg_sym->string() == "Var") {
        return {head_sym->string(), ast::list()};
    } else {
        throw std::runtime_error{"Invalid argument list to `def`"};
    }
}

ast::node def_macro(context&, const ast::list& args_) {
    macro_argument_parser args{args_};
    if (args.count() != 2) {
        throw std::runtime_error{"Invalid arguments to `def`"};
    }
    auto call_head = let::escape(args.nth(0));
    auto do_block  = args.keyword_get("do");
    if (!do_block) {
        throw std::runtime_error{"`def` expects a 'do' block"};
    }
    auto body = let::escape(*do_block);
    return ast::call(ast::call(ast::symbol("."),
                               {},
                               ast::list(
                                   {ast::symbol("__let"), ast::symbol("def_module_function")})),
                     {},
                     ast::list({call_head, body}));
}

let::value define_module_function(let::exec::context& ctx, const let::value& args_) {
    argument_parser args{args_};

    // Do some environment checks
    auto maybe_mod = ctx.get_environment_value("compiling_module");
    if (!maybe_mod) {
        throw std::runtime_error{"`def` macro must appear within a `defmodule` block"};
    }

    // Get the function definition accumulator
    auto acc_val = ctx.get_environment_value("module_function_accumulator");
    assert(acc_val);
    auto acc_box = acc_val->as_boxed();
    assert(acc_box);
    auto& mod_fn_acc = let::mut_box_cast<function_accumulator>(*acc_box);

    auto sig_ast  = ast::node::from_value(args.nth(0));
    auto body_ast = ast::node::from_value(args.nth(1));

    auto sig_call = sig_ast.as_call();
    if (!sig_call) {
        throw std::runtime_error{"Invalid signature to `def`"};
    }
    auto [def_name, def_arglist] = extract_call_sig(*sig_call);

    // Find the function accumulator for this function
    auto fn_iter = mod_fn_acc.fns.find(def_name);
    if (fn_iter == mod_fn_acc.fns.end()) {
        // New function name.
        fn_iter = mod_fn_acc.fns.emplace(def_name, function_def_acc()).first;
    }
    assert(fn_iter != mod_fn_acc.fns.end());
    auto& fn_acc = fn_iter->second;

    auto expanded = expand_macros(ctx, body_ast);
    fn_acc.defs.emplace_back(std::move(def_arglist), std::move(expanded));

    return symbol("ok");
}

module build_bootstrap_module() {
    module ret;
    ret.add_function("register_module", &register_module);
    ret.add_function("register_function", &register_function);
    ret.add_function("compile_module", &compile_module);
    ret.add_function("def_module_function", &define_module_function);
    ret.add_function("get_env", [](context& ctx, const let::value& args_) -> let::value {
        argument_parser args{args_};
        assert(args.count() == 1);
        if (auto sym_arg = args.nth_as<symbol>(0)) {
            auto maybe_val = ctx.get_environment_value(sym_arg->string());
            if (maybe_val) {
                return *maybe_val;
            } else {
                return symbol("nil");
            }
        } else {
            assert(false && "Invalid arguments to :__let.get_env");
        }
    });
    return ret;
}

module build_kernel_module() {
    module ret;
    ret.add_macro("defmodule", &defmodule_macro);
    ret.add_macro("def", &def_macro);
    return ret;
}

const module& bootstrap_module() {
    static auto mod = build_bootstrap_module();
    return mod;
}

const module& kernel_module() {
    static auto mod = build_kernel_module();
    return mod;
}

}  // namespace

context exec::build_bootstrap_context() {
    context ret;
    ret.register_module("__let", bootstrap_module());
    return ret;
}

context exec::build_kernel_context() {
    auto ret = build_bootstrap_context();
    ret.register_module("Kernel", kernel_module());
    return ret;
}