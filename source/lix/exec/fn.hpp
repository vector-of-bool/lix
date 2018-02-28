#ifndef LIX_EXEC_FN_HPP_INCLUDED
#define LIX_EXEC_FN_HPP_INCLUDED

#include <memory>
#include <optional>
#include <ostream>
#include <type_traits>

#include <lix/util.hpp>
#include <lix/value.hpp>

#include "fn_no_impl.hpp"

namespace lix::exec {

class context;

namespace detail {

class erased_fn_base {
public:
    virtual ~erased_fn_base() = default;

    virtual lix::value call(context&, const lix::value&) const = 0;
};

template <typename Func>
class erased_fn_impl : public erased_fn_base {
    Func _fn;

    template <typename Void,
              typename = std::enable_if_t<std::is_same<Void, void>::value>,
              typename = void>
    lix::value _do_call(context&, const lix::value&, tag<Void>) const {
        static_assert(
            !std::is_same<Void, void>::value,
            "Invalid function. Must return an object convertible to lix::value, not 'void'");
        return lix::value(0);
    }

    template <typename Ret, typename = std::enable_if<!std::is_same<Ret, void>::value>>
    lix::value _do_call(context& ctx, const lix::value& arg, tag<Ret>) const {
        static_assert(
            std::is_convertible<Ret, lix::value>::value,
            "Invalid function return value. Return type must be convertible to 'lix::value'");
        Ret&& r = _fn(ctx, arg);
        return lix::value(std::forward<Ret>(r));
    }

public:
    template <typename T>
    erased_fn_impl(T&& fn)
        : _fn(std::forward<T>(fn)) {}

    lix::value call(context& ctx, const lix::value& val) const override {
        using ret_type = decltype(_fn(ctx, val));
        return _do_call(ctx, val, lix::tag<ret_type>{});
    }
};

}  // namespace detail

template <typename Function, typename Void, typename Any>
function::function(Function&& fn)
    : _func(std::make_shared<detail::erased_fn_impl<std::decay_t<Function>>>(
          std::forward<Function>(fn))) {}

inline std::ostream& operator<<(std::ostream& o, const function&) {
    o << "<lix::exec::function>";
    return o;
}

}  // namespace lix::exec

#endif  // LIX_EXEC_FN_HPP_INCLUDED