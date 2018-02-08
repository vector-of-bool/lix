#include "exec.hpp"

using namespace let;
using namespace let::exec;

namespace is = let::code::is_types;

namespace let::exec::detail {

class exec_frame {
    code::code     _code;
    code::iterator _first_instr   = _code.begin();
    code::iterator _current_instr = _first_instr;
    code::iterator _end_instr     = _code.end();
    stack          _stack;

public:
    explicit exec_frame(code::code c)
        : _code(c) {}

    explicit exec_frame(code::code c, code::iterator instr_inner)
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

class executor_impl {
public:
    std::stack<exec_frame>    _call_frames;
    bool                      _test_state = false;
    std::optional<let::value> _bottom_ret;

    exec_frame& _top_frame() {
        assert(!_call_frames.empty());
        return _call_frames.top();
    }

    const exec_frame& _top_frame() const {
        assert(!_call_frames.empty());
        return _call_frames.top();
    }

    const let::value& nth(slot_ref_t n) const { return _top_frame().nth(n); }

    void pop_frame_return(slot_ref_t r) {
        auto rv = _top_frame().nth(r);
        _call_frames.pop();
        if (_call_frames.empty()) {
            _bottom_ret.emplace(std::move(rv));
        } else {
            _top_frame().push(std::move(rv));
        }
    }

    void push_frame(code::code c, code::iterator inst) { _call_frames.emplace(c, inst); }

    void push(value&& el) { _top_frame().push(std::move(el)); }
    void push(const value& el) { _top_frame().push(el); }
    void rewind(slot_ref_t slot) { _top_frame().rewind(slot); }

    void bind_slot(slot_ref_t slot, const let::value& el) { _top_frame().bind_slot(slot, el); }

    void jump(inst_offset_t target) { _top_frame().jump(target); }

    inline void _exec_one(context& ctx);

    const code::code& current_code() const noexcept { return _top_frame().code(); }

    std::optional<let::value> execute_n(std::size_t n, context& ctx) {
        while (!_call_frames.empty() && n) {
            _exec_one(ctx);
            --n;
        }
        if (_call_frames.empty()) {
            assert(_bottom_ret);
            return _bottom_ret;
        } else {
            return std::nullopt;
        }
    }
};

struct exec_visitor {
    let::exec::context&               ctx;
    let::exec::detail::executor_impl& ex;

    void execute(is::const_int i) { ex.push(i.value); }
    void execute(is::const_double d) { ex.push(d.value); }
    void execute(const is::const_symbol& sym) { ex.push(let::symbol{sym.string}); }
    void execute(const is::const_str& str) { ex.push(let::string{str.string}); }
    void execute(is::const_binding_slot s) { ex.push(binding_slot{s.slot}); }

    void execute(is::ret r) { ex.pop_frame_return(r.slot); }

    void execute(is::call c) {
        auto& callee = ex.nth(c.fn);
        if (auto closure = callee.as_closure()) {
            auto arg = ex.nth(c.arg);
            ex.push_frame(closure->code(), closure->code_begin());
            // Push the values it has captured
            for (auto& el : closure->captures()) {
                ex.push(el);
            }
            ex.push(std::move(arg));
        } else if (auto fn = callee.as_function()) {
            ex.push(fn->call_ll(ctx, ex.nth(c.arg)));
        } else {
            assert(false && "Call to non-function");
        }
    }

    void execute(is::jump j) { ex.jump(j.target); }

    void execute(is::test_true t) {
        auto value     = ex.nth(t.slot).as_symbol();
        ex._test_state = value && value->string() == "true";
    }

    void execute(is::false_jump j) {
        if (ex._test_state == false) {
            ex.jump(j.target);
        }
    }

    void execute(is::rewind r) { ex.rewind(r.slot); }

    void execute(is::add add) {
        auto lhs = ex.nth(add.a).as_integer();
        auto rhs = ex.nth(add.b).as_integer();
        // TODO: Check for nullptr
        ex.push(*lhs + *rhs);
    }
    void execute(is::sub sub) {
        auto lhs = ex.nth(sub.a).as_integer();
        auto rhs = ex.nth(sub.b).as_integer();
        // TODO: Check for nullptr;
        ex.push(*lhs - *rhs);
    }

    void execute(is::eq eq) {
        // Compare operands for equality/equivalence
        auto& lhs       = ex.nth(eq.a);
        auto& rhs       = ex.nth(eq.b);
        auto  are_equal = lhs == rhs;
        ex.push(are_equal ? let::symbol{"true"} : let::symbol{"false"});
    }

    template <typename LHS, typename RHS>
    void _match_failure(const LHS&, const RHS&) {
        assert(false && "TODO: Match failure");
        std::terminate();
    }

    bool _do_match(const tuple& lhs, const tuple& rhs) {
        if (lhs.size() != rhs.size()) {
            return false;
        }
        for (auto i = 0u; i < lhs.size(); ++i) {
            auto did_match = _match(lhs[i], rhs[i]);
            if (!did_match) {
                return false;
            }
        }
        return true;
    }

    bool _do_match(const let::list& lhs, const let::list& rhs) {
        if (lhs.size() != rhs.size()) {
            return false;
        }
        auto       lhs_iter = lhs.begin();
        const auto lhs_end  = lhs.end();
        auto       rhs_iter = rhs.begin();
        while (lhs_iter != lhs_end) {
            auto did_match = _match(*lhs_iter, *rhs_iter);
            if (!did_match) {
                return false;
            }
            ++lhs_iter;
            ++rhs_iter;
        }
        return true;
    }

    bool _do_match(const cons& lhs, const let::list& list) {
        auto& head = lhs.head;
        auto& tail = lhs.tail;
        if (list.size() < 1) {
            return false;
        }
        auto head_matched = _match(head, *list.begin());
        if (!head_matched) {
            return false;
        }
        return _match(tail, list.pop_front());
    }

    bool _match(const let::value& lhs, const let::value& rhs) {
        if (auto bind_slot = lhs.as_binding_slot()) {
            // We're a binding. Fill in the variable slot.
            ex.bind_slot(bind_slot->slot, rhs);
            return true;
        } else if (auto lhs_tup = lhs.as_tuple()) {
            // We're a tuple match
            auto rhs_tup = rhs.as_tuple();
            if (!rhs_tup) {
                return false;
            }
            return _do_match(*lhs_tup, *rhs_tup);
        } else if (auto lhs_cons = lhs.as_cons()) {
            // A cons!
            auto rhs_list = rhs.as_list();
            if (!rhs_list) {
                return false;
            }
            return _do_match(*lhs_cons, *rhs_list);
        } else if (auto lhs_list = lhs.as_list()) {
            // A list. We just match each element
            auto rhs_list = rhs.as_list();
            if (!rhs_list) {
                return false;
            }
            return _do_match(*lhs_list, *rhs_list);
        } else {
            // Other than direct assignment and tuple expansion, we just do
            // an equality check
            return lhs == rhs;
        }
    }

    void execute(is::hard_match mat) {
        auto& lhs       = ex.nth(mat.lhs);
        auto& rhs       = ex.nth(mat.rhs);
        auto  did_match = _match(lhs, rhs);
        if (!did_match) {
            // TODO: Throw, not assert
            assert(false && "Match failure");
        }
    }
    void execute(is::try_match mat) {
        auto& lhs       = ex.nth(mat.lhs);
        auto& rhs       = ex.nth(mat.rhs);
        auto  did_match = _match(lhs, rhs);
        ex._test_state  = did_match;
    }
    void execute(is::mk_tuple_0) { ex.push(let::tuple{{}}); }
    void execute(is::mk_tuple_1 t) { ex.push(let::tuple{{ex.nth(t.a)}}); }
    void execute(is::mk_tuple_2 t) { ex.push(let::tuple{{ex.nth(t.a), ex.nth(t.b)}}); }
    void execute(is::mk_tuple_3 t) { ex.push(let::tuple{{ex.nth(t.a), ex.nth(t.b), ex.nth(t.c)}}); }
    void execute(is::mk_tuple_4 t) {
        ex.push(let::tuple{{ex.nth(t.a), ex.nth(t.b), ex.nth(t.c), ex.nth(t.d)}});
    }
    void execute(is::mk_tuple_5 t) {
        ex.push(let::tuple{{ex.nth(t.a), ex.nth(t.b), ex.nth(t.c), ex.nth(t.d), ex.nth(t.e)}});
    }
    void execute(is::mk_tuple_6 t) {
        ex.push(let::tuple{
            {ex.nth(t.a), ex.nth(t.b), ex.nth(t.c), ex.nth(t.d), ex.nth(t.e), ex.nth(t.f)}});
    }
    void execute(is::mk_tuple_7 t) {
        ex.push(let::tuple{{ex.nth(t.a),
                            ex.nth(t.b),
                            ex.nth(t.c),
                            ex.nth(t.d),
                            ex.nth(t.e),
                            ex.nth(t.f),
                            ex.nth(t.g)}});
    }
    void execute(const is::mk_tuple_n& t) {
        std::vector<let::value> new_tup;
        for (auto slot : t.slots) {
            new_tup.push_back(ex.nth(slot));
        }
        ex.push(let::tuple(std::move(new_tup)));
    }
    void execute(const is::mk_list& l) {
        std::vector<let::value> new_list;
        for (auto slot : l.slots) {
            new_list.push_back(ex.nth(slot));
        }
        ex.push(let::list(std::make_move_iterator(new_list.begin()),
                          std::make_move_iterator(new_list.end())));
    }
    void execute(const is::mk_closure& clos) {
        std::vector<let::value> captured;
        for (auto& slot : clos.captures) {
            captured.push_back(ex.nth(slot));
        }
        auto cl = closure(ex.current_code(),
                          ex.current_code().begin() + clos.code_begin.index,
                          std::move(captured));
        ex.push(std::move(cl));
    }
    void execute(const is::mk_cons& c) {
        auto& lhs = ex.nth(c.lhs);
        auto& rhs = ex.nth(c.rhs);
        ex.push(cons{lhs, rhs});
    }
    void execute(is::no_clause) { throw std::runtime_error{"No matching clause"}; }
    void execute(is::dot d) {
        auto& lhs = ex.nth(d.object);
        auto  rhs = ex.nth(d.attr_name).as_symbol();
        assert(rhs && "Non-symbol attribute name");
        if (auto lhs_sym = lhs.as_symbol()) {
            // Module function lookup
            auto mod = ctx.get_module(lhs_sym->string());
            if (!mod) {
                assert(false && "Bad module name");
            }
            auto maybe_fn = mod->get_function(rhs->string());
            assert(maybe_fn && "Bad function for module");
            std::visit([&](auto&& fn) { ex.push(fn); }, *maybe_fn);
        } else {
            assert(false && "Unhandled dot operator case");
        }
    }

    void execute(is::push_front push) {
        auto& elem = ex.nth(push.elem);
        auto& list = ex.nth(push.list);
        if (auto list_ptr = list.as_list()) {
            ex.push(list_ptr->push_front(elem));
        } else {
            throw std::runtime_error{"Attempt to push to non-list"};
        }
    }
};

}  // namespace let::exec::detail

void let::exec::detail::executor_impl::_exec_one(context& ctx) {
    const auto& instr = _top_frame().get_and_advance();
    instr.visit([&](const auto& i) { exec_visitor{ctx, *this}.execute(i); });
}

executor::executor()
    : _impl(std::make_unique<let::exec::detail::executor_impl>()) {}

executor::executor(const code::code& c, code::iterator iter)
    : _impl(std::make_unique<let::exec::detail::executor_impl>()) {
    _impl->push_frame(c, iter);
}
executor::executor(const code::code& c)
    : executor(c, c.begin()) {}
executor::executor(const exec::closure& clos, const let::value& arg)
    : _impl(std::make_unique<let::exec::detail::executor_impl>()) {
    _impl->push_frame(clos.code(), clos.code_begin());
    // Push the values it has captured
    for (auto& el : clos.captures()) {
        _impl->push(el);
    }
    // And the initial argument
    _impl->push(std::move(arg));
}

executor::~executor() = default;

executor::executor(executor&&) = default;
executor& executor::operator=(executor&&) = default;

std::optional<let::value> let::exec::executor::execute_n(let::exec::context& ctx, std::size_t n) {
    return _impl->execute_n(n, ctx);
}
let::value let::exec::executor::execute_all(let::exec::context& ctx) {
    while (!_impl->_call_frames.empty()) {
        _impl->_exec_one(ctx);
    }
    assert(_impl->_bottom_ret);
    return *_impl->_bottom_ret;
}
