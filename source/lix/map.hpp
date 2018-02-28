#ifndef LIX_MAP_HPP_INCLUDED
#define LIX_MAP_HPP_INCLUDED

#include <memory>
#include <ostream>
#include <utility>

#include <lix/util/opt_ref.hpp>
#include <lix/value_fwd.hpp>

namespace lix {

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

    [[nodiscard]] map insert(const lix::value&, const lix::value&) const;
    [[nodiscard]] map insert_or_update(const lix::value&, const lix::value&) const;
    [[nodiscard]] std::optional<std::pair<lix::value, map>> pop(const lix::value&) const;
    [[nodiscard]] opt_ref<const lix::value>                 find(const lix::value&) const;
};

std::ostream& operator<<(std::ostream& o, const map& l);

}  // namespace lix

#endif  // LIX_MAP_HPP_INCLUDED