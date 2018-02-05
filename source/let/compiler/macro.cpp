#include "macro.hpp"

#include <let/exec/context.hpp>
#include <let/exec/module.hpp>

#include <cassert>

using namespace let;

namespace {

using ast::node;

struct macro_expander {
    exec::context&           ctx;
    std::vector<std::string> imported_modules{"Kernel"};

    node operator()(const ast::list& l) {
        std::vector<ast::node> new_nodes;
        for (auto& n : l.nodes) {
            new_nodes.emplace_back(n.visit(*this));
        }
        return ast::list(std::move(new_nodes));
    }

    node operator()(const ast::tuple& t) {
        std::vector<ast::node> new_nodes;
        for (auto& n : t.nodes) {
            new_nodes.emplace_back(n.visit(*this));
        }
        return ast::tuple(std::move(new_nodes));
    }

    node operator()(ast::integer i) { return node(i); }
    node operator()(ast::floating f) { return node(f); }
    node operator()(const ast::symbol& s) { return node(s); }

    // The real meat of the operation.
    node operator()(const ast::call& call) {
        if (auto lhs_sym = call.target().as_symbol()) {
            if (lhs_sym->string() == "__block__") {
                // This is a context in which they might `import` a module.
                auto prev_imports = imported_modules;
                auto block_args   = call.arguments().visit(*this);
                auto ret
                    = node(ast::call(call.target().clone(), call.meta(), std::move(block_args)));
                imported_modules = prev_imports;
                return std::move(ret);
            } else if (lhs_sym->string() == "import") {
                auto args = call.arguments().as_list();
                assert(args);
                for (auto& n : args->nodes) {
                    if (auto import_sym = n.as_symbol()) {
                        imported_modules.emplace_back(import_sym->string());
                    } else {
                        // Argument is not a symbol. Not allowed
                        throw std::runtime_error{"`import` expects symbol arguments"};
                    }
                }
                return node(call);
            } else if (auto arglist = call.arguments().as_list()) {
                // We're calling an unqualified name. That might be a macro
                return _try_expand(*lhs_sym, call.meta(), *arglist);
            } else {
                // Not a function call
                return node(call);
            }
        }
        // TODO: Remote call macros
        return node(call);
    }

    node _try_expand(const ast::symbol& s, const ast::meta& m, const ast::list& arguments) {
        for (auto& modname : imported_modules) {
            auto maybe_mod = ctx.get_module(modname);
            if (maybe_mod) {
                auto maybe_macro = maybe_mod->get_macro(s.string());
                if (maybe_macro) {
                    return maybe_macro->evaluate(ctx, arguments);
                }
            }
        }
        // No macro expanded.
        return node(ast::call(s, m, ast::node(arguments)));
    }
};

struct ast_escaper {
    node operator()(const ast::list& l) {
        std::vector<ast::node> new_nodes;
        for (auto& n : l.nodes) {
            new_nodes.push_back(n.visit(*this));
        }
        return ast::list(std::move(new_nodes));
    }
    node operator()(const ast::tuple& t) {
        std::vector<ast::node> new_nodes;
        for (auto& n : t.nodes) {
            new_nodes.push_back(n.visit(*this));
        }
        if (new_nodes.size() == 3) {
            // Need to escape
            return ast::call(symbol("{}"), {}, ast::list(std::move(new_nodes)));
        } else {
            return ast::tuple(std::move(new_nodes));
        }
    }
    node operator()(ast::integer i) { return node(i); }
    node operator()(ast::floating f) { return node(f); }
    node operator()(const ast::symbol& s) { return node(s); }

    node operator()(const ast::call& call) {
        auto target = call.target().visit(*this);
        auto args   = call.arguments().visit(*this);
        return ast::call(symbol("{}"), {}, ast::list({target, node(ast::list()), args}));
    }
};

}  // namespace

ast::node let::expand_macros(exec::context& ctx, const ast::node& input) {
    macro_expander ex{ctx};
    return input.visit(ex);
}

ast::node let::escape(const ast::node& n) { return n.visit(ast_escaper{}); }

ast::node let::escape(const let::value& v) { return escape(node::from_value(v)); }