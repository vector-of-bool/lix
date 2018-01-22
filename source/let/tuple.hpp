#ifndef LET_TUPLE_HPP_INCLUDED
#define LET_TUPLE_HPP_INCLUDED

#include <stdexcept>
#include <vector>

#include "boxed.hpp"

namespace let {

class bad_tuple_access : std::out_of_range {
public:
    using std::out_of_range::out_of_range;
};

class tuple {
private:
    std::vector<let::boxed> _values;

public:
    explicit tuple(std::vector<let::boxed> b)
        : _values(std::move(b)) {
    }

    template <typename... Ts> static tuple make(Ts&&... ts) {
        std::vector<let::boxed> values = {let::boxed(std::forward<Ts>(ts))...};
        return tuple(std::move(values));
    }

    std::size_t size() const noexcept {
        return _values.size();
    }

    const let::boxed& operator[](std::size_t idx) const {
        if (idx >= size()) {
            throw bad_tuple_access{"Invalid index on tuple element"};
        }
        return _values[idx];
    }
};

}  // namespace let

LET_BASIC_TYPEINFO(let::tuple);

#endif  // LET_TUPLE_HPP_INCLUDED