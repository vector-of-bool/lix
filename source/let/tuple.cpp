#include "tuple.hpp"

#include <algorithm>

#include <let/value.hpp>

std::ostream& let::operator<<(std::ostream& o, const tuple& l) {
    o << "{";
    for (auto i = 0u; i < l.size(); ++i) {
        o << inspect(l[i]);
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

std::size_t std::hash<let::tuple>::operator()(const let::tuple& tup) const {
    auto        iter     = tup.val_begin();
    const auto  end      = tup.val_end();
    std::size_t hash_acc = 0;
    while (iter != end) {
        auto elem_hash = std::hash<let::value>()(*iter++);
        hash_acc ^= elem_hash + 0x9e3779b97f4a7c16 + (hash_acc << 6) + (hash_acc >> 2);
    }
    return hash_acc;
}