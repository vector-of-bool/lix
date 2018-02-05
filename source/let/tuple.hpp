#ifndef LET_TUPLE_HPP_INCLUDED
#define LET_TUPLE_HPP_INCLUDED

#include <stdexcept>
#include <vector>

#include "value_fwd.hpp"

namespace let {

class bad_tuple_access : std::out_of_range {
public:
    using std::out_of_range::out_of_range;
};

class tuple {
private:
    std::vector<let::value> _values;

public:
    inline explicit tuple(std::vector<let::value> b);
    inline tuple(const tuple&);
    inline tuple(tuple&&);
    inline tuple& operator=(const tuple&);
    inline tuple& operator=(tuple&&);
    inline ~tuple();

    template <typename... Ts>
    inline static tuple make(Ts&&... ts);

    inline std::size_t size() const noexcept;
    inline const let::value& operator[](std::size_t idx) const;
};

}  // namespace let

#include <let/value.hpp>

let::tuple::tuple(std::vector<let::value> values)
    : _values(std::move(values)) {}

let::tuple::~tuple() = default;

let::tuple::tuple(const let::tuple&) = default;
let::tuple::tuple(let::tuple&&)      = default;
let::tuple& let::tuple::operator=(const let::tuple&) = default;
let::tuple& let::tuple::operator=(let::tuple&&) = default;

template <typename... Ts>
let::tuple let::tuple::make(Ts&&... ts) {
    std::vector<let::value> values = {let::value(std::forward<Ts>(ts))...};
    return tuple(std::move(values));
}

const let::value& let::tuple::operator[](const std::size_t idx) const {
    if (idx >= size()) {
        throw let::bad_tuple_access{"Invalid index on tuple element"};
    }
    return _values[idx];
}

std::size_t let::tuple::size() const noexcept { return _values.size(); }

#endif  // LET_TUPLE_HPP_INCLUDED