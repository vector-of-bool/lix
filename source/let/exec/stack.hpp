#ifndef LET_EXEC_STACK_HPP_INCLUDED
#define LET_EXEC_STACK_HPP_INCLUDED

#include <let/code/types.hpp>

#include <let/value.hpp>

#include <let/exec/closure.hpp>
#include <let/exec/fn.hpp>

#include <cassert>
#include <deque>
#include <variant>
#include <vector>

namespace let::exec {

using let::code::inst_offset_t;
using let::code::slot_ref_t;

class stack_element;

struct binding_slot {
    slot_ref_t slot;
};
struct ex_tuple {
    std::vector<stack_element> elements;
};
struct ex_list {
    std::vector<stack_element> elements;
};

let::tuple convert_ex(const ex_tuple&);
let::list  convert_ex(const ex_list&);

using stack_element_var = std::variant<let::value, closure, ex_tuple, ex_list, binding_slot>;

class stack_element {
    stack_element_var _value;

public:
    template <typename T,
              typename = std::enable_if_t<std::is_convertible<T&&, stack_element_var>::value>>
    stack_element(T&& t)
        : _value(std::forward<T>(t)) {}

#define DEF_OBS(name)                                                                              \
    opt_ref<const name> as_##name() const& {                                                       \
        if (auto ptr = std::get_if<name>(&_value)) {                                               \
            return *ptr;                                                                           \
        } else {                                                                                   \
            return nullopt;                                                                        \
        }                                                                                          \
    }                                                                                              \
    opt_ref<const name> as_##name()&&       = delete;                                              \
    opt_ref<const name> as_##name() const&& = delete;                                              \
    static_assert(true)

    DEF_OBS(value);
    DEF_OBS(closure);
    DEF_OBS(binding_slot);
    DEF_OBS(ex_tuple);
    DEF_OBS(ex_list);
#undef DEF_OBS

#define DEF_INDIR_OBS(name)                                                                        \
    decltype(std::declval<const let::value&>().as_##name()) as_##name() const& {                   \
        auto valptr = as_value();                                                                  \
        if (valptr) {                                                                              \
            return valptr->as_##name();                                                            \
        } else {                                                                                   \
            return nullopt;                                                                        \
        }                                                                                          \
    }                                                                                              \
    void as_##name()&&       = delete;                                                             \
    void as_##name() const&& = delete;                                                             \
    static_assert(true)
    DEF_INDIR_OBS(integer);
    DEF_INDIR_OBS(real);
    DEF_INDIR_OBS(symbol);
    DEF_INDIR_OBS(string);
    DEF_INDIR_OBS(tuple);
    DEF_INDIR_OBS(list);
    DEF_INDIR_OBS(function);
#undef DEF_INDIR_OBS

    template <typename Fun>
    decltype(auto) visit(Fun&& fn) const {
        return std::visit(std::forward<Fun>(fn), _value);
    }

    let::value convert_to_value() const;
};

class stack {
    std::deque<stack_element> _items;

public:
    using iterator       = std::deque<stack_element>::const_iterator;
    using const_iterator = iterator;
    using size_type      = std::deque<stack_element>::size_type;

    size_type            size() const { return _items.size(); }
    const stack_element& nth(slot_ref_t off) const {
        // assert(off < size());
        // return _items[off];
        return _items.at(off.index);
    }
    stack_element& nth_mut(slot_ref_t off) { return _items.at(off.index); }
    void           push(stack_element el) { _items.push_back(std::move(el)); }
    void           rewind(slot_ref_t new_top) {
        assert(new_top.index <= size());
        auto new_end = _items.begin() + new_top.index;
        _items.erase(new_end, _items.end());
    }
};

}  // namespace let::exec

#endif  // LET_EXEC_STACK_HPP_INCLUDED