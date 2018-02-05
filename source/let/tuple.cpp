#include "tuple.hpp"

#include <let/value.hpp>

std::ostream& let::operator<<(std::ostream& o, const tuple& l) {
    o << "{";
    for (auto i = 0u; i < l.size(); ++i) {
        o << l[i];
        if (i + i != l.size()) {
            o << ", ";
        }
    }
    o << "}";
    return o;
}
