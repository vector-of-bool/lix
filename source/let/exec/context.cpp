#include "context.hpp"

using namespace let;
using namespace let::exec;

namespace let::exec::detail {

namespace is = let::code::is_types;

struct instr_visitor {
    context& ctx;

    void operator()(is::const_int i) { ctx.push(i.value); }
    void operator()(is::const_double d) { ctx.push(d.value); }
    void operator()(const is::const_symbol& sym) { ctx.push(let::symbol{sym.string}); }
    void operator()(const is::const_str& str) { ctx.push(let::string{str.string}); }
    void operator()(is::const_binding_slot s) { ctx.push(binding_slot{s.slot}); }

    void operator()(is::ret r) { ctx.pop_frame_return(r.slot); }

    void operator()(is::call c) {
        auto& callee = ctx.nth(c.fn);
        if (auto closure = callee.as_closure()) {
            auto arg = ctx.nth(c.arg);
            ctx.push_frame(closure->code(), closure->code_begin());
            ctx.push(std::move(arg));
        } else if (auto fn = callee.as_function()) {
            if (auto arg = ctx.nth(c.arg).as_value()) {
                ctx.push(fn->call_ll(ctx, *arg));
            } else if (auto ex_tup = ctx.nth(c.arg).as_ex_tuple()) {
                auto tup = convert_ex(*ex_tup);
                ctx.push(fn->call_ll(ctx, tup));
            } else if (auto ex_list = ctx.nth(c.arg).as_ex_list()) {
                // TODO
                std::terminate();
            } else {
                assert(false && "Invalid function argument");
            }
        } else {
            assert(false && "Call to non-function");
        }
    }

    void operator()(is::jump j) { ctx.jump(j.target); }

    void operator()(is::test_true t) {
        auto value = ctx.nth(t.slot).as_symbol();
        ctx.set_test_state(value && value->string() == "true");
    }

    void operator()(is::false_jump j) {
        if (ctx.get_test_state() == false) {
            ctx.jump(j.target);
        }
    }

    void operator()(is::rewind r) { ctx.rewind(r.slot); }

    void operator()(is::add add) {
        auto lhs = ctx.nth(add.a).as_integer();
        auto rhs = ctx.nth(add.b).as_integer();
        // TODO: Check for nullptr
        ctx.push(*lhs + *rhs);
    }
    void operator()(is::sub sub) {
        auto lhs = ctx.nth(sub.a).as_integer();
        auto rhs = ctx.nth(sub.b).as_integer();
        // TODO: Check for nullptr;
        ctx.push(*lhs - *rhs);
    }

    void operator()(is::eq eq) {
        // Compare operands for equality/equivalence
        auto& lhs       = ctx.nth(eq.a);
        auto& rhs       = ctx.nth(eq.b);
        auto are_equal = _compare_eq(lhs, rhs);
        ctx.push(are_equal ? let::symbol{"true"} : let::symbol{"false"});
    }

    bool _compare_eq(const stack_element& lhs, const stack_element& rhs) {
        if (auto lhs_val = lhs.as_value()) {
            auto rhs_val = rhs.as_value();
            return rhs_val && *lhs_val == *rhs_val;
        } else {
            assert(false && "TODO: Compare unimplemented");
            std::terminate();
        }
    }

    template <typename LHS, typename RHS>
    void _match_failure(const LHS&, const RHS&) {
        assert(false && "TODO: Match failure");
        std::terminate();
    }

    bool _do_match(const ex_tuple& lhs, const ex_tuple& rhs) {
        if (lhs.elements.size() != rhs.elements.size()) {
            return false;
        }
        auto       lhs_iter = lhs.elements.begin();
        const auto lhs_end  = lhs.elements.end();
        auto       rhs_iter = rhs.elements.begin();
        while (lhs_iter != lhs_end) {
            auto did_match = _do_match(*lhs_iter, *rhs_iter);
            if (!did_match) {
                return false;
            }
            ++lhs_iter;
            ++rhs_iter;
        }
        return true;
    }

    bool _do_match(const stack_element& lhs, const stack_element& rhs) {
        if (auto bind_slot = lhs.as_binding_slot()) {
            // We're a binding. Fill in the variable slot.
            ctx.bind_slot(bind_slot->slot, rhs);
            return true;
        } else if (auto lhs_tup = lhs.as_ex_tuple()) {
            // We're a tuple match
            auto rhs_tup = rhs.as_ex_tuple();
            if (!rhs_tup) {
                return false;
            }
            return _do_match(*lhs_tup, *rhs_tup);
        } else {
            // Other than direct assignment and tuple expansion, we just do
            // an equality check
            return _compare_eq(lhs, rhs);
        }
    }

    void operator()(is::hard_match mat) {
        auto& lhs       = ctx.nth(mat.lhs);
        auto& rhs       = ctx.nth(mat.rhs);
        auto  did_match = _do_match(lhs, rhs);
        if (!did_match) {
            // TODO: Throw, not assert
            assert(false && "Match failure");
        }
    }
    void operator()(is::try_match mat) {
        auto& lhs       = ctx.nth(mat.lhs);
        auto& rhs       = ctx.nth(mat.rhs);
        auto  did_match = _do_match(lhs, rhs);
        ctx.set_test_state(did_match);
    }
    void operator()(is::mk_tuple_0) { ctx.push(ex_tuple{{}}); }
    void operator()(is::mk_tuple_1 t) { ctx.push(ex_tuple{{ctx.nth(t.a)}}); }
    void operator()(is::mk_tuple_2 t) { ctx.push(ex_tuple{{ctx.nth(t.a), ctx.nth(t.b)}}); }
    void operator()(is::mk_tuple_3 t) {
        ctx.push(ex_tuple{{ctx.nth(t.a), ctx.nth(t.b), ctx.nth(t.c)}});
    }
    void operator()(is::mk_tuple_4 t) {
        ctx.push(ex_tuple{{ctx.nth(t.a), ctx.nth(t.b), ctx.nth(t.c), ctx.nth(t.d)}});
    }
    void operator()(is::mk_tuple_5 t) {
        ctx.push(ex_tuple{{ctx.nth(t.a), ctx.nth(t.b), ctx.nth(t.c), ctx.nth(t.d), ctx.nth(t.e)}});
    }
    void operator()(is::mk_tuple_6 t) {
        ctx.push(ex_tuple{
            {ctx.nth(t.a), ctx.nth(t.b), ctx.nth(t.c), ctx.nth(t.d), ctx.nth(t.e), ctx.nth(t.f)}});
    }
    void operator()(is::mk_tuple_7 t) {
        ctx.push(ex_tuple{{ctx.nth(t.a),
                           ctx.nth(t.b),
                           ctx.nth(t.c),
                           ctx.nth(t.d),
                           ctx.nth(t.e),
                           ctx.nth(t.f),
                           ctx.nth(t.g)}});
    }
    void operator()(const is::mk_tuple_n& t) {
        ex_tuple new_tup;
        for (auto slot : t.slots) {
            new_tup.elements.push_back(ctx.nth(slot));
        }
        ctx.push(std::move(new_tup));
    }
    void operator()(const is::mk_list& l) {
        ex_list new_list;
        for (auto slot : l.slots) {
            new_list.elements.push_back(ctx.nth(slot));
        }
        ctx.push(std::move(new_list));
    }
    void operator()(const is::mk_closure& clos) {
        auto cl = closure(ctx.current_code(), ctx.current_code().begin() + clos.code_begin.index);
        ctx.push(std::move(cl));
    }
    void operator()(is::no_clause) { throw std::runtime_error{"No matching clause"}; }
    void operator()(is::dot d) {
        auto& lhs = ctx.nth(d.object);
        auto  rhs = ctx.nth(d.attr_name).as_symbol();
        assert(rhs && "Non-symbol attribute name");
        if (auto lhs_sym = lhs.as_symbol()) {
            // Module function lookup
            auto mod = ctx.get_module(lhs_sym->string());
            if (!mod) {
                assert(false && "Bad module name");
            }
            auto maybe_fn = mod->get_function(rhs->string());
            assert(maybe_fn && "Bad function for module");
            std::visit([&](auto&& fn) { ctx.push(fn); }, *maybe_fn);
        } else {
            assert(false && "Unhandled dot operator case");
        }
    }
};

class context_impl {
public:
    std::stack<frame>                              _call_frames;
    bool                                           _test_state = false;
    std::optional<let::value>                      _bottom_ret;
    std::map<std::string, module>                  _modules;
    std::vector<std::map<std::string, let::value>> _environments;

    frame& _top_frame() {
        assert(!_call_frames.empty());
        return _call_frames.top();
    }

    const frame& _top_frame() const {
        assert(!_call_frames.empty());
        return _call_frames.top();
    }

    friend struct inst_evaluator;

    void _exec_one(context& ctx) {
        const auto& instr = _top_frame().get_and_advance();
        instr.visit(instr_visitor{ctx});
    }

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

    let::value execute_frame(code::code c, code::iterator iter, context& ctx) {
        auto end_frame_size = _call_frames.size();
        push_frame(c, iter);
        _top_frame();
        while (_call_frames.size() > end_frame_size) {
            _exec_one(ctx);
        }
        // The frame we pushed has returned. Steal its return value
        if (_call_frames.size() != 0) {
            auto value = _top_frame().top();
            _top_frame().pop();
            return value.convert_to_value();
        } else {
            assert(_bottom_ret);
            auto val    = std::move(*_bottom_ret);
            _bottom_ret = std::nullopt;
            return val;
        }
    }

    const stack_element& nth(slot_ref_t n) const { return _top_frame().nth(n); }

    void register_module(const std::string& name, module mod) {
        const auto did_insert = _modules.emplace(name, std::move(mod)).second;
        if (!did_insert) {
            throw std::runtime_error{"Double-registered module"};
        }
    }

    void pop_frame_no_ret() {
        // _data_stack.rewind(_top_frame().frame_stack_offset());
        _call_frames.pop();
    }

    void pop_frame_return(slot_ref_t r) {
        auto rv = _top_frame().nth(r).convert_to_value();
        pop_frame_no_ret();
        if (_call_frames.empty()) {
            _bottom_ret.emplace(std::move(rv));
        } else {
            _top_frame().push(std::move(rv));
        }
    }

    void push_frame(code::code c, code::iterator inst) { _call_frames.emplace(c, inst); }

    void push(stack_element el) { _top_frame().push(std::move(el)); }
    void rewind(slot_ref_t slot) { _top_frame().rewind(slot); }

    void bind_slot(slot_ref_t slot, stack_element el) {
        _top_frame().bind_slot(slot, std::move(el));
    }

    void jump(inst_offset_t target) { _top_frame().jump(target); }
};

}  // namespace let::exec::detail

context::context()
    : _impl(std::make_unique<let::exec::detail::context_impl>()) {}
context::context(code::code c)
    : _impl(std::make_unique<let::exec::detail::context_impl>()) {
    push_frame(c);
}

context::~context() = default;

context::context(context&&) = default;
context& context::operator=(context&&) = default;

std::optional<let::value> context::execute_n(std::size_t n) { return _impl->execute_n(n, *this); }

void context::register_module(const std::string& name, let::exec::module mod) {
    _impl->register_module(name, mod);
}

void context::push_frame(code::code c, code::iterator current) { _impl->push_frame(c, current); }
void context::pop_frame_return(slot_ref_t r) { _impl->pop_frame_return(r); }

let::value context::execute_frame(code::code c) { return execute_frame(c, c.begin()); }
let::value context::execute_frame(code::code c, code::iterator iter) {
    return _impl->execute_frame(c, iter, *this);
}

void context::push(stack_element el) { _impl->push(el); }
void context::rewind(slot_ref_t slot) { _impl->rewind(slot); }
void context::jump(inst_offset_t dest) { _impl->jump(dest); }
void context::set_test_state(bool b) { _impl->_test_state = b; }
bool context::get_test_state() const noexcept { return _impl->_test_state; }
void context::bind_slot(slot_ref_t slot, stack_element el) {
    _impl->bind_slot(slot, std::move(el));
}
const stack_element& context::nth(slot_ref_t n) const { return _impl->nth(n); }
const stack_element& context::top() const { return _impl->_top_frame().top(); }
const code::code&    context::current_code() const { return _impl->_top_frame().code(); }

std::optional<let::exec::module> context::get_module(const std::string& name) const {
    auto mod_iter = _impl->_modules.find(name);
    if (mod_iter == _impl->_modules.end()) {
        return std::nullopt;
    } else {
        return mod_iter->second;
    }
}

void context::set_environment_value(const std::string& name, let::value val) {
    if (_impl->_environments.empty()) {
        throw std::runtime_error{"No environment"};
    }
    _impl->_environments.back().emplace(name, std::move(val));
}
std::optional<let::value> context::get_environment_value(const std::string& name) const {
    auto iter = _impl->_environments.rbegin();
    auto end  = _impl->_environments.rend();
    while (iter != end) {
        auto found = iter->find(name);
        if (found != iter->end()) {
            return found->second;
        }
        ++iter;
    }
    return std::nullopt;
}
void context::_push_environment() { _impl->_environments.emplace_back(); }
void context::_pop_environment() { _impl->_environments.pop_back(); }