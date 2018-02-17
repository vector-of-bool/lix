#include "tuple.hpp"

#include <algorithm>

#include <let/value.hpp>

std::ostream& let::operator<<(std::ostream& o, const tuple& l) {
    o << "{";
    for (auto i = 0u; i < l.size(); ++i) {
        o << l[i];
        if (i + 1 != l.size()) {
            o << ", ";
        }
    }
    o << "}";
    return o;
}

bool let::operator==(const let::tuple& lhs, const let::tuple& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    return std::equal(lhs.val_begin(), lhs.val_end(), rhs.val_begin());
}