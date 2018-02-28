#include "exec.hpp"

#include <let/compiler/compile.hpp>

#include <let/refl_get_member.hpp>

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
    std::string    _ident;

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

    void jump(inst_offset_t target) {
        auto avail = std::distance(_first_instr, _end_instr);
        assert(target.index > 0);
        assert(target.index < static_cast<std::size_t>(avail));
        _current_instr = _first_instr + target.index;
    }

    void bind_slot(slot_ref_t slot, let::value el) {
        auto& dest = _stack.nth_mut(slot);
        assert(dest.as_binding_slot() && "Binding to non-binding slot");
        dest = std::move(el);
    }

    void               set_ident(const std::string& name) { _ident = name; }
    const std::string& ident() const { return _ident; }

    void rewind(slot_ref_t new_top) { _stack.rewind(new_top); }

    const code::code& code() const { return _code; }
};

class executor_impl {
public:
    std::deque<exec_frame>    _call_frames;
    bool                      _test_state = false;
    std::optional<let::value> _bottom_ret;

    exec_frame& _top_frame() {
        assert(!_call_frames.empty());
        return _call_frames.back();
    }

    const exec_frame& _top_frame() const {
        assert(!_call_frames.empty());
        return _call_frames.back();
    }

    const let::value& nth(slot_ref_t n) const { return _top_frame().nth(n); }

    void pop_frame_return(slot_ref_t r) {
        auto rv = _top_frame().nth(r);
        _call_frames.pop_back();
        if (_call_frames.empty()) {
            _bottom_ret.emplace(std::move(rv));
        } else {
            _top_frame().push(std::move(rv));
        }
    }

    void push_frame(code::code c, code::iterator inst) { _call_frames.emplace_back(c, inst); }
    void replace_frame(code::code c, code::iterator inst) {
        _call_frames.back() = exec_frame(c, inst);
    }

    void push(value&& el) { _top_frame().push(std::move(el)); }
    void push(const value& el) { _top_frame().push(el); }
    void rewind(slot_ref_t slot) { _top_frame().rewind(slot); }

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

    std::vector<std::string> _make_traceback() const {
        std::vector<std::string> traceback;
        for (auto& fr : ex._call_frames) {
            traceback.push_back(fr.ident());
        }
        return traceback;
    }

    template <typename... Args>
    [[noreturn]] void _raise_tuple(Args&&... args) const {
        auto tup = let::tuple::make(std::forward<Args>(args)...);
        let::raise(tup, _make_traceback());
    }

    let::value _call_ll(const let::exec::function& fn, const let::value& argument) const {
        try {
            return fn.call_ll(ctx, argument);
        } catch (const let::compile_error& e) {
            _raise_tuple("CompileError"_sym, e.line(), e.column(), e.what());
        } catch (const std::runtime_error& e) {
            _raise_tuple("RuntimeError"_sym, e.what());
        }
    }

    void execute(is::const_int i) { ex.push(i.value); }
    void execute(is::const_real d) { ex.push(d.value); }
    void execute(is::const_symbol sym) { ex.push(let::symbol{sym.sym}); }
    void execute(const is::const_str& str) { ex.push(let::string{str.string}); }
    void execute(is::const_binding_slot s) { ex.push(binding_slot{s.slot}); }

    void execute(is::ret r) { ex.pop_frame_return(r.slot); }

    void _call_closure(const exec::closure& closure, const let::value& arg_tup, bool is_tail) {
        if (is_tail) {
            ex.replace_frame(closure.code(), closure.code_begin());
        } else {
            ex.push_frame(closure.code(), closure.code_begin());
        }
        for (auto& el : closure.captures()) {
            ex.push(el);
        }
        ex.push(std::move(arg_tup));
    }

    template <typename CallInstr>
    void _dyn_call(const CallInstr& c, bool is_tail) {
        auto& callee = ex.nth(c.fn);
        auto arg = ex.nth(c.arg);
        if (auto closure = callee.as_closure()) {
            _call_closure(*closure, arg, is_tail);
        } else if (auto fn = callee.as_function()) {
            ex.push(_call_ll(*fn, arg));
        } else {
            _raise_tuple("badcall"_sym, callee);
        }
    }
    void execute(is::call c) { _dyn_call(c, false); }
    void execute(is::tail c) { _dyn_call(c, true); }

    template <typename CallInstr>
    void _mfa_call(const CallInstr& c, bool is_tail) {
        std::vector<let::value> vals;
        for (auto slot : c.args) {
            vals.push_back(ex.nth(slot));
        }
        auto tup = let::tuple(std::move(vals));
        auto mod = ctx.get_module(c.module.string());
        if (!mod) {
            _raise_tuple("undefined"_sym, c.module);
        }
        auto fun = mod->get_function(c.fn.string());
        if (!fun) {
            _raise_tuple("undefined"_sym, c.module, c.fn);
        }
        if (auto closure = std::get_if<let::exec::closure>(&*fun)) {
            _call_closure(*closure, tup, is_tail);
        } else if (auto native_fn = std::get_if<let::exec::function>(&*fun)) {
            ex.push(_call_ll(*native_fn, tup));
        } else {
            assert(false && "Unreachable");
            std::terminate();
        }
    }

    void execute(const is::call_mfa& c) { _mfa_call(c, false); }
    void execute(const is::tail_mfa& t) { _mfa_call(t, true); }

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

    template <typename BinOp>
    bool _try_arith(BinOp op, const let::value& lhs, const let::value& rhs) {
        if (auto l_int = lhs.as_integer()) {
            if (auto r_int = rhs.as_integer()) {
                ex.push(op(*l_int, *r_int));
            } else if (auto r_real = rhs.as_real()) {
                ex.push(op(*l_int, *r_real));
            } else {
                return false;
            }
        } else if (auto l_real = lhs.as_real()) {
            if (auto r_real = rhs.as_real()) {
                ex.push(op(*l_real, *r_real));
            } else if (auto r_int = rhs.as_integer()) {
                ex.push(op(*l_real, *r_int));
            } else {
                return false;
            }
        }
        return true;
    }

    void execute(is::add add) {
        auto& lhs       = ex.nth(add.a);
        auto& rhs       = ex.nth(add.b);
        auto  did_arith = _try_arith(std::plus<>{}, lhs, rhs);
        if (did_arith) {
            return;
        }
        if (auto l_str = lhs.as_string()) {
            auto r_str = rhs.as_string();
            if (!r_str) {
                _raise_tuple("einval"_sym, "+"_sym, lhs, rhs);
            }
            ex.push(*l_str + *r_str);
        } else {
            _raise_tuple("einval"_sym, "+"_sym, lhs, rhs);
        }
    }

    void execute(is::sub sub) {
        auto& lhs = ex.nth(sub.a);
        auto& rhs = ex.nth(sub.b);
        if (!_try_arith(std::minus<>{}, lhs, rhs)) {
            _raise_tuple("einval"_sym, "-"_sym, lhs, rhs);
        }
    }

    void execute(is::mul mul) {
        auto& lhs = ex.nth(mul.a);
        auto& rhs = ex.nth(mul.b);
        if (!_try_arith(std::multiplies<>{}, lhs, rhs)) {
            _raise_tuple("einval"_sym, "*"_sym, lhs, rhs);
        }
    }

    void execute(is::div div) {
        auto& lhs = ex.nth(div.a);
        auto& rhs = ex.nth(div.b);
        if (!_try_arith(std::divides<>{}, lhs, rhs)) {
            _raise_tuple("einval"_sym, "/"_sym, lhs, rhs);
        }
    }

    void execute(is::eq eq) {
        // Compare operands for equality/equivalence
        auto& lhs       = ex.nth(eq.a);
        auto& rhs       = ex.nth(eq.b);
        auto  are_equal = lhs == rhs;
        ex.push(are_equal ? let::symbol{"true"} : let::symbol{"false"});
    }

    void execute(is::neq eq) {
        // Compare operands for inequality/inequivalence
        auto& lhs       = ex.nth(eq.a);
        auto& rhs       = ex.nth(eq.b);
        auto  are_equal = lhs == rhs;
        ex.push(are_equal ? let::symbol{"false"} : let::symbol{"true"});
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
            auto  lhs_slot  = bind_slot->slot;
            auto& lhs_value = ex.nth(lhs_slot);
            if (lhs_value.as_binding_slot()) {
                // Unbound slot
                ex._top_frame().bind_slot(lhs_slot, rhs);
                return true;
            } else {
                // Check that the already-bound value is equivalent to the
                // current value
                return lhs_value == rhs;
            }
            // } else if (auto rhs_bind_slot = rhs.as_binding_slot()) {
            //     // Whoa! Right-hand can also be a binding slot in cases where we
            //     // have a bind (=) within a binding context, such as the head of a
            //     // function or case clause
            //     // In this case, we bind backward and put the left into the right
            //     auto rhs_slot = rhs_bind_slot->slot;
            //     auto& rhs_value = ex.nth(rhs_slot);
            //     if (rhs_value.as_binding_slot()) {
            //         // Right-hand is unbound
            //         ex._top_frame().bind_slot(rhs_slot, lhs);
            //         return true;
            //     } else {
            //         // Check that the two are equivalent
            //         return lhs == rhs_value;
            //     }
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
            _raise_tuple("badmatch"_sym, lhs, rhs);
        }
    }
    void execute(is::try_match mat) {
        auto& lhs       = ex.nth(mat.lhs);
        auto& rhs       = ex.nth(mat.rhs);
        auto  did_match = _match(lhs, rhs);
        ex._test_state  = did_match;
    }
    void execute(is::try_match_conj mat) {
        if (!ex._test_state) {
            // We only perform our test if we already matched
            return;
        }
        execute(is::try_match{mat.lhs, mat.rhs});
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
    void execute(const is::mk_map& m) {
        let::map map;
        for (auto slot : m.slots) {
            auto& pair = ex.nth(slot);
            auto  tup  = pair.as_tuple();
            if (!tup && tup->size() != 2) {
                _raise_tuple("badarg"_sym, "%{}", pair);
            }
            map = map.insert((*tup)[0], (*tup)[1]);
        }
        ex.push(std::move(map));
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
    void execute(is::no_clause n) { _raise_tuple("nomatch"_sym, ex.nth(n.unmatched)); }
    void execute(is::dot d) {
        auto& lhs = ex.nth(d.object);
        auto  rhs = ex.nth(d.attr_name).as_symbol();
        assert(rhs && "Non-symbol attribute name");
        if (auto lhs_sym = lhs.as_symbol()) {
            // Module function lookup
            auto mod = ctx.get_module(lhs_sym->string());
            if (!mod) {
                throw std::runtime_error{"No such module '" + lhs_sym->string() + "'"};
            }
            auto maybe_fn = mod->get_function(rhs->string());
            if (!maybe_fn) {
                throw std::runtime_error{"Module '" + lhs_sym->string() + "' has no member '"
                                         + rhs->string() + "'"};
            }
            std::visit([&](auto&& fn) { ex.push(fn); }, *maybe_fn);
        } else if (auto lhs_map = lhs.as_map()) {
            auto elem = lhs_map->find(*rhs);
            if (!elem) {
                _raise_tuple("KeyError"_sym, *rhs);
            }
            ex.push(*elem);
        } else if (auto lhs_box = lhs.as_boxed()) {
            _dot_boxed(*lhs_box, rhs->string());
        } else {
            _raise_tuple("einval"_sym, "."_sym, lhs);
        }
    }
    void execute(is::is_list i) {
        auto& arg = ex.nth(i.arg);
        if (arg.as_list()) {
            ex.push("true"_sym);
        } else {
            ex.push("false"_sym);
        }
    }
    void execute(is::to_string t) {
        auto& arg = ex.nth(t.arg);
        if (auto sym = arg.as_symbol()) {
            ex.push(sym->string());
        } else if (auto int_ = arg.as_integer()) {
            ex.push(std::to_string(*int_));
        } else if (auto str = arg.as_string()) {
            ex.push(*str);
        } else {
            _raise_tuple("einval"_sym, "to_string"_sym, arg);
        }
    }
    void execute(is::inspect t) {
        auto& arg = ex.nth(t.arg);
        ex.push(let::inspect(arg));
    }

    void execute(is::raise r) {
        auto arg = ex.nth(r.arg);
        let::raise(std::move(arg), std::move(_make_traceback()));
    }

    void execute(is::apply a) {
        auto modname     = ex.nth(a.mod);
        auto fn_name     = ex.nth(a.fn);
        auto args        = ex.nth(a.arglist);
        auto modname_sym = modname.as_symbol();
        if (!modname_sym) {
            let::raise(tuple::make("einval"_sym, "First argument to apply() must be a symbol"));
        }
        auto fn_sym = fn_name.as_symbol();
        if (!fn_sym) {
            let::raise(tuple::make("einval"_sym, "Second argument to apply() must be a symbol"));
        }
        auto arg_list = args.as_list();
        if (!arg_list) {
            let::raise(tuple::make("einval"_sym, "Third argument to apply() must be a list"));
        }
        std::vector<let::value> argtup_els{arg_list->begin(), arg_list->end()};
        auto                    arg_tup = let::tuple(std::move(argtup_els));
        auto                    mod     = ctx.get_module(modname_sym->string());
        if (!mod) {
            let::raise(
                tuple::make("einval"_sym, "No such module to apply(): " + modname_sym->string()));
        }
        auto fn = mod->get_function(fn_sym->string());
        if (!fn) {
            let::raise(tuple::make("einval"_sym,
                                   "No such function '" + fn_sym->string() + "' in module '"
                                       + modname_sym->string() + "'"));
        }
        if (auto fun = std::get_if<let::exec::function>(&*fn)) {
            ex.push(_call_ll(*fun, arg_tup));
        } else {
            auto clos = std::get_if<let::exec::closure>(&*fn);
            assert(clos);
            ex.push_frame(clos->code(), clos->code_begin());
            for (auto& el : clos->captures()) {
                ex.push(el);
            }
            ex.push(std::move(std::move(arg_tup)));
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

    void execute(const is::frame_id& id) { ex._top_frame().set_ident(id.id); }

    void _dot_boxed(const let::boxed& b, const std::string& member) {
        auto val = b.get_member(member);
        ex.push(std::move(val));
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
