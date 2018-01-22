#ifndef LET_LIST_HPP_INCLUDED
#define LET_LIST_HPP_INCLUDED

#include <vector>

#include "boxed.hpp"

namespace let {

class list {
    std::vector<let::boxed> _elements;

public:
    explicit list(std::vector<let::boxed> els)
        : _elements(std::move(els)) {
    }

    auto size() const noexcept {
        return _elements.size();
    }

    void push(let::boxed b) {
        _elements.push_back(std::move(b));
    }

    void pop() {
        _elements.pop_back();
    }

    void push_front(let::boxed b) {
        _elements.insert(_elements.begin(), b);
    }

    void pop_front() {
        _elements.erase(_elements.begin());
    }
};

}  // namespace let

LET_TYPEINFO(let::list, push, pop, push_front, pop_front, size);

#endif  // LET_LIST_HPP_INCLUDED