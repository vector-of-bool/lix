#include "kernel.hpp"

#include <lix/compiler/compile.hpp>
#include <lix/exec/exec.hpp>
#include <lix/exec/module.hpp>

#include <lix/util/args.hpp>
#include <lix/util/wrap_fn.hpp>

#include <algorithm>

using namespace lix;
using namespace lix::exec;

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
    std::string                             module_name;
    std::map<std::string, function_def_acc> fns;
};

}  // namespace

LIX_BASIC_TYPEINFO(function_accumulator);

namespace {

value register_module(context& ctx, const value& args) {
    const auto& [sym] = unpack_arg_tuple<symbol>(args);
    module ret;
    ctx.register_module(sym.string(), ret);
    return lix::boxed(ret);
}

value register_function(context&, const value& args) {
    const auto& [mod, sym, fun] = unpack_arg_tuple<module, symbol, closure>(args);
    mod.add_closure_function(sym.string(), fun);
    return symbol("ok");
}

struct function_final_pass {
    ast::node run_final_pass(const ast::node&            node,
                             const std::string&          fn_name,
                             const function_accumulator& fn_acc) {
        return node.visit(
            [&](const auto& what) -> ast::node { return do_final_pass(what, fn_name, fn_acc); });
    }

    ast::node do_final_pass(ast::integer i, const std::string&, const function_accumulator&) {
        return ast::node(i);
    }
    ast::node do_final_pass(ast::real i, const std::string&, const function_accumulator&) {
        return ast::node(i);
    }
    ast::node do_final_pass(const ast::symbol& s, const std::string&, const function_accumulator&) {
        return ast::node(s);
    }
    ast::node do_final_pass(const ast::string& s, const std::string&, const function_accumulator&) {
        return ast::node(s);
    }
    ast::node do_final_pass(const ast::list&            l,
                            const std::string&          fn_name,
                            const function_accumulator& fn_acc) {
        std::vector<ast::node> new_nodes;
        for (auto& n : l.nodes) {
            new_nodes.push_back(run_final_pass(n, fn_name, fn_acc));
        }
        return ast::list(std::move(new_nodes));
    }
    ast::node do_final_pass(const ast::tuple&           t,
                            const std::string&          fn_name,
                            const function_accumulator& fn_acc) {
        std::vector<ast::node> new_nodes;
        for (auto& n : t.nodes) {
            new_nodes.push_back(run_final_pass(n, fn_name, fn_acc));
        }
        return ast::tuple(std::move(new_nodes));
    }
    ast::node do_final_pass(const ast::call&            call,
                            const std::string&          fn_name,
                            const function_accumulator& fn_acc) {
        auto args = run_final_pass(call.arguments(), fn_name, fn_acc);
        if (auto lhs_sym = call.target().as_symbol(); lhs_sym && !args.as_symbol()) {
            // Call to a symbol. Maybe an unqualified call?
            auto fn_def_iter = fn_acc.fns.find(lhs_sym->string());
            if (fn_def_iter != fn_acc.fns.end()) {
                // It's an unqualified call to a function in this module. Nice
                auto qual = ast::call(symbol("."),
                                      {},
                                      ast::list({symbol(fn_acc.module_name), call.target()}));
                return ast::call(qual, call.meta(), std::move(args));
            }
        }
        // Just another call
        auto lhs = run_final_pass(call.target(), fn_name, fn_acc);
        return ast::call(lhs, call.meta(), std::move(args));
    }
};

value finalize_module(context& ctx, const function_accumulator& fns) {
    std::vector<ast::node> block;
    block.emplace_back(
        ast::make_assignment("__module",
                             ast::make_call("__lix", "get_env", symbol("compiling_module"))));

    auto modname_ = ctx.get_environment_value("compiling_module_name");
    assert(modname_);
    auto modname_1 = modname_->as_string();
    assert(modname_1);
    auto& modname = *modname_1;

    for (auto&& [name, defs] : fns.fns) {
        std::vector<ast::node> clauses;
        for (auto& def : defs.defs) {
            auto l2r_args = ast::list({def.arglist, def.body});
            auto l2r_call = ast::call(symbol("->"), {}, std::move(l2r_args));
            auto fn_final = function_final_pass{}.run_final_pass(l2r_call, name, fns);
            clauses.emplace_back(std::move(fn_final));
        }

        ast::meta fn_meta;
        fn_meta.set_fn_details(modname, name);
        auto anon_fn_ast
            = ast::call(symbol("fn"), std::move(fn_meta), ast::list(std::move(clauses)));
        block.push_back(ast::make_call("__lix",
                                       "register_function",
                                       ast::make_variable("__module"),
                                       ast::symbol(name),
                                       anon_fn_ast));
    }

    auto block_ast = ast::call(symbol("__block__"), {}, ast::list(std::move(block)));
    auto code      = lix::compile(block_ast);
    return exec::executor(code).execute_all(ctx);
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
        ctx.set_environment_value("compiling_module", lix::boxed(mod));
        ctx.set_environment_value("compiling_module_name", mod_sym->string());
        ctx.set_environment_value("module_function_accumulator",
                                  lix::boxed(function_accumulator{mod_sym->string(), {}}));
        auto inner_expanded = lix::expand_macros(ctx, ast);
        auto inner_code     = lix::compile(inner_expanded);
        // Execute the code that will accumulate our attributes and function definitions
        exec::executor(inner_code).execute_all(ctx);
        // Now we compile and run some ethereal code that produce the actual
        // module code.
        auto acc_val = ctx.get_environment_value("module_function_accumulator");
        assert(acc_val);
        auto acc_box = acc_val->as_boxed();
        assert(acc_box);
        auto& mod_fn_acc = lix::mut_box_cast<function_accumulator>(*acc_box);
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

    auto      mod_ast = lix::escape(*block);
    ast::call register_call{ast::call(ast::symbol("."),
                                      {},
                                      ast::list(
                                          {ast::symbol("__lix"), ast::symbol("compile_module")})),
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
    auto call_head = lix::escape(args.nth(0));
    auto do_block  = args.keyword_get("do");
    if (!do_block) {
        throw std::runtime_error{"`def` expects a 'do' block"};
    }
    auto body = lix::escape(*do_block);
    return ast::call(ast::call(ast::symbol("."),
                               {},
                               ast::list(
                                   {ast::symbol("__lix"), ast::symbol("def_module_function")})),
                     {},
                     ast::list({call_head, body}));
}

lix::value define_module_function(lix::exec::context& ctx, const lix::value& args_) {
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
    auto& mod_fn_acc = lix::mut_box_cast<function_accumulator>(*acc_box);

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
    ret.add_function("get_env", [](context& ctx, const lix::value& args_) -> lix::value {
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
            assert(false && "Invalid arguments to :__lix.get_env");
            std::terminate();
        }
    });
    return ret;
}

lix::value k_reverse_list(exec::context&, const lix::value& v) {
    const auto& [list] = unpack_arg_tuple<lix::list>(v);
    std::vector<lix::value> new_list{list.begin(), list.end()};
    std::reverse(new_list.begin(), new_list.end());
    return lix::list(std::make_move_iterator(new_list.begin()),
                     std::make_move_iterator(new_list.end()));
}

const module& bootstrap_module() {
    static auto mod = build_bootstrap_module();
    return mod;
}

const module& kernel_module() {
    static auto mod = [] {
        module mod;
        mod.add_macro("defmodule", &defmodule_macro);
        mod.add_macro("def", &def_macro);
        mod.add_function("__reverse_list", &k_reverse_list);
        mod.add_function("__map_pop",
                         lix::wrap_function([](const lix::map&   map,
                                               const lix::value& key,
                                               const lix::value& def) -> lix::value {
                             auto pair_opt = map.pop(key);
                             if (!pair_opt) {
                                 return lix::tuple::make(def, map);
                             } else {
                                 return lix::tuple::make(pair_opt->first, pair_opt->second);
                             }
                         }));
        mod.add_function("__map_put",
                         lix::wrap_function([](const lix::map&   map,
                                               const lix::value& key,
                                               const lix::value& val) -> lix::value {
                             return map.insert_or_update(key, val);
                         }));
        mod.add_function("__map_fetch",
                         lix::wrap_function(
                             [](const lix::map& map, const lix::value& key) -> lix::value {
                                 auto found = map.find(key);
                                 if (found) {
                                     return lix::tuple::make("ok"_sym, *found);
                                 } else {
                                     return "error"_sym;
                                 }
                             }));
        return mod;
    }();
    return mod;
}

}  // namespace

context exec::build_bootstrap_context() {
    context ret;
    ret.register_module("__lix", bootstrap_module());
    return ret;
}

context exec::build_kernel_context() {
    auto ret = build_bootstrap_context();
    ret.register_module("Kernel", kernel_module());
    return ret;
}