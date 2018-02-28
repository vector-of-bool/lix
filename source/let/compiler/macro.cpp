#include "macro.hpp"

#include <let/exec/context.hpp>
#include <let/exec/module.hpp>

#include <let/util/args.hpp>

#include <cassert>

using namespace let;

namespace {

using ast::node;

struct aliased_symbols {
    std::string alias;
    std::string expansion;
};

struct macro_expander {
    exec::context& ctx;

    explicit macro_expander(exec::context& c)
        : ctx(c) {}

    std::vector<std::string>     imported_modules{"Kernel"};
    std::vector<aliased_symbols> aliases;

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
    node operator()(ast::real f) { return node(f); }
    node operator()(const ast::symbol& s) {
        for (auto& alias : aliases) {
            if (s.string().find(alias.alias) == 0) {
                if (s.string() == alias.alias) {
                    // Full expansion. Just expand the alias
                    return node(symbol(alias.expansion));
                } else {
                    assert(s.string().size() > alias.alias.size());
                    if (s.string()[alias.alias.size()] == '.') {
                        auto cp = s.string();
                        cp.replace(cp.begin(), cp.begin() + alias.alias.size(), alias.expansion);
                        return node(symbol(cp));
                    } else {
                        return node(s);
                    }
                }
            }
        }
        return node(s);
    }
    node operator()(const ast::string& s) { return node(s); }

    // The real meat of the operation.
    node operator()(const ast::call& call) {
        if (auto lhs_sym = call.target().as_symbol()) {
            if (lhs_sym->string() == "__block__") {
                // This is a context in which they might `import` a module.
                auto prev_imports = imported_modules;
                auto prev_aliases = aliases;
                auto block_args   = call.arguments().visit(*this);
                auto ret = node(ast::call(call.target(), call.meta(), std::move(block_args)));
                imported_modules = prev_imports;
                aliases          = prev_aliases;
                return std::move(ret);
            } else if (lhs_sym->string() == "import") {
                auto args_list = call.arguments().as_list();
                assert(args_list);
                for (auto& n : args_list->nodes) {
                    if (auto import_sym = n.as_symbol()) {
                        imported_modules.emplace_back(import_sym->string());
                    } else {
                        // Argument is not a symbol. Not allowed
                        throw std::runtime_error{"`import` expects symbol arguments"};
                    }
                }
                return node(symbol("ok"));
            } else if (lhs_sym->string() == "alias") {
                auto args_list = call.arguments().as_list();
                assert(args_list);
                macro_argument_parser args{*args_list};
                auto                  target = args.nth_as<symbol>(0);
                if (!target) {
                    throw std::runtime_error{"First argument to 'alias' must be a symbol"};
                }
                auto as_kw = args.keyword_get("as");
                if (as_kw) {
                    auto as_sym = as_kw->as_symbol();
                    assert(as_sym);
                    aliases.emplace_back(aliased_symbols{as_sym->string(), target->string()});
                } else {
                    auto final_dot = target->string().rfind('.');
                    if (final_dot == target->string().npos) {
                        throw std::runtime_error{"Invalid alias '" + target->string() + "'"};
                    }
                    auto alias = target->string().substr(final_dot + 1);
                    aliases.emplace_back(aliased_symbols{alias, target->string()});
                }
                return node(symbol("ok"));
            } else if (auto arglist = call.arguments().as_list()) {
                // We're calling an unqualified name. That might be a macro
                return _try_expand(*lhs_sym, call.meta(), *arglist);
            } else {
                // Not a function call
                return node(call);
            }
        }
        // TODO: Remote call macros
        auto lhs = call.target().visit(*this);
        auto args = call.arguments().visit(*this);
        return ast::call(std::move(lhs), call.meta(), std::move(args));
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
        auto lhs = (*this)(s);
        auto args = (*this)(arguments);
        return node(ast::call(std::move(lhs), m, std::move(args)));
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
    node operator()(ast::real f) { return node(f); }
    node operator()(const ast::symbol& s) { return node(s); }
    node operator()(const ast::string& s) { return node(s); }

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