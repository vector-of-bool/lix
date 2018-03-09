#include "list.hpp"

#include <lix/value.hpp>

std::ostream& lix::operator<<(std::ostream& o, const list& l) {
    o << "[";
    auto iter = l.begin();
    while (iter != l.end()) {
        o << *iter;
        ++iter;
        if (iter != l.end()) {
            o << ", ";
        }
    }
    o << "]";
    return o;
}

lix::list lix::list::concat(const list& a, const list& b) {
    list       ret;
    auto*      tail  = &ret._head_node;
    auto       first = a.begin();
    const auto last  = b.end();
    // Copy each node into the list.
    while (first != last) {
        *tail = std::make_shared<detail::list_node>(detail::list_node_emplace(), *first);
        // Grab a ref to the tail
        tail = &(*tail)->next_node;
        ++first;
        ++ret._size;
    }
    // Now just set the tail of the return to the head of the right-hand list
    *tail = b._head_node;
    ret._size += b.size();
    return ret;
}