#ifndef LIX_TUPLE_HPP_INCLUDED
#define LIX_TUPLE_HPP_INCLUDED

#include "value_fwd.hpp"

#include <functional>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <vector>

namespace lix {

class bad_tuple_access : std::out_of_range {
public:
    using std::out_of_range::out_of_range;
};

class tuple {
private:
    std::shared_ptr<const std::vector<lix::value>> _values;

public:
    inline explicit tuple(const std::vector<lix::value>& b);
    inline explicit tuple(std::vector<lix::value>&& b);
    inline tuple(const tuple&);
    inline tuple(tuple&&);
    inline tuple& operator=(const tuple&);
    inline tuple& operator=(tuple&&);
    inline ~tuple();

    template <typename... Ts>
    inline static tuple make(Ts&&... ts);

    inline std::size_t       size() const noexcept;
    inline const lix::value& operator[](std::size_t idx) const;

    auto val_begin() const noexcept { return _values->begin(); }
    auto val_end() const noexcept { return _values->end(); }
};

std::ostream& operator<<(std::ostream& o, const tuple& l);

bool operator==(const lix::tuple& lhs, const lix::tuple& rhs);

}  // namespace lix

#include <lix/value.hpp>

lix::tuple::tuple(const std::vector<lix::value>& values)
    : _values(std::make_shared<std::vector<lix::value>>(values)) {}
lix::tuple::tuple(std::vector<lix::value>&& values)
    : _values(std::make_shared<std::vector<lix::value>>(std::move(values))) {}

lix::tuple::~tuple() = default;

lix::tuple::tuple(const lix::tuple&) = default;
lix::tuple::tuple(lix::tuple&&)      = default;
lix::tuple& lix::tuple::operator=(const lix::tuple&) = default;
lix::tuple& lix::tuple::operator=(lix::tuple&&) = default;

template <typename... Ts>
lix::tuple lix::tuple::make(Ts&&... ts) {
    std::vector<lix::value> values = {lix::value(std::forward<Ts>(ts))...};
    return tuple(std::move(values));
}

const lix::value& lix::tuple::operator[](const std::size_t idx) const {
    if (idx >= size()) {
        throw lix::bad_tuple_access{"Invalid index on tuple element"};
    }
    return (*_values)[idx];
}

std::size_t lix::tuple::size() const noexcept { return _values->size(); }

namespace std {

template <>
struct hash<lix::tuple> {
    std::size_t operator()(const lix::tuple&) const;
};

} // namespace std

#endif  // LIX_TUPLE_HPP_INCLUDED