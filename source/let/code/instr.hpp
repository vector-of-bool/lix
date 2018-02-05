#ifndef LET_CODE_INSTR_HPP_INCLUDED
#define LET_CODE_INSTR_HPP_INCLUDED

#include <let/code/types.hpp>

#include <cassert>
#include <string>
#include <variant>
#include <vector>

namespace let::code {

namespace is_types {

struct ret {
    slot_ref_t slot;
};
struct call {
    slot_ref_t fn;
    slot_ref_t arg;
};
struct add {
    slot_ref_t a;
    slot_ref_t b;
};
struct sub {
    slot_ref_t a;
    slot_ref_t b;
};
struct eq {
    slot_ref_t a;
    slot_ref_t b;
};
struct const_int {
    std::int64_t value;
};
struct const_double {
    double value;
};
struct const_symbol {
    std::string string;
    explicit const_symbol(std::string s)
        : string(std::move(s)) {}
};
struct hard_match {
    slot_ref_t lhs;
    slot_ref_t rhs;
};
struct const_binding_slot {
    slot_ref_t slot;
    explicit const_binding_slot(slot_ref_t i)
        : slot(i) {}
};
struct mk_tuple_0 {};
struct mk_tuple_1 {
    slot_ref_t a;
};
struct mk_tuple_2 : mk_tuple_1 {
    slot_ref_t b;
};
struct mk_tuple_3 : mk_tuple_2 {
    slot_ref_t c;
};
struct mk_tuple_4 : mk_tuple_3 {
    slot_ref_t d;
};
struct mk_tuple_5 : mk_tuple_4 {
    slot_ref_t e;
};
struct mk_tuple_6 : mk_tuple_5 {
    slot_ref_t f;
};
struct mk_tuple_7 : mk_tuple_6 {
    slot_ref_t g;
};
struct mk_tuple_n {
    std::vector<slot_ref_t> slots;
};
struct mk_list {
    std::vector<slot_ref_t> slots;
};
struct mk_closure {
    inst_offset_t code_begin;
    inst_offset_t code_end;
};
struct jump {
    inst_offset_t target;
};
struct test_true {
    slot_ref_t slot;
};
struct try_match {
    slot_ref_t lhs;
    slot_ref_t rhs;
};
struct false_jump {
    inst_offset_t target;
};
struct rewind {
    slot_ref_t slot;
};
struct dot {
    slot_ref_t object;
    slot_ref_t attr_name;
};
struct no_clause {};
struct debug {};

using any_var = std::variant<ret,
                             call,
                             add,
                             sub,
                             eq,
                             const_int,
                             const_symbol,
                             hard_match,
                             try_match,
                             const_binding_slot,
                             mk_tuple_0,
                             mk_tuple_1,
                             mk_tuple_2,
                             mk_tuple_3,
                             mk_tuple_4,
                             mk_tuple_5,
                             mk_tuple_6,
                             mk_tuple_7,
                             mk_tuple_n,
                             mk_list,
                             jump,
                             test_true,
                             false_jump,
                             rewind,
                             no_clause,
                             dot,
                             mk_closure>;

}  // namespace is_types

class instr {
    is_types::any_var _inst;

public:
    template <typename Inst,
              typename = std::enable_if_t<std::is_convertible<Inst&&, is_types::any_var>::value>>
    instr(Inst&& inst)
        : _inst(std::forward<Inst>(inst)) {}

    template <typename Fun>
    decltype(auto) visit(Fun&& fn) const {
        return std::visit(std::forward<Fun>(fn), _inst);
    }

    void set_jump_target(inst_offset_t target) {
        if (auto ptr = std::get_if<is_types::jump>(&_inst)) {
            ptr->target = target;
        } else if (auto ptr = std::get_if<is_types::false_jump>(&_inst)) {
            ptr->target = target;
        } else {
            assert(false && "set_jump_target() on non-jump instruction");
        }
    }

    is_types::any_var&       instr_var() { return _inst; }
    const is_types::any_var& instr_var() const { return _inst; }
};

}  // namespace let::code

#endif  // LET_CODE_INSTR_HPP_INCLUDED