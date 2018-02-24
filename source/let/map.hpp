#ifndef LET_MAP_HPP_INCLUDED
#define LET_MAP_HPP_INCLUDED

#include <memory>
#include <utility>

#include <let/util/opt_ref.hpp>
#include <let/value_fwd.hpp>

namespace let {

namespace detail {

struct map_impl;

}  // namespace detail

class map {
public:
    class iterator;
    using const_iterator = iterator;

private:
    std::shared_ptr<detail::map_impl> _impl;

    map(detail::map_impl&& ptr);

public:
    map();
    ~map() = default;
    iterator begin() const;
    iterator cbegin() const;
    iterator end() const;
    iterator cend() const;

    [[nodiscard]] map insert(const let::value&, const let::value&) const;
    [[nodiscard]] map insert_or_update(const let::value&, const let::value&) const;
    [[nodiscard]] std::pair<let::value, map> pop(const let::value&) const;
    [[nodiscard]] opt_ref<const let::value>  find(const let::value&) const;
};

}  // namespace let

#endif  // LET_MAP_HPP_INCLUDED