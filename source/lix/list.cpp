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
