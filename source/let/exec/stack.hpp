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

class stack {
    std::deque<let::value> _items;

public:
    using iterator       = std::deque<let::value>::const_iterator;
    using const_iterator = iterator;
    using size_type      = std::deque<let::value>::size_type;

    size_type         size() const { return _items.size(); }
    const let::value& nth(slot_ref_t off) const {
        // assert(off < size());
        // return _items[off];
        return _items.at(off.index);
    }
    let::value& nth_mut(slot_ref_t off) { return _items.at(off.index); }
    void        push(let::value&& el) { _items.push_back(std::move(el)); }
    void        push(const let::value& el) { _items.push_back(el); }
    void        rewind(slot_ref_t new_top) {
        assert(new_top.index <= size());
        auto new_end = _items.begin() + new_top.index;
        _items.erase(new_end, _items.end());
    }
};

}  // namespace let::exec

#endif  // LET_EXEC_STACK_HPP_INCLUDED