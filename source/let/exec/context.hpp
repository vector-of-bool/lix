#ifndef LET_EXEC_CONTEXT_HPP_INCLUDED
#define LET_EXEC_CONTEXT_HPP_INCLUDED

#include <let/exec/closure.hpp>
#include <let/exec/module.hpp>
#include <let/exec/stack.hpp>

#include <let/code/code.hpp>
#include <let/code/instr.hpp>

#include <let/boxed.hpp>

#include <cassert>
#include <map>
#include <optional>
#include <stack>
#include <vector>

namespace let {

namespace exec {

class block;
class instruction;
using instruction_iterator = std::vector<instruction>::const_iterator;

enum class status {
    done,
    more,
};

namespace detail {

class frame {
    code::code     _code;
    code::iterator _first_instr   = _code.begin();
    code::iterator _current_instr = _first_instr;
    code::iterator _end_instr     = _code.end();
    stack          _stack;

public:
    explicit frame(code::code c)
        : _code(c) {}

    explicit frame(code::code c, code::iterator instr_inner)
        : _code(c)
        , _current_instr(instr_inner) {}

    const auto& get_and_advance() {
        assert(_current_instr != _end_instr);
        return *_current_instr++;
    }

    const let::value& nth(slot_ref_t off) const { return _stack.nth(off); }

    const let::value& top() const {
        assert(_stack.size() != 0);
        return nth({_stack.size() - 1});
    }

    void push(let::value el) { _stack.push(std::move(el)); }
    void pop() {
        assert(_stack.size() != 0);
        rewind({_stack.size() - 1});
    }
    void bind_slot(slot_ref_t slot, let::value el) {
        auto& dest = _stack.nth_mut(slot);
        assert(dest.as_binding_slot() && "Binding to non-binding slot");
        dest = std::move(el);
    }

    void jump(inst_offset_t target) {
        auto avail = std::distance(_first_instr, _end_instr);
        assert(target.index > 0);
        assert(target.index < static_cast<std::size_t>(avail));
        _current_instr = _first_instr + target.index;
    }

    void rewind(slot_ref_t new_top) { _stack.rewind(new_top); }

    const code::code& code() const { return _code; }
};

class instr_visitor;

class context_impl;

}  // namespace detail

class context {
    std::unique_ptr<detail::context_impl> _impl;

    void _push_environment();
    void _pop_environment();

public:
    context();
    context(code::code);
    ~context();
    context(context&&);
    context& operator=(context&&);

    std::optional<let::value> execute_n(std::size_t n);

    void push_frame(code::code c) { push_frame(c, c.begin()); }
    void push_frame(code::code c, code::iterator current);
    let::value execute_frame(code::code c);
    let::value execute_frame(code::code c, code::iterator);
    void pop_frame_return(slot_ref_t r);
    const let::value& nth(slot_ref_t n) const;
    const let::value& top() const;
    void push(let::value e);
    void jump(inst_offset_t);
    void set_test_state(bool);
    bool get_test_state() const noexcept;
    void rewind(slot_ref_t);
    void bind_slot(slot_ref_t slot, let::value e);

    std::optional<module> get_module(const std::string& name) const;

    void                      set_environment_value(const std::string&, let::value);
    std::optional<let::value> get_environment_value(const std::string& name) const;

    // void push_frame(code::iterator first, code::iterator last, code::iterator inner_first) {
    //     _frames.emplace(_stack, first, last, inner_first);
    // }

    // void pop_frame_no_ret();

    void register_module(const std::string& name, module mod);

    const code::code& current_code() const;

    template <typename Func>
    auto push_environment(Func&& fn) {
        try {
            _push_environment();
            auto&& ret = std::forward<Func>(fn)();
            _pop_environment();
            return std::forward<decltype(ret)>(ret);
        } catch (...) {
            _pop_environment();
            throw;
        }
    }
};

}  // namespace exec

}  // namespace let

#endif  // LET_EXEC_CONTEXT_HPP_INCLUDED