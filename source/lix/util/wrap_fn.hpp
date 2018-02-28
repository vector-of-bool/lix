#ifndef LIX_UTIL_WRAP_FN_HPP_INCLUDED
#define LIX_UTIL_WRAP_FN_HPP_INCLUDED

#include <lix/exec/context.hpp>
#include <lix/value.hpp>
#include <lix/util/args.hpp>

#include <type_traits>
#include <tuple>

namespace lix {

namespace detail {

template <typename T>
struct signature_of : signature_of<decltype(&T::operator())> {};

template <typename Ret, typename... Args>
struct signature_of<Ret (*)(Args...)> {
    using type = Ret(Args...);
};

template <typename Class, typename Ret, typename... Args>
struct signature_of<Ret (Class::*)(Args...)> {
    using type = Ret(Args...);
};

template <typename Class, typename Ret, typename... Args>
struct signature_of<Ret (Class::*)(Args...) const> {
    using type = Ret(Args...);
};

template <typename Class, typename Ret, typename... Args>
struct signature_of<Ret (Class::*)(Args...) const noexcept> {
    using type = Ret(Args...);
};

template <typename Signature>
struct call_wrapped;

template <typename Ret, typename... Args>
struct call_wrapped<Ret(Args...)> {
    template <typename Func>
    static lix::value call(Func&& fn, lix::exec::context&, const lix::value& args) {
        auto arg_tup = lix::unpack_arg_tuple<std::decay_t<Args>...>(args);
        return std::apply(std::forward<Func>(fn), arg_tup);
    }
};

template <typename Func>
class wrapped_function {
    Func _fn;

public:
    template <typename Init>
    wrapped_function(lix::tag<>, Init&& arg)
        : _fn(std::forward<Init>(arg)) {}

    lix::value operator()(lix::exec::context& ctx, const lix::value& args) const {
        using sig_type = typename signature_of<Func>::type;
        return call_wrapped<sig_type>::call(_fn, ctx, args);
    }
};

}

template <typename Func>
detail::wrapped_function<std::decay_t<Func>> wrap_function(Func&& fn) {
    return {lix::tag<>{}, std::forward<Func>(fn)};
}

}  // namespace lix

#endif // LIX_UTIL_WRAP_FN_HPP_INCLUDED