#include "compile.hpp"

#include <let/parser/node.hpp>

#include <let/code/builder.hpp>
#include <let/code/code.hpp>

#include <let/util/args.hpp>

#include <cassert>
#include <list>
#include <map>
#include <optional>
#include <stack>

using namespace let;
namespace is = let::code::is_types;

namespace {

using let::code::inst_offset_t;
using let::code::slot_ref_t;

constexpr slot_ref_t    invalid_slot = {static_cast<std::size_t>(-1)};
constexpr inst_offset_t invalid_inst = {static_cast<std::size_t>(-1)};
using varslot_map                    = std::map<std::string, slot_ref_t>;
struct capture {
    std::string varname;
    slot_ref_t  parent_slot;
    slot_ref_t  inner_slot;
};
using capture_list = std::vector<capture>;

struct expand_quoted {};

using varscope_stack = std::vector<varslot_map>;

opt_ref<const string> get_var_string(const ast::node& n) {
    auto call = n.as_call();
    if (!call) {
        return std::nullopt;
    }
    auto varsym = call->target().as_symbol();
    if (!varsym) {
        return std::nullopt;
    }
    auto var_arg = call->arguments().as_symbol();
    if (!var_arg) {
        return std::nullopt;
    }
    return varsym->string();
}

struct minifun_rewriter {
    std::string  arg_prefix;
    ast::integer n_args;

    ast::node _make_arg_ref(ast::integer i) {
        n_args = (std::max)(i, n_args);
        return ast::make_variable(arg_prefix + std::to_string(i - 1));
    }

    ast::node operator()(ast::integer i) { return ast::node(i); }
    ast::node operator()(ast::floating f) { return ast::node(f); }
    ast::node operator()(ast::symbol s) { return ast::node(s); }
    ast::node operator()(const ast::string& s) { return ast::node(s); }
    ast::node operator()(const ast::list& l) {
        ast::list ret;
        for (auto& n : l.nodes) {
            ret.nodes.push_back(n.visit(*this));
        }
        return ret;
    }
    ast::node operator()(const ast::tuple& t) {
        ast::tuple ret;
        for (auto& n : t.nodes) {
            ret.nodes.push_back(n.visit(*this));
        }
        return ret;
    }
    ast::node operator()(const ast::call& c) {
        auto& lhs = c.target();
        if (lhs.as_symbol() && lhs.as_symbol()->string() == "&") {
            auto arg_list = c.arguments().as_list();
            if (arg_list && arg_list->nodes.size() == 1) {
                auto first_int = arg_list->nodes[0].as_integer();
                if (first_int) {
                    // We're a minifun argument
                    return _make_arg_ref(*first_int);
                }
            }
        }
        // Just a call
        auto new_target = c.target().visit(*this);
        auto new_args   = c.arguments().visit(*this);
        return ast::call(new_target, c.meta(), new_args);
    }
};

ast::node rewrite_minifun(const ast::list& args) {
    if (args.nodes.size() != 1) {
        throw std::runtime_error{"Invalid arguments to unary '&' operator"};
    }
    auto& first     = args.nodes[0];
    auto  first_int = first.as_integer();
    if (first_int) {
        throw std::runtime_error{"Invalid &N argument outside of &() function expression"};
    }
    static unsigned  mf_counter = 0;
    std::string      arg_prefix = "__minifun_arg_" + std::to_string(mf_counter++);
    minifun_rewriter rewriter{arg_prefix, 0};
    const auto       new_rhs = first.visit(rewriter);
    ast::list        fn_arglist;
    for (auto i = 0; i < rewriter.n_args; ++i) {
        fn_arglist.nodes.push_back(ast::make_variable(arg_prefix + std::to_string(i)));
    }
    auto l2r_args = ast::make_list(fn_arglist, new_rhs);
    const auto l2r_call = ast::call("->"_sym, {}, std::move(l2r_args));
    auto       clause_list = ast::make_list(l2r_call);
    return ast::call("fn"_sym, {}, clause_list);
}

struct block_compiler {
    code::code_builder& builder;
    varscope_stack      variable_scopes;

    /**
     * To handle bindings versus variable references:
     *
     * When we are parsing a binding context, we increment binding_expr_depth by
     * one. When we finish, we decrement.
     *
     * If a variable is referenced that isn't yet defined, we insert a
     * "binding slot" in the stack that will be replaced at runtime once the
     * match has been completed.
     */
    int binding_expr_depth = 0;

    /**
     * Binding clauses (the head of a `case` or `fn` clause) is different than
     * a regular binding expr. Within its bounds, hard matches become test
     * matches. We keep track of that here.
     */
    int clause_test_depth = 0;

    /**
     * We keep track of where the expressions end up in the slot stack and
     * increment while we advance instructions that produce values
     */
    slot_ref_t current_end_slot{0};
    /**
     * Call to advance the slot counter and return the just consumed slot. The
     * first call will return slot zero, the second slot one, and so on.
     */
    slot_ref_t consume_slot() {
        auto old = current_end_slot;
        current_end_slot.index++;
        return old;
    }
    /**
     * Get the offset of the current instruction
     */
    inst_offset_t current_instruction() const { return builder.current_offset(); }

    explicit block_compiler(code::code_builder& b)
        : builder(b) {}

    /**
     * Obtain a reference to the slot belonging to the naemd variable
     */
    std::optional<slot_ref_t> slot_for_variable(const std::string& name) {
        auto       map_iter = variable_scopes.rbegin();
        const auto map_end  = variable_scopes.rend();
        while (map_iter != map_end) {
            auto found = map_iter->find(name);
            if (found != map_iter->end()) {
                return found->second;
            }
            ++map_iter;
        }
        return std::nullopt;
    }

    /**
     * Get a reference to the top-most variable scope
     */
    varslot_map& top_varmap() {
        assert(!variable_scopes.empty());
        return variable_scopes.back();
    }

    /**
     * Compile the given AST node and return the slot that corresponds to its
     * resulting runtime value
     */
    slot_ref_t compile(const ast::node& n) { return n.visit(*this); }

    /**
     * Compile the given AST with a new variable scope
     */
    slot_ref_t _compile_new_scope(const ast::node& n) {
        // Create a new variable scope:
        variable_scopes.emplace_back();
        auto ret = compile(n);
        // Erase the variable scope. The variables from the scope are no longer visible
        variable_scopes.pop_back();
        return ret;
    }

    slot_ref_t compile_root(const ast::node& n) {
        auto ret_slot = _compile_new_scope(n);
        builder.push_instr(is::ret{ret_slot});
        return ret_slot;
    }

    /**
     * Compile a tuple expression. We have dedicated bytecode for each arity up to 7
     */
    slot_ref_t _compile_tuple(const std::vector<ast::node>& nodes) {
        switch (nodes.size()) {
        case 0:
            builder.push_instr(is::mk_tuple_0{});
            return consume_slot();
        case 1: {
            auto a = compile(nodes[0]);
            builder.push_instr(is::mk_tuple_1{a});
            return consume_slot();
        }
        case 2: {
            auto a = compile(nodes[0]);
            auto b = compile(nodes[1]);
            builder.push_instr(is::mk_tuple_2{a, b});
            return consume_slot();
        }
        case 3: {
            auto a = compile(nodes[0]);
            auto b = compile(nodes[1]);
            auto c = compile(nodes[2]);
            builder.push_instr(is::mk_tuple_3{a, b, c});
            return consume_slot();
        }
        case 4: {
            auto a = compile(nodes[0]);
            auto b = compile(nodes[1]);
            auto c = compile(nodes[2]);
            auto d = compile(nodes[3]);
            builder.push_instr(is::mk_tuple_4{a, b, c, d});
            return consume_slot();
        }
        case 5: {
            auto a = compile(nodes[0]);
            auto b = compile(nodes[1]);
            auto c = compile(nodes[2]);
            auto d = compile(nodes[3]);
            auto e = compile(nodes[4]);
            builder.push_instr(is::mk_tuple_5{a, b, c, d, e});
            return consume_slot();
        }
        case 6: {
            auto a = compile(nodes[0]);
            auto b = compile(nodes[1]);
            auto c = compile(nodes[2]);
            auto d = compile(nodes[3]);
            auto e = compile(nodes[4]);
            auto f = compile(nodes[5]);
            builder.push_instr(is::mk_tuple_6{a, b, c, d, e, f});
            return consume_slot();
        }
        case 7: {
            auto a = compile(nodes[0]);
            auto b = compile(nodes[1]);
            auto c = compile(nodes[2]);
            auto d = compile(nodes[3]);
            auto e = compile(nodes[4]);
            auto f = compile(nodes[5]);
            auto g = compile(nodes[6]);
            builder.push_instr(is::mk_tuple_7{a, b, c, d, e, f, g});
            return consume_slot();
        }
        default:
            assert(false && "Unimplemented (more than seven tuple elements)");
            std::terminate();
        }
    }

    void _check_binary(const ast::list& args) const {
        if (args.nodes.size() != 2) {
            assert(false && "Invalid args");
            std::terminate();
        }
    }

    slot_ref_t _compile_call(const ast::node& lhs, const ast::meta& meta, const ast::list& args) {
        auto lhs_sym = lhs.as_symbol();
        if (lhs_sym) {
            auto& lhs_str = lhs_sym->string();
            if (lhs_str == "+") {
                _check_binary(args);
                auto lhs_slot = compile(args.nodes[0]);
                auto rhs_slot = compile(args.nodes[1]);
                builder.push_instr(is::add{lhs_slot, rhs_slot});
                return consume_slot();
            } else if (lhs_str == "-") {
                _check_binary(args);
                auto lhs_slot = compile(args.nodes[0]);
                auto rhs_slot = compile(args.nodes[1]);
                builder.push_instr(is::sub{lhs_slot, rhs_slot});
                return consume_slot();
            } else if (lhs_str == "=") {
                _check_binary(args);
                return _compile_assign(args.nodes);
            } else if (lhs_str == "__block__") {
                assert(args.nodes.size() != 0 && "Invalid block. Needs at least one expression");
                auto ret = invalid_slot;
                for (auto& arg : args.nodes) {
                    ret = compile(arg);
                }
                // Return the slot from the last expression we compiled
                return ret;
            } else if (lhs_str == "{}") {
                return _compile_tuple(args.nodes);
            } else if (lhs_str == "==") {
                _check_binary(args);
                auto lhs_slot = compile(args.nodes[0]);
                auto rhs_slot = compile(args.nodes[1]);
                builder.push_instr(is::eq{lhs_slot, rhs_slot});
                return consume_slot();
            } else if (lhs_str == "!=") {
                _check_binary(args);
                auto lhs_slot = compile(args.nodes[0]);
                auto rhs_slot = compile(args.nodes[1]);
                builder.push_instr(is::neq{lhs_slot, rhs_slot});
                return consume_slot();
            } else if (lhs_str == "&") {
                auto rewritten = rewrite_minifun(args);
                return compile(rewritten);
            } else if (lhs_str == "cond") {
                return _compile_cond(args.nodes);
            } else if (lhs_str == "case") {
                return _compile_case(args.nodes);
            } else if (lhs_str == "quote") {
                return _compile_quote(args);
            } else if (lhs_str == "is_list") {
                assert(args.nodes.size() == 1);
                auto rhs_slot = compile(args.nodes[0]);
                builder.push_instr(is::is_list{rhs_slot});
                return consume_slot();
            } else if (lhs_str == "raise") {
                if (args.nodes.size() != 1) {
                    throw std::runtime_error{"'raise' expects one argument"};
                }
                auto arg_slot = compile(args.nodes[0]);
                builder.push_instr(is::raise{arg_slot});
                // Raise will not consume a slot. It has no expression value
                return current_end_slot;
            } else if (lhs_str == "__slot!!") {
                auto arg = args.nodes[0].as_integer();
                return slot_ref_t{static_cast<std::size_t>(*arg)};
            } else if (lhs_str == ".") {
                if (args.nodes.size() == 1) {
                    // We're a closure call.
                    return compile(args.nodes[0]);
                } else {
                    _check_binary(args);
                    auto lhs_slot = compile(args.nodes[0]);
                    auto rhs_slot = compile(args.nodes[1]);
                    builder.push_instr(is::dot{lhs_slot, rhs_slot});
                    return consume_slot();
                }
            } else if (lhs_str == "|>") {
                _check_binary(args);
                return _compile_pipe(args.nodes);
            } else if (lhs_str == "fn") {
                return _compile_anon_fn(args.nodes, meta);
            } else if (lhs_str == "apply") {
                if (args.nodes.size() != 3) {
                    throw std::runtime_error{"'apply' expects three arguments"};
                }
                auto mod_slot = compile(args.nodes[0]);
                auto fn_slot  = compile(args.nodes[1]);
                auto arg_slot = compile(args.nodes[2]);
                builder.push_instr(is::apply{mod_slot, fn_slot, arg_slot});
                return consume_slot();
            }
        }
        // No special function
        if (auto lhs_sym = lhs.as_symbol()) {
            // Unqualified call is not allowed at this level in the AST
            throw std::runtime_error{
                "Unqualified function call does not resolve to a function in the current module: "
                + lhs_sym->string()};
        }
        auto                    fn_slot = compile(lhs);
        std::vector<slot_ref_t> arg_slots;
        for (auto& arg : args.nodes) {
            arg_slots.push_back(compile(arg));
        }
        builder.push_instr(is::mk_tuple_n{std::move(arg_slots)});
        auto arg_slot = consume_slot();
        builder.push_instr(is::call{fn_slot, arg_slot});
        return consume_slot();
    }

    slot_ref_t _compile_assign(const std::vector<ast::node>& args) {
        if (clause_test_depth != 0) {
            throw std::runtime_error{
                "Bindings `=` are not allowed within function or `case` clause heads"};
        }
        auto left_var_name = get_var_string(args[0]);
        if (left_var_name) {
            auto var_slot = slot_for_variable(*left_var_name);
            if (!var_slot) {
                // We're a plain variable assignment. We can optimize out
                // doing bind matching and just alias the variable slot to the
                // slot of our right-hand side
                auto rhs_slot = compile(args[1]);
                top_varmap().emplace(*left_var_name, rhs_slot);
                return rhs_slot;
            }
        }
        // We get here if we're not just a variable assignment, ie. A more
        // complete match expression.
        binding_expr_depth++;
        auto lhs_slot = compile(args[0]);
        binding_expr_depth--;
        auto rhs_slot = compile(args[1]);
        builder.push_instr(is::hard_match{lhs_slot, rhs_slot});
        return rhs_slot;
    }

    slot_ref_t _compile_pipe(const std::vector<ast::node>& pipe_args) {
        auto lhs_node = pipe_args[0];
        auto rhs_call = pipe_args[1].as_call();
        if (!rhs_call) {
            throw std::runtime_error{"Right-hand of |> operator must be a function call"};
        }
        auto arglist = rhs_call->arguments().as_list();
        if (!arglist) {
            throw std::runtime_error{"Right-hand of |> operator must be a function call"};
        }
        auto new_args = arglist->nodes;
        new_args.insert(new_args.begin(), pipe_args[0]);
        auto new_ast
            = ast::call(rhs_call->target(), rhs_call->meta(), ast::list(std::move(new_args)));
        return compile(new_ast);
    }

    slot_ref_t _compile_case(const std::vector<ast::node>& case_args) {
        // TODO:  Replace all these asserts with real error handling
        auto arg_check = [](bool b) {
            if (!b) {
                throw std::runtime_error{"`case` expects one argument and a 'do' clause list"};
            }
        };
        arg_check(case_args.size() == 2);
        auto match_value_slot = compile(case_args[0]);
        auto kwargs           = case_args[1].as_list();
        arg_check(kwargs && kwargs->nodes.size() == 1);
        auto pair = kwargs->nodes[0].as_tuple();
        arg_check(pair && pair->nodes.size() == 2);
        auto kw_do = pair->nodes[0].as_symbol();
        arg_check(kw_do && kw_do->string() == "do");
        auto rhs = pair->nodes[1].as_list();
        arg_check(!!rhs);
        return _compile_branches(match_value_slot, *rhs);
    }

    /**
     * `cond` is really just a special case of `case`.
     */
    slot_ref_t _compile_cond(const std::vector<ast::node>& cond_args) {
        // TODO:  Replace all these asserts with real error handling
        auto arg_check = [](bool b) {
            if (!b) {
                throw std::runtime_error{"`cond` expects a single 'do' clause list"};
            }
        };
        arg_check(cond_args.size() == 1);
        auto kwargs = cond_args[0].as_list();
        arg_check(kwargs && kwargs->nodes.size() == 1);
        auto pair = kwargs->nodes[0].as_tuple();
        arg_check(pair && pair->nodes.size() == 2);
        auto kw_do = pair->nodes[0].as_symbol();
        arg_check(kw_do && kw_do->string() == "do");
        auto rhs = pair->nodes[1].as_list();
        arg_check(!!rhs);
        auto true_value = compile(ast::symbol("true"));
        return _compile_branches(true_value, *rhs);
    }

    slot_ref_t _compile_branches(const slot_ref_t         match_slot,
                                 const ast::list&         clause_list,
                                 opt_ref<const ast::meta> meta = std::nullopt) {
        // Create a binding slot where the result of the cond will go
        auto res_slot = current_end_slot;
        builder.push_instr(is::const_binding_slot{res_slot});
        consume_slot();
        return _compile_branch_clauses(match_slot, res_slot, clause_list.nodes, meta);
    }

    slot_ref_t _compile_branch_clauses(slot_ref_t                    match_slot,
                                       slot_ref_t                    res_slot,
                                       const std::vector<ast::node>& clauses,
                                       opt_ref<const ast::meta>      meta = std::nullopt) {
        const auto                 rewind_to = current_end_slot;
        std::vector<inst_offset_t> exit_inst_offsets;
        std::vector<is::jump*>     exit_instrs;
        is::false_jump*            prev_false_jump = nullptr;
        for (auto& n : clauses) {
            if (prev_false_jump) {
                // Resolve the false-jump of the prior clause
                prev_false_jump->target = current_instruction();
                // Rewind the stack for any work that the prior clause may have
                // done in its test
                builder.push_instr(is::rewind{rewind_to});
                current_end_slot = rewind_to;
            }
            auto [false_jump_target_, end_jump] = _compile_branch_clause(match_slot, res_slot, n);
            exit_instrs.push_back(end_jump);
            prev_false_jump = false_jump_target_;
        }
        // We must have compiled at least one clause, so we'll have a trailing fail target
        assert(prev_false_jump);
        prev_false_jump->target = current_instruction();
        builder.push_instr(is::rewind{rewind_to});
        current_end_slot = rewind_to;
        if (meta && meta->fn_details()) {
            // We're the tail of a function. We make a message based on the
            // meta details from the anonymous fn
            auto& dets        = *meta->fn_details();
            auto  fname_str   = dets.first + "." + dets.second;
            auto  fname_slot  = compile(ast::node(fname_str));
            auto  badarg_slot = compile("badarg"_sym);
            builder.push_instr(is::mk_tuple_3{badarg_slot, fname_slot, match_slot});
            auto raise_tup_slot = consume_slot();
            builder.push_instr(is::raise{raise_tup_slot});
        } else {
            builder.push_instr(is::no_clause{match_slot});
        }
        // Full fill the exit jumps:
        for (auto& jump : exit_instrs) {
            // Resolve the jumps at the end of each clause to jump to the end
            // of the cond block
            jump->target = current_instruction();
        }
        // Rewind _again_, after the no-clause instructions
        builder.push_instr(is::rewind{rewind_to});
        current_end_slot = rewind_to;
        return res_slot;
    }

    std::pair<is::false_jump*, is::jump*>
    _compile_branch_clause(slot_ref_t match_slot, slot_ref_t res_slot, const ast::node& n) {
        auto call = n.as_call();
        assert(call);
        auto arrow = call->target().as_symbol();
        assert(arrow);
        assert(arrow->string() == "->");
        auto args = call->arguments().as_list();
        assert(args);
        assert(args->nodes.size() == 2);
        auto lhs_list = args->nodes[0].as_list();
        assert(lhs_list);
        assert(lhs_list->nodes.size() == 1);
        auto& lhs = lhs_list->nodes[0];
        auto& rhs = args->nodes[1];
        // Each clause gets it's own new scope
        variable_scopes.emplace_back();
        // Compile the test expression:
        binding_expr_depth++;
        clause_test_depth++;
        auto test_slot = compile(lhs);
        clause_test_depth--;
        binding_expr_depth--;
        // Do a conditional jump
        builder.push_instr(is::try_match{test_slot, match_slot});
        // Jump will be resolved later:
        auto fail_jump = &builder.push_instr(is::false_jump{invalid_inst});
        // Now compile our right-hand side
        auto rhs_slot = compile(rhs);
        // Add an instruction to put the result of our RHS into the result slot
        builder.push_instr(is::hard_match{res_slot, rhs_slot});
        // Jump to the rewind trampoline
        auto exit_jump = &builder.push_instr(is::jump{invalid_inst});
        variable_scopes.pop_back();
        return {fail_jump, exit_jump};
    }

    void _find_closure_variables(const ast::node& node, capture_list& dest) {
        node.visit([&](const auto& real) -> void { _do_find_closure_variables(real, dest); });
    }

    void _do_find_closure_variables(const ast::list& l, capture_list& dest) {
        for (auto& n : l.nodes) {
            _find_closure_variables(n, dest);
        }
    }

    void _do_find_closure_variables(const ast::tuple& t, capture_list& dest) {
        for (auto& n : t.nodes) {
            _find_closure_variables(n, dest);
        }
    }

    void _do_find_closure_variables(ast::integer, capture_list&) {}
    void _do_find_closure_variables(ast::floating, capture_list&) {}
    void _do_find_closure_variables(const ast::symbol&, capture_list&) {}
    void _do_find_closure_variables(const ast::string&, capture_list&) {}
    void _do_find_closure_variables(const ast::call& call, capture_list& dest) {
        if (auto var_sym = call.arguments().as_symbol(); var_sym && var_sym->string() == "Var") {
            auto lhs_sym = call.target().as_symbol();
            assert(lhs_sym);
            auto var_slot = slot_for_variable(lhs_sym->string());
            if (!var_slot) {
                // Not a variable in scope.
                return;
            }
            dest.push_back(capture{lhs_sym->string(), *var_slot, dest.size()});
        } else {
            _find_closure_variables(call.target(), dest);
            _find_closure_variables(call.arguments(), dest);
        }
    }

    /**
     * Compile an anonymous function. This is tough, and the basis for all
     * abstractions.
     */
    slot_ref_t _compile_anon_fn(const std::vector<ast::node>& args, const ast::meta& meta) {
        // To find the variable captures, we'll do a pass over the anonymous
        // function in search of variables from the parent scopes.
        capture_list captures;
        for (auto& n : args) {
            _find_closure_variables(n, captures);
        }
        auto old_scope  = std::move(variable_scopes);
        variable_scopes = {};
        variable_scopes.emplace_back();
        // Bring the captures we've discovered into scope
        for (auto& cap : captures) {
            top_varmap().emplace(cap.varname, cap.inner_slot);
        }
        // We start with a new basis slot, and reserve space for the captures
        auto old_top_slot = current_end_slot;
        current_end_slot  = slot_ref_t{captures.size()};
        // A jump to jump over the anon fn:
        auto& jump_over_inst = builder.push_instr(is::jump{invalid_inst});
        // Save the stack top. The fn will run with a clean stack
        // The first instruction in the fn code:
        const auto code_begin = current_instruction();
        // Start by identifying the frame
        if (auto fn_meta = meta.fn_details()) {
            auto frame_id = fn_meta->first + "." + fn_meta->second;
            builder.push_instr(is::frame_id{std::move(frame_id)});
        } else {
            builder.push_instr(is::frame_id{"<anonymous-function>"});
        }
        // Compile the fn body:
        auto ret_slot = _compile_anon_fn_inner(args, meta);
        // Generate the final return instruction:
        builder.push_instr(is::ret{ret_slot});
        auto code_end = current_instruction();
        // Fulfill the jump-over:
        jump_over_inst.target = current_instruction();
        // An inst that will create the closure when we pass over the anon fn expr
        std::vector<slot_ref_t> closure_slots;
        for (auto& cap : captures) {
            closure_slots.push_back(cap.parent_slot);
        }
        builder.push_instr(is::mk_closure{code_begin, code_end, std::move(closure_slots)});
        // Restore the state of the compile before we started the fn
        variable_scopes  = std::move(old_scope);
        current_end_slot = old_top_slot;
        return consume_slot();
    }

    slot_ref_t _compile_anon_fn_inner(const std::vector<ast::node>& args, const ast::meta& meta) {
        // The argument is always the first slot (after captures):
        const slot_ref_t arg_slot = consume_slot();
        // Convert the clause list into a case statement that we match against
        // with the argument list
        std::vector<ast::node> new_clauses;
        for (auto& clause : args) {
            auto call = clause.as_call();
            assert(call);
            auto arg_list = call->arguments().as_list();
            assert(arg_list);
            auto fn_args     = arg_list->nodes[0];
            auto fn_arg_list = fn_args.as_list();
            assert(fn_arg_list);
            auto                   fn_arg_tup = ast::tuple(std::move(fn_arg_list->nodes));
            std::vector<ast::node> new_args;
            std::vector<ast::node> oneoff;
            oneoff.push_back(std::move(fn_arg_tup));
            new_args.push_back(ast::list(std::move(oneoff)));
            new_args.push_back(arg_list->nodes[1]);
            new_clauses.emplace_back(
                ast::node(ast::call(symbol("->"), {}, ast::list(std::move(new_args)))));
        }
        // Compile the code for the actual anon fn
        return _compile_branches(arg_slot, ast::list(std::move(new_clauses)), meta);
    }

    /**
     * Compile a list cons, that is: [hd|tail]
     */
    slot_ref_t _compile_cons(const ast::node& args) {
        if (binding_expr_depth != 0) {
            return _compile_cons_pop(args);
        } else {
            return _compile_cons_push(args);
        }
    }

    slot_ref_t _compile_cons_pop(const ast::node& args_) {
        auto arglist = args_.as_list();
        assert(arglist);
        macro_argument_parser args{*arglist};
        if (args.count() != 2) {
            throw std::runtime_error{"Cons expects two arguments"};
        }
        auto lhs_slot = compile(args.nth(0));
        auto rhs_slot = compile(args.nth(1));
        builder.push_instr(is::mk_cons{lhs_slot, rhs_slot});
        return consume_slot();
    }

    slot_ref_t _compile_cons_push(const ast::node& args_) {
        auto arglist = args_.as_list();
        assert(arglist);
        macro_argument_parser args{*arglist};
        if (args.count() != 2) {
            throw std::runtime_error{"Cons expects two arguments"};
        }
        auto lhs_slot = compile(args.nth(0));
        auto rhs_slot = compile(args.nth(1));
        builder.push_instr(is::push_front{lhs_slot, rhs_slot});
        return consume_slot();
    }

    slot_ref_t _compile_quote(const ast::list& args) {
        assert(args.nodes.size() == 1 && "Invalid arguments to quote");
        auto kwargs = args.nodes[0].as_list();
        assert(kwargs && "Wanted kwargs for quote");
        assert(kwargs->nodes.size() == 1 && "Want one keyword arg for quote");
        auto pair = kwargs->nodes[0].as_tuple();
        assert(pair && "Expected keyword pair for quote");
        assert(pair->nodes.size() == 2 && "Invalid kw pair");
        auto kw_do = pair->nodes[0].as_symbol();
        assert(kw_do && "Expected symbol");
        assert(kw_do->string() == "do");
        auto& rhs = pair->nodes[1];
        return _compile_quoted(rhs);
    }

    slot_ref_t _compile_quoted(const ast::node& node) { return node.visit(*this, expand_quoted{}); }

    /**
     * ========================================================================
     * The std::visit cases. These are executed by the compile() method
     * ========================================================================
     */

    slot_ref_t operator()(const ast::list& l) {
        // Yeah, the indentation is bad, but we have to do some careful checking
        // for a cons
        if (l.nodes.size() == 1) {
            if (auto inner_call = l.nodes[0].as_call()) {
                if (auto call_sym = inner_call->target().as_symbol();
                    call_sym && call_sym->string() == "|") {
                    // ITS A CONS
                    return _compile_cons(inner_call->arguments());
                }
            }
        }
        // Not a const, just a list
        std::vector<slot_ref_t> slots;
        for (auto& n : l.nodes) {
            auto next_slot = compile(n);
            slots.push_back(next_slot);
        }
        builder.push_instr(is::mk_list{std::move(slots)});
        return consume_slot();
    }

    slot_ref_t operator()(const ast::list& list, expand_quoted) {
        std::vector<slot_ref_t> slots;
        for (auto& el : list.nodes) {
            slots.push_back(_compile_quoted(el));
        }
        builder.push_instr(is::mk_list{std::move(slots)});
        return consume_slot();
    }

    /**
     * Compile a tuple
     */
    slot_ref_t operator()(const ast::tuple& tup) { return _compile_tuple(tup.nodes); }

    slot_ref_t operator()(const ast::tuple& t, expand_quoted) {
        std::vector<slot_ref_t> slots;
        for (auto& el : t.nodes) {
            slots.push_back(_compile_quoted(el));
        }
        // TODO: Branch based on slot count
        builder.push_instr(is::mk_tuple_n{std::move(slots)});
        return consume_slot();
    }

    /**
     * Compile a call. This is also where we "expand" intrinsic macros.
     *
     * Intrinsic macros include simple binary operators as well as some special
     * build-ins that are generated by the parser.
     *
     * DOES NOT expand any other macros. Those should have already been expanded
     * by another AST transformer.
     */
    slot_ref_t operator()(const ast::call& call) {
        auto& args = call.arguments();
        if (auto arg_list_ptr = args.as_list()) {
            // We're given an argument list. A regular function call
            return _compile_call(call.target(), call.meta(), *arg_list_ptr);
        } else {
            // We're a variable reference
            auto var_sym = call.target().as_symbol();
            if (!var_sym) {
                // Var name should be a symbol
                assert(false && "TODO: Compile error");
            }
            auto var_slot = slot_for_variable(var_sym->string());
            if (var_slot) {
                return *var_slot;
            } else {
                // Unbound variable
                if (binding_expr_depth != 0) {
                    // We're binding a new variable here
                    builder.push_instr(is::const_binding_slot{current_end_slot});
                    auto new_var_slot = consume_slot();
                    // Announce the slot for the variable:
                    top_varmap().emplace(var_sym->string(), new_var_slot);
                    return new_var_slot;
                } else {
                    throw std::runtime_error{"Name '" + var_sym->string()
                                             + "' does not name a variable bound at this scope."};
                }
            }
        }
    }

    slot_ref_t operator()(const ast::call& c, expand_quoted) {
        auto target = _compile_quoted(c.target());
        auto meta   = _compile_quoted(ast::node(ast::list()));
        auto args   = _compile_quoted(c.arguments());
        builder.push_instr(is::mk_tuple_3{target, meta, args});
        return consume_slot();
    }

    /**
     * Compile an integer literal. Simple.
     */
    slot_ref_t operator()(ast::integer i, expand_quoted = {}) {
        builder.push_instr(is::const_int{i});
        return consume_slot();
    }

    /**
     * Floating point
     */
    slot_ref_t operator()(ast::floating, expand_quoted = {}) {
        // TODO
        return invalid_slot;
    }

    /**
     * Symbols
     */
    slot_ref_t operator()(const ast::symbol& s, expand_quoted = {}) {
        builder.push_instr(is::const_symbol{s});
        return consume_slot();
    }

    slot_ref_t operator()(const ast::string& s, expand_quoted = {}) {
        builder.push_instr(is::const_str{s});
        return consume_slot();
    }
};
}  // namespace

code::code let::compile(const ast::node& node) {
    let::code::code_builder builder;
    block_compiler          comp{builder};
    comp.compile_root(node);
    return builder.save();
    // block_compiler comp;
    // comp.compile_root(node);
    // return exec::block(std::move(comp.instrs));
}
