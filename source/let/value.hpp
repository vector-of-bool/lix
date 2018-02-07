#ifndef LET_VALUE_HPP_INCLUDED
#define LET_VALUE_HPP_INCLUDED

#include <let/boxed.hpp>
#include <let/exec/closure.hpp>
#include <let/exec/fn_no_impl.hpp>
#include <let/list_fwd.hpp>
#include <let/numbers.hpp>
#include <let/string.hpp>
#include <let/symbol.hpp>
#include <let/tuple.hpp>
#include <let/util.hpp>
#include <let/util/opt_ref.hpp>

#include "value_fwd.hpp"

#include <ostream>
#include <variant>

namespace let {

namespace exec {

namespace detail {

struct binding_slot {
    code::slot_ref_t slot;
};

struct cons {
    std::reference_wrapper<const let::value> head;
    std::reference_wrapper<const let::value> tail;
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
    std::variant<let::integer,
                 let::real,
                 let::symbol,
                 let::string,
                 let::tuple,
                 let::list,
                 let::exec::function,
                 let::exec::closure,
                 let::exec::detail::binding_slot,
                 let::exec::detail::cons,
                 let::boxed>
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

    DECL_METHODS(let::integer, integer);
    DECL_METHODS(let::real, real);
    DECL_METHODS(let::symbol, symbol);
    DECL_METHODS(let::string, string);
    DECL_METHODS(let::tuple, tuple);
    DECL_METHODS(let::list, list);
    DECL_METHODS(let::exec::function, function);
    DECL_METHODS(let::exec::closure, closure);
    DECL_METHODS(let::exec::detail::binding_slot, binding_slot);
    DECL_METHODS(let::exec::detail::cons, cons);
    DECL_METHODS(let::boxed, boxed);
#undef DECL_METHODS

    template <typename Integer, typename = std::enable_if_t<std::is_integral<Integer>::value>>
    value(Integer i)
        : value(integer(i)) {}

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

}  // namespace let

#include <let/list.hpp>

#endif  // LET_VALUE_HPP_INCLUDED