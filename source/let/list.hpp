#ifndef LET_LIST_HPP_INCLUDED
#define LET_LIST_HPP_INCLUDED

#include <vector>

#include "value_fwd.hpp"

namespace let {

class list {
    std::vector<let::value> _elements;

public:
    explicit list(std::vector<let::value> els);
    std::size_t size() const noexcept;
    void push(let::value b);
    void pop();
    void push_front(let::value b);
    void pop_front();

    auto begin() const { return _elements.begin(); }
    auto end() const { return _elements.end(); }
};

}  // namespace let

#endif  // LET_LIST_HPP_INCLUDED