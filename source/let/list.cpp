#include "list.hpp"

#include <let/value.hpp>

std::ostream& let::operator<<(std::ostream& o, const list& l) {
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
