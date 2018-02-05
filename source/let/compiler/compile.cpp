#include "compile.hpp"

#include <let/parser/node.hpp>

#include <let/code/builder.hpp>
#include <let/code/code.hpp>

#include <cassert>
#include <list>
#include <map>
#include <optional>
#include <stack>

using namespace let;
// namespace is  = let::exec::instructions;
namespace is2 = let::code::is_types;

namespace {

using let::code::slot_ref_t;
using let::code::inst_offset_t;

constexpr slot_ref_t    invalid_slot = {static_cast<std::size_t>(-1)};
constexpr inst_offset_t invalid_inst = {static_cast<std::size_t>(-1)};
using varslot_map                    = std::map<std::string, slot_ref_t>;

struct expand_quoted {};

using varscope_stack = std::vector<varslot_map>;

struct block_compiler2 {
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

    explicit block_compiler2(code::code_builder& b)
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
        builder.push_instr(is2::ret{ret_slot});
        return ret_slot;
    }

    /**
     * Compile a tuple expression. We have dedicated bytecode for each arity up to 7
     */
    slot_ref_t _compile_tuple(const std::vector<ast::node>& nodes) {
        switch (nodes.size()) {
        case 0:
            builder.push_instr(is2::mk_tuple_0{});
            return consume_slot();
        case 1: {
            auto a = compile(nodes[0]);
            builder.push_instr(is2::mk_tuple_1{a});
            return consume_slot();
        }
        case 2: {
            auto a = compile(nodes[0]);
            auto b = compile(nodes[1]);
            builder.push_instr(is2::mk_tuple_2{a, b});
            return consume_slot();
        }
        case 3: {
            auto a = compile(nodes[0]);
            auto b = compile(nodes[1]);
            auto c = compile(nodes[2]);
            builder.push_instr(is2::mk_tuple_3{a, b, c});
            return consume_slot();
        }
        case 4: {
            auto a = compile(nodes[0]);
            auto b = compile(nodes[1]);
            auto c = compile(nodes[2]);
            auto d = compile(nodes[3]);
            builder.push_instr(is2::mk_tuple_4{a, b, c, d});
            return consume_slot();
        }
        case 5: {
            auto a = compile(nodes[0]);
            auto b = compile(nodes[1]);
            auto c = compile(nodes[2]);
            auto d = compile(nodes[3]);
            auto e = compile(nodes[4]);
            builder.push_instr(is2::mk_tuple_5{a, b, c, d, e});
            return consume_slot();
        }
        case 6: {
            auto a = compile(nodes[0]);
            auto b = compile(nodes[1]);
            auto c = compile(nodes[2]);
            auto d = compile(nodes[3]);
            auto e = compile(nodes[4]);
            auto f = compile(nodes[5]);
            builder.push_instr(is2::mk_tuple_6{a, b, c, d, e, f});
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
            builder.push_instr(is2::mk_tuple_7{a, b, c, d, e, f, g});
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

    slot_ref_t _compile_call(const ast::node& lhs, const ast::list& args) {
        auto lhs_sym = lhs.as_symbol();
        if (lhs_sym) {
            auto& lhs_str = lhs_sym->string();
            if (lhs_str == "+") {
                _check_binary(args);
                auto lhs_slot = compile(args.nodes[0]);
                auto rhs_slot = compile(args.nodes[1]);
                builder.push_instr(is2::add{lhs_slot, rhs_slot});
                return consume_slot();
            } else if (lhs_str == "-") {
                _check_binary(args);
                auto lhs_slot = compile(args.nodes[0]);
                auto rhs_slot = compile(args.nodes[1]);
                builder.push_instr(is2::sub{lhs_slot, rhs_slot});
                return consume_slot();
            } else if (lhs_str == "=") {
                _check_binary(args);
                binding_expr_depth++;
                auto lhs_slot = compile(args.nodes[0]);
                binding_expr_depth--;
                auto rhs_slot = compile(args.nodes[1]);
                builder.push_instr(is2::hard_match{lhs_slot, rhs_slot});
                return rhs_slot;
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
                builder.push_instr(is2::eq{lhs_slot, rhs_slot});
                return consume_slot();
            } else if (lhs_str == "cond") {
                return _compile_cond(args.nodes);
            } else if (lhs_str == "case") {
                return _compile_case(args.nodes);
            } else if (lhs_str == "quote") {
                return _compile_quote(args);
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
                    builder.push_instr(is2::dot{lhs_slot, rhs_slot});
                    return consume_slot();
                }
            } else if (lhs_str == "fn") {
                return _compile_anon_fn(args.nodes);
            }
        }
        // No special function
        auto                    fn_slot = compile(lhs);
        std::vector<slot_ref_t> arg_slots;
        for (auto& arg : args.nodes) {
            arg_slots.push_back(compile(arg));
        }
        builder.push_instr(is2::mk_tuple_n{std::move(arg_slots)});
        auto arg_slot = consume_slot();
        builder.push_instr(is2::call{fn_slot, arg_slot});
        return consume_slot();
    }

    slot_ref_t _compile_case(const std::vector<ast::node>& case_args) {
        // TODO:  Replace all these asserts with real error handling
        assert(case_args.size() == 2 && "Invalid arguments to case");
        auto match_value_slot = compile(case_args[0]);
        auto kwargs           = case_args[1].as_list();
        assert(kwargs && "Wanted kwargs for cond");
        assert(kwargs->nodes.size() == 1 && "Want one keyword arg for cond");
        auto pair = kwargs->nodes[0].as_tuple();
        assert(pair && "Expected keyword pair for cond");
        assert(pair->nodes.size() == 2 && "Invalid kw pair");
        auto kw_do = pair->nodes[0].as_symbol();
        assert(kw_do && "Expected symbol");
        assert(kw_do->string() == "do");
        auto rhs = pair->nodes[1].as_list();
        assert(rhs && "Expected list for rhs of `do' argument");
        return _compile_branches(match_value_slot, *rhs);
    }

    /**
     * `cond` is really just a special case of `case`.
     */
    slot_ref_t _compile_cond(const std::vector<ast::node>& cond_args) {
        // TODO:  Replace all these asserts with real error handling
        assert(cond_args.size() == 1 && "Invalid arguments to cond");
        auto kwargs = cond_args[0].as_list();
        assert(kwargs && "Wanted kwargs for cond");
        assert(kwargs->nodes.size() == 1 && "Want one keyword arg for cond");
        auto pair = kwargs->nodes[0].as_tuple();
        assert(pair && "Expected keyword pair for cond");
        assert(pair->nodes.size() == 2 && "Invalid kw pair");
        auto kw_do = pair->nodes[0].as_symbol();
        assert(kw_do && "Expected symbol");
        assert(kw_do->string() == "do");
        auto rhs = pair->nodes[1].as_list();
        assert(rhs && "Expected list for rhs of `do' argument");
        auto true_value = compile(ast::symbol("true"));
        return _compile_branches(true_value, *rhs);
    }

    slot_ref_t _compile_branches(const slot_ref_t match_slot, const ast::list& clause_list) {
        // Create a binding slot where the result of the cond will go
        auto res_slot = current_end_slot;
        builder.push_instr(is2::const_binding_slot{res_slot});
        consume_slot();
        return _compile_branch_clauses(match_slot, res_slot, clause_list.nodes);
    }

    slot_ref_t _compile_branch_clauses(slot_ref_t                    match_slot,
                                       slot_ref_t                    res_slot,
                                       const std::vector<ast::node>& clauses) {
        const auto                 rewind_to = current_end_slot;
        std::vector<inst_offset_t> exit_inst_offsets;
        std::vector<is2::jump*>    exit_instrs;
        is2::false_jump*           prev_false_jump = nullptr;
        for (auto& n : clauses) {
            if (prev_false_jump) {
                // Resolve the false-jump of the prior clause
                prev_false_jump->target = current_instruction();
                // Rewind the stack for any work that the prior clause may have
                // done in its test
                builder.push_instr(is2::rewind{rewind_to});
                current_end_slot = rewind_to;
            }
            auto[false_jump_target_, end_jump] = _compile_branch_clause(match_slot, res_slot, n);
            exit_instrs.push_back(end_jump);
            prev_false_jump = false_jump_target_;
        }
        // We must have compiled at least one clause, so we'll have a trailing fail target
        assert(prev_false_jump);
        // TODO: Add a cond-failure case
        prev_false_jump->target = current_instruction();
        builder.push_instr(is2::no_clause{});
        // Full fill the exit jumps:
        for (auto& jump : exit_instrs) {
            // Resolve the jumps at the end of each clause to jump to the end
            // of the cond block
            jump->target = current_instruction();
        }
        builder.push_instr(is2::rewind{rewind_to});
        current_end_slot = rewind_to;
        return res_slot;
    }

    std::pair<is2::false_jump*, is2::jump*>
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
        auto test_slot = compile(lhs);
        binding_expr_depth--;
        // Do a conditional jump
        builder.push_instr(is2::try_match{test_slot, match_slot});
        // Jump will be resolved later:
        auto fail_jump = &builder.push_instr(is2::false_jump{invalid_inst});
        // Now compile our right-hand side
        auto rhs_slot = compile(rhs);
        // Add an instruction to put the result of our RHS into the result slot
        builder.push_instr(is2::hard_match{res_slot, rhs_slot});
        // Jump to the rewind trampoline
        auto exit_jump = &builder.push_instr(is2::jump{invalid_inst});
        variable_scopes.pop_back();
        return {fail_jump, exit_jump};
    }

    /**
     * Compile an anonymous function. This is tough, and the basis for all
     * abstractions.
     */
    slot_ref_t _compile_anon_fn(const std::vector<ast::node>& args) {
        // Compiling an anon fn requires that we accumulate a list of closure
        // slots. The values are sealed once the anon fn expression is evaluated.
        auto& jump_over_inst = builder.push_instr(is2::jump{invalid_inst});
        // Save the stack top. The fn will run with a clean stack
        const auto old_top    = current_end_slot;
        current_end_slot      = slot_ref_t{0};
        const auto code_begin = current_instruction();
        // Convert the clause list into a case statement that we match against
        // with the argument list
        // The argument is always the first slot:
        const slot_ref_t       arg_slot = consume_slot();
        std::vector<ast::node> new_clauses;
        for (auto& clause : args) {
            auto call = clause.as_call();
            assert(call);
            auto arg_list = call->arguments().as_list();
            assert(arg_list);
            auto fn_args     = arg_list->nodes[0].clone();
            auto fn_arg_list = fn_args.as_list();
            assert(fn_arg_list);
            auto                   fn_arg_tup = ast::tuple(std::move(fn_arg_list->nodes));
            std::vector<ast::node> new_args;
            std::vector<ast::node> oneoff;
            oneoff.push_back(std::move(fn_arg_tup));
            new_args.push_back(ast::list(std::move(oneoff)));
            new_args.push_back(arg_list->nodes[1].clone());
            new_clauses.emplace_back(
                ast::node(ast::call(symbol("->"), {}, ast::list(std::move(new_args)))));
        }
        // Compile the code for the actual anon fn
        auto ret_slot = _compile_branches(arg_slot, ast::list(std::move(new_clauses)));
        builder.push_instr(is2::ret{ret_slot});
        auto code_end = current_instruction();
        // An inst that will create the closure when we pass over the anon fn expr
        jump_over_inst.target = current_instruction();
        builder.push_instr(is2::mk_closure{code_begin, code_end});
        current_end_slot = old_top;
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
        std::vector<slot_ref_t> slots;
        for (auto& n : l.nodes) {
            auto next_slot = compile(n);
            slots.push_back(next_slot);
        }
        builder.push_instr(is2::mk_list{std::move(slots)});
        return consume_slot();
    }

    slot_ref_t operator()(const ast::list& list, expand_quoted) {
        std::vector<slot_ref_t> slots;
        for (auto& el : list.nodes) {
            slots.push_back(_compile_quoted(el));
        }
        builder.push_instr(is2::mk_list{std::move(slots)});
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
        builder.push_instr(is2::mk_tuple_n{std::move(slots)});
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
            return _compile_call(call.target(), *arg_list_ptr);
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
                    builder.push_instr(is2::const_binding_slot{current_end_slot});
                    auto new_var_slot = consume_slot();
                    // Announce the slot for the variable:
                    top_varmap().emplace(var_sym->string(), new_var_slot);
                    return new_var_slot;
                } else {
                    assert(false && "TODO: compile error (unbound variable)");
                }
            }
        }
    }

    slot_ref_t operator()(const ast::call& c, expand_quoted) {
        auto target = _compile_quoted(c.target());
        auto meta   = _compile_quoted(ast::node(ast::list()));
        auto args   = _compile_quoted(c.arguments());
        builder.push_instr(is2::mk_tuple_3{target, meta, args});
        return consume_slot();
    }

    /**
     * Compile an integer literal. Simple.
     */
    slot_ref_t operator()(ast::integer i, expand_quoted = {}) {
        builder.push_instr(is2::const_int{i});
        return consume_slot();
    }

    /**
     * Floating point
     */
    slot_ref_t operator()(ast::floating, expand_quoted = {}) {
        // TODO
        return invalid_slot;
    }

    slot_ref_t operator()(const ast::symbol& s, expand_quoted = {}) {
        builder.push_instr(is2::const_symbol{s.string()});
        return consume_slot();
    }
};
}  // namespace

code::code let::compile(const ast::node& node) {
    let::code::code_builder builder;
    block_compiler2         comp{builder};
    comp.compile_root(node);
    return builder.save();
    // block_compiler comp;
    // comp.compile_root(node);
    // return exec::block(std::move(comp.instrs));
}
