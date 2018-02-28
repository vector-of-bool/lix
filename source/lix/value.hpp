#ifndef LIX_VALUE_HPP_INCLUDED
#define LIX_VALUE_HPP_INCLUDED

#include <lix/boxed.hpp>
#include <lix/exec/closure.hpp>
#include <lix/exec/fn_no_impl.hpp>
#include <lix/list_fwd.hpp>
#include <lix/map.hpp>
#include <lix/numbers.hpp>
#include <lix/string.hpp>
#include <lix/symbol.hpp>
#include <lix/tuple.hpp>
#include <lix/util.hpp>
#include <lix/util/opt_ref.hpp>

#include "value_fwd.hpp"

#include <functional>
#include <ostream>
#include <variant>

namespace lix {

namespace exec {

namespace detail {

struct binding_slot {
    code::slot_ref_t slot;
};

struct cons {
    std::reference_wrapper<const lix::value> head;
    std::reference_wrapper<const lix::value> tail;
};

inline std::ostream& operator<<(std::ostream& o, binding_slot) {
    o << "<unbound>";
    return o;
}

inline std::ostream& operator<<(std::ostream& o, cons) {
    o << "<cons>";
    return o;
}

}  // namespace detail

}  // namespace exec

class value {
    std::variant<lix::integer,
                 lix::real,
                 lix::symbol,
                 lix::string,
                 lix::tuple,
                 lix::list,
                 lix::map,
                 lix::exec::function,
                 lix::exec::closure,
                 lix::exec::detail::binding_slot,
                 lix::exec::detail::cons,
                 lix::boxed>
        _value;

public:
#define DECL_METHODS(type, basename)                                                               \
    value(const type& t)                                                                           \
        : _value(t) {}                                                                             \
    value(type&& t)                                                                                \
        : _value(std::move(t)) {}                                                                  \
    opt_ref<const type> as_##basename() const noexcept {                                           \
        if (auto ptr = std::get_if<type>(&_value)) {                                               \
            return *ptr;                                                                           \
        } else {                                                                                   \
            return nullopt;                                                                        \
        }                                                                                          \
    }                                                                                              \
    opt_ref<const type> as(tag<type>) const noexcept { return as_##basename(); }                   \
    static_assert(true)

    DECL_METHODS(lix::integer, integer);
    DECL_METHODS(lix::real, real);
    DECL_METHODS(lix::symbol, symbol);
    DECL_METHODS(lix::string, string);
    DECL_METHODS(lix::tuple, tuple);
    DECL_METHODS(lix::list, list);
    DECL_METHODS(lix::map, map);
    DECL_METHODS(lix::exec::function, function);
    DECL_METHODS(lix::exec::closure, closure);
    DECL_METHODS(lix::exec::detail::binding_slot, binding_slot);
    DECL_METHODS(lix::exec::detail::cons, cons);
    DECL_METHODS(lix::boxed, boxed);
#undef DECL_METHODS

    template <typename Integer, typename = std::enable_if_t<std::is_integral<Integer>::value>>
    value(Integer i)
        : value(integer(i)) {}

    template <typename Boxable,
              typename = std::enable_if_t<lix::is_boxable<std::decay_t<Boxable>>::value>,
              typename = void>
    value(Boxable&& b)
        : value(boxed(std::forward<Boxable>(b))) {}

    template <typename Fun, typename... Args>
    decltype(auto) visit(Fun&& fn, Args&&... args) const {
        return std::visit(
            [&](auto&& item) -> decltype(auto) {
                return std::forward<Fun>(fn)(item, std::forward<Args>(args)...);
            },
            _value);
    }
};

inline bool operator==(const value& lhs, const value& rhs) {
    return lhs.visit([&](const auto& real_value) -> bool {
        auto rhs_same = rhs.as(tag<std::decay_t<decltype(real_value)>>());
        return rhs_same && real_value == *rhs_same;
    });
}

inline bool operator!=(const value& lhs, const value& rhs) { return !(lhs == rhs); }

inline std::ostream& operator<<(std::ostream& o, const value& rhs) {
    rhs.visit([&](const auto& val) { o << val; });
    return o;
}

std::string to_string(const value&);
std::string inspect(const value&);

}  // namespace lix

namespace std {

template <> struct hash <lix::value> {
    std::size_t operator()(const lix::value&) const;
};

} // namespace std

#include <lix/list.hpp>
#include <lix/refl_get_member.hpp>

#endif  // LIX_VALUE_HPP_INCLUDED