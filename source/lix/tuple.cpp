#include "tuple.hpp"

#include <algorithm>

#include <lix/value.hpp>

std::ostream& lix::operator<<(std::ostream& o, const tuple& l) {
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

bool lix::operator==(const lix::tuple& lhs, const lix::tuple& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    return std::equal(lhs.val_begin(), lhs.val_end(), rhs.val_begin());
}

std::size_t std::hash<lix::tuple>::operator()(const lix::tuple& tup) const {
    auto        iter     = tup.val_begin();
    const auto  end      = tup.val_end();
    std::size_t hash_acc = 0;
    while (iter != end) {
        auto elem_hash = std::hash<lix::value>()(*iter++);
        hash_acc ^= elem_hash + 0x9e3779b97f4a7c16 + (hash_acc << 6) + (hash_acc >> 2);
    }
    return hash_acc;
}