#ifndef LET_LIST_HPP_INCLUDED
#define LET_LIST_HPP_INCLUDED

#include <memory>
#include <ostream>
#include <vector>

#include "list_fwd.hpp"
#include "value.hpp"

namespace let::detail {

struct list_node_emplace {};

struct list_node {
    std::shared_ptr<list_node> next_node;
    let::value                 my_value;

    template <typename... Args>
    list_node(list_node_emplace, Args&&... args)
        : my_value(std::forward<Args>(args)...) {}
};

}  // namespace let::detail

inline let::list::iterator& let::list::iterator::operator++() {
    assert(_node != nullptr && "Increment list end iterator");
    _node = _node->next_node;
    return *this;
}

inline let::list::iterator let::list::iterator::operator++(int) {
    auto tmp = *this;
    ++*this;
    return tmp;
}

inline bool let::list::iterator::operator==(const iterator& other) const {
    return _node == other._node;
}
inline bool let::list::iterator::operator!=(const iterator& other) const {
    return _node != other._node;
}

inline const let::value& let::list::iterator::operator*() const noexcept {
    assert(_node != nullptr && "Dereference list end iterator");
    return _node->my_value;
}

inline const let::value* let::list::iterator::operator->() const noexcept {
    return std::addressof(**this);
}

template <typename Iterator, typename EndIter, typename, typename>
inline let::list::list(Iterator first, EndIter last) {
    std::shared_ptr<detail::list_node>* tail = &_head_node;
    while (first != last) {
        *tail = std::make_shared<detail::list_node>(detail::list_node_emplace(), *first);
        tail  = &(*tail)->next_node;
        ++first;
        ++_size;
    }
}

inline let::list::iterator let::list::begin() const noexcept { return iterator(_head_node); }
inline let::list::iterator let::list::end() const noexcept { return iterator(nullptr); }

inline let::list let::list::pop_front() const noexcept {
    assert(_head_node != nullptr && "Pop front of empty list");
    return let::list(_head_node->next_node, _size - 1);
}
inline let::list let::list::push_front(let::value&& val) const noexcept {
    auto new_head
        = std::make_shared<detail::list_node>(detail::list_node_emplace(), std::move(val));
    new_head->next_node = _head_node;
    return let::list(std::move(new_head), _size + 1);
}
inline let::list let::list::push_front(const let::value& val) const {
    auto new_head       = std::make_shared<detail::list_node>(detail::list_node_emplace(), val);
    new_head->next_node = _head_node;
    return let::list(std::move(new_head), _size + 1);
}

#endif  // LET_LIST_HPP_INCLUDED