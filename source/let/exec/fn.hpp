#ifndef LET_EXEC_FN_HPP_INCLUDED
#define LET_EXEC_FN_HPP_INCLUDED

#include <memory>
#include <optional>
#include <ostream>
#include <type_traits>

#include <let/util.hpp>
#include <let/value.hpp>

#include "fn_no_impl.hpp"

namespace let::exec {

class context;

namespace detail {

class erased_fn_base {
public:
    virtual ~erased_fn_base() = default;

    virtual let::value call(context&, const let::value&) const = 0;
};

template <typename Func>
class erased_fn_impl : public erased_fn_base {
    Func _fn;

    template <typename Void,
              typename = std::enable_if_t<std::is_same<Void, void>::value>,
              typename = void>
    let::value _do_call(context&, const let::value&, tag<Void>) const {
        static_assert(
            !std::is_same<Void, void>::value,
            "Invalid function. Must return an object convertible to let::value, not 'void'");
        return let::value(0);
    }

    template <typename Ret, typename = std::enable_if<!std::is_same<Ret, void>::value>>
    let::value _do_call(context& ctx, const let::value& arg, tag<Ret>) const {
        static_assert(
            std::is_convertible<Ret, let::value>::value,
            "Invalid function return value. Return type must be convertible to 'let::value'");
        Ret&& r = _fn(ctx, arg);
        return let::value(std::forward<Ret>(r));
    }

public:
    template <typename T>
    erased_fn_impl(T&& fn)
        : _fn(std::forward<T>(fn)) {}

    let::value call(context& ctx, const let::value& val) const override {
        using ret_type = decltype(_fn(ctx, val));
        return _do_call(ctx, val, let::tag<ret_type>{});
    }
};

}  // namespace detail

template <typename Function, typename Void, typename Any>
function::function(Function&& fn)
    : _func(std::make_shared<detail::erased_fn_impl<std::decay_t<Function>>>(
          std::forward<Function>(fn))) {}

inline std::ostream& operator<<(std::ostream& o, const function&) {
    o << "<let::exec::function>";
    return o;
}

}  // namespace let::exec

#endif  // LET_EXEC_FN_HPP_INCLUDED