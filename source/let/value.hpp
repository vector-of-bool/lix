#ifndef LET_VALUE_HPP_INCLUDED
#define LET_VALUE_HPP_INCLUDED

#include <let/boxed.hpp>
#include <let/exec/closure.hpp>
#include <let/exec/fn_no_impl.hpp>
#include <let/list.hpp>
#include <let/numbers.hpp>
#include <let/string.hpp>
#include <let/symbol.hpp>
#include <let/tuple.hpp>
#include <let/util.hpp>
#include <let/util/opt_ref.hpp>

#include "value_fwd.hpp"

#include <variant>

namespace let {

class value {
    std::variant<let::integer,
                 let::real,
                 let::symbol,
                 let::string,
                 let::tuple,
                 let::list,
                 let::exec::function,
                 let::exec::closure,
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
    opt_ref<const type> as(tag<type>) const noexcept {                                             \
        return as_##basename();                                                                    \
    }                                                                                              \
    static_assert(true)

    DECL_METHODS(let::integer, integer);
    DECL_METHODS(let::real, real);
    DECL_METHODS(let::symbol, symbol);
    DECL_METHODS(let::string, string);
    DECL_METHODS(let::tuple, tuple);
    DECL_METHODS(let::list, list);
    DECL_METHODS(let::exec::function, function);
    DECL_METHODS(let::exec::closure, closure);
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

}  // namespace let

#endif  // LET_VALUE_HPP_INCLUDED