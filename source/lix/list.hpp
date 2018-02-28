#ifndef LIX_LIST_HPP_INCLUDED
#define LIX_LIST_HPP_INCLUDED

#include <memory>
#include <ostream>
#include <vector>
#include <utility>

#include "list_fwd.hpp"
#include "value.hpp"

namespace lix::detail {

struct list_node_emplace {};

struct list_node {
    std::shared_ptr<list_node> next_node;
    lix::value                 my_value;

    template <typename... Args>
    list_node(list_node_emplace, Args&&... args)
        : my_value(std::forward<Args>(args)...) {}
};

}  // namespace lix::detail

inline lix::list::iterator& lix::list::iterator::operator++() {
    assert(_node != nullptr && "Increment list end iterator");
    _node = _node->next_node;
    return *this;
}

inline lix::list::iterator lix::list::iterator::operator++(int) {
    auto tmp = *this;
    ++*this;
    return tmp;
}

inline bool lix::list::iterator::operator==(const iterator& other) const {
    return _node == other._node;
}
inline bool lix::list::iterator::operator!=(const iterator& other) const {
    return _node != other._node;
}

inline const lix::value& lix::list::iterator::operator*() const noexcept {
    assert(_node != nullptr && "Dereference list end iterator");
    return _node->my_value;
}

inline const lix::value* lix::list::iterator::operator->() const noexcept {
    return std::addressof(**this);
}

template <typename Iterator, typename EndIter, typename, typename>
inline lix::list::list(Iterator first, EndIter last) {
    std::shared_ptr<detail::list_node>* tail = &_head_node;
    while (first != last) {
        *tail = std::make_shared<detail::list_node>(detail::list_node_emplace(), *first);
        tail  = &(*tail)->next_node;
        ++first;
        ++_size;
    }
}

inline lix::list::iterator lix::list::begin() const noexcept { return iterator(_head_node); }
inline lix::list::iterator lix::list::end() const noexcept { return iterator(nullptr); }

inline lix::list lix::list::pop_front() const noexcept {
    assert(_head_node != nullptr && "Pop front of empty list");
    return lix::list(_head_node->next_node, _size - 1);
}
inline std::pair<lix::value, lix::list> lix::list::take_front() const noexcept {
    assert(_head_node != nullptr && "Take front of empty list");
    auto front = *begin();
    return std::make_pair(std::move(front), pop_front());
}
inline lix::list lix::list::push_front(lix::value&& val) const noexcept {
    auto new_head
        = std::make_shared<detail::list_node>(detail::list_node_emplace(), std::move(val));
    new_head->next_node = _head_node;
    return lix::list(std::move(new_head), _size + 1);
}
inline lix::list lix::list::push_front(const lix::value& val) const {
    auto new_head       = std::make_shared<detail::list_node>(detail::list_node_emplace(), val);
    new_head->next_node = _head_node;
    return lix::list(std::move(new_head), _size + 1);
}

#endif  // LIX_LIST_HPP_INCLUDED