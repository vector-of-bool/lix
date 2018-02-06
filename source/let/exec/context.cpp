#include "context.hpp"

using namespace let;
using namespace let::exec;

namespace let::exec::detail {

namespace is = let::code::is_types;

struct instr_visitor {
    context& ctx;

    void execute(is::const_int i) { ctx.push(i.value); }
    void execute(is::const_double d) { ctx.push(d.value); }
    void execute(const is::const_symbol& sym) { ctx.push(let::symbol{sym.string}); }
    void execute(const is::const_str& str) { ctx.push(let::string{str.string}); }
    void execute(is::const_binding_slot s) { ctx.push(binding_slot{s.slot}); }

    void execute(is::ret r) { ctx.pop_frame_return(r.slot); }

    void execute(is::call c) {
        auto& callee = ctx.nth(c.fn);
        if (auto closure = callee.as_closure()) {
            auto                    arg = ctx.nth(c.arg);
            std::vector<let::value> caps_tmp;
            for (auto cap : closure->captures()) {
                caps_tmp.push_back(ctx.nth(cap));
            }
            ctx.push_frame(closure->code(), closure->code_begin());
            // Push the values it has captured
            for (auto& el : caps_tmp) {
                ctx.push(el);
            }
            ctx.push(std::move(arg));
        } else if (auto fn = callee.as_function()) {
            ctx.push(fn->call_ll(ctx, ctx.nth(c.arg)));
        } else {
            assert(false && "Call to non-function");
        }
    }

    void execute(is::jump j) { ctx.jump(j.target); }

    void execute(is::test_true t) {
        auto value = ctx.nth(t.slot).as_symbol();
        ctx.set_test_state(value && value->string() == "true");
    }

    void execute(is::false_jump j) {
        if (ctx.get_test_state() == false) {
            ctx.jump(j.target);
        }
    }

    void execute(is::rewind r) { ctx.rewind(r.slot); }

    void execute(is::add add) {
        auto lhs = ctx.nth(add.a).as_integer();
        auto rhs = ctx.nth(add.b).as_integer();
        // TODO: Check for nullptr
        ctx.push(*lhs + *rhs);
    }
    void execute(is::sub sub) {
        auto lhs = ctx.nth(sub.a).as_integer();
        auto rhs = ctx.nth(sub.b).as_integer();
        // TODO: Check for nullptr;
        ctx.push(*lhs - *rhs);
    }

    void execute(is::eq eq) {
        // Compare operands for equality/equivalence
        auto& lhs       = ctx.nth(eq.a);
        auto& rhs       = ctx.nth(eq.b);
        auto  are_equal = lhs == rhs;
        ctx.push(are_equal ? let::symbol{"true"} : let::symbol{"false"});
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
        if (list.size() < 2) {
            return false;
        }
        auto head_matched = _match(head, *list.begin());
        if (!head_matched) {
            return false;
        }
        std::vector<value> tail_els{list.begin() + 1, list.end()};
        let::list          tail_list{std::move(tail_els)};
        return _match(tail, tail_list);
    }

    bool _match(const let::value& lhs, const let::value& rhs) {
        if (auto bind_slot = lhs.as_binding_slot()) {
            // We're a binding. Fill in the variable slot.
            ctx.bind_slot(bind_slot->slot, rhs);
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
        auto& lhs       = ctx.nth(mat.lhs);
        auto& rhs       = ctx.nth(mat.rhs);
        auto  did_match = _match(lhs, rhs);
        if (!did_match) {
            // TODO: Throw, not assert
            assert(false && "Match failure");
        }
    }
    void execute(is::try_match mat) {
        auto& lhs       = ctx.nth(mat.lhs);
        auto& rhs       = ctx.nth(mat.rhs);
        auto  did_match = _match(lhs, rhs);
        ctx.set_test_state(did_match);
    }
    void execute(is::mk_tuple_0) { ctx.push(let::tuple{{}}); }
    void execute(is::mk_tuple_1 t) { ctx.push(let::tuple{{ctx.nth(t.a)}}); }
    void execute(is::mk_tuple_2 t) { ctx.push(let::tuple{{ctx.nth(t.a), ctx.nth(t.b)}}); }
    void execute(is::mk_tuple_3 t) {
        ctx.push(let::tuple{{ctx.nth(t.a), ctx.nth(t.b), ctx.nth(t.c)}});
    }
    void execute(is::mk_tuple_4 t) {
        ctx.push(let::tuple{{ctx.nth(t.a), ctx.nth(t.b), ctx.nth(t.c), ctx.nth(t.d)}});
    }
    void execute(is::mk_tuple_5 t) {
        ctx.push(
            let::tuple{{ctx.nth(t.a), ctx.nth(t.b), ctx.nth(t.c), ctx.nth(t.d), ctx.nth(t.e)}});
    }
    void execute(is::mk_tuple_6 t) {
        ctx.push(let::tuple{
            {ctx.nth(t.a), ctx.nth(t.b), ctx.nth(t.c), ctx.nth(t.d), ctx.nth(t.e), ctx.nth(t.f)}});
    }
    void execute(is::mk_tuple_7 t) {
        ctx.push(let::tuple{{ctx.nth(t.a),
                             ctx.nth(t.b),
                             ctx.nth(t.c),
                             ctx.nth(t.d),
                             ctx.nth(t.e),
                             ctx.nth(t.f),
                             ctx.nth(t.g)}});
    }
    void execute(const is::mk_tuple_n& t) {
        std::vector<let::value> new_tup;
        for (auto slot : t.slots) {
            new_tup.push_back(ctx.nth(slot));
        }
        ctx.push(let::tuple(std::move(new_tup)));
    }
    void execute(const is::mk_list& l) {
        std::vector<let::value> new_list;
        for (auto slot : l.slots) {
            new_list.push_back(ctx.nth(slot));
        }
        ctx.push(let::list(std::move(new_list)));
    }
    void execute(const is::mk_closure& clos) {
        auto cl = closure(ctx.current_code(),
                          ctx.current_code().begin() + clos.code_begin.index,
                          clos.captures);
        ctx.push(std::move(cl));
    }
    void execute(const is::mk_cons& c) {
        auto& lhs = ctx.nth(c.lhs);
        auto& rhs = ctx.nth(c.rhs);
        ctx.push(cons{lhs, rhs});
    }
    void execute(is::no_clause) { throw std::runtime_error{"No matching clause"}; }
    void execute(is::dot d) {
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

    void execute(is::push_front push) {
        auto& elem = ctx.nth(push.elem);
        auto& list = ctx.nth(push.list);
        if (auto list_ptr = list.as_list()) {
            // TODO: Sort out the ex_list vs list and forward-linking persistence
            std::vector<let::value> new_list{list_ptr->begin(), list_ptr->end()};
            new_list.insert(new_list.begin(), elem);
            ctx.push(let::list{std::move(new_list)});
        } else {
            throw std::runtime_error{"Attempt to push to non-list"};
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
        instr.visit([&](const auto& i) { instr_visitor{ctx}.execute(i); });
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
            return value;
        } else {
            assert(_bottom_ret);
            auto val    = std::move(*_bottom_ret);
            _bottom_ret = std::nullopt;
            return val;
        }
    }

    const let::value& nth(slot_ref_t n) const { return _top_frame().nth(n); }

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
        auto rv = _top_frame().nth(r);
        pop_frame_no_ret();
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

void context::push(let::value el) { _impl->push(el); }
void context::rewind(slot_ref_t slot) { _impl->rewind(slot); }
void context::jump(inst_offset_t dest) { _impl->jump(dest); }
void context::set_test_state(bool b) { _impl->_test_state = b; }
bool context::get_test_state() const noexcept { return _impl->_test_state; }
void context::bind_slot(slot_ref_t slot, let::value el) { _impl->bind_slot(slot, std::move(el)); }
const let::value& context::nth(slot_ref_t n) const { return _impl->nth(n); }
const let::value& context::top() const { return _impl->_top_frame().top(); }
const code::code& context::current_code() const { return _impl->_top_frame().code(); }

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