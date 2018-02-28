#ifndef LIX_EXEC_STACK_HPP_INCLUDED
#define LIX_EXEC_STACK_HPP_INCLUDED

#include <lix/code/types.hpp>

#include <lix/value.hpp>

#include <lix/exec/closure.hpp>
#include <lix/exec/fn.hpp>

#include <cassert>
#include <deque>
#include <variant>
#include <vector>

namespace lix::exec {

using lix::code::inst_offset_t;
using lix::code::slot_ref_t;

class stack {
    std::deque<lix::value> _items;

public:
    using iterator       = std::deque<lix::value>::const_iterator;
    using const_iterator = iterator;
    using size_type      = std::deque<lix::value>::size_type;

    size_type         size() const { return _items.size(); }
    const lix::value& nth(slot_ref_t off) const {
        // assert(off < size());
        // return _items[off];
        return _items.at(off.index);
    }
    lix::value& nth_mut(slot_ref_t off) { return _items.at(off.index); }
    void        push(lix::value&& el) { _items.push_back(std::move(el)); }
    void        push(const lix::value& el) { _items.push_back(el); }
    void        rewind(slot_ref_t new_top) {
        assert(new_top.index <= size());
        auto new_end = _items.begin() + new_top.index;
        _items.erase(new_end, _items.end());
    }
};

}  // namespace lix::exec

#endif  // LIX_EXEC_STACK_HPP_INCLUDED