#ifndef LET_UTIL_WRAP_FN_HPP_INCLUDED
#define LET_UTIL_WRAP_FN_HPP_INCLUDED

#include <let/exec/context.hpp>
#include <let/value.hpp>
#include <let/util/args.hpp>

#include <type_traits>
#include <tuple>

namespace let {

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
    static let::value call(Func&& fn, let::exec::context&, const let::value& args) {
        auto arg_tup = let::unpack_arg_tuple<std::decay_t<Args>...>(args);
        return std::apply(std::forward<Func>(fn), arg_tup);
    }
};

template <typename Func>
class wrapped_function {
    Func _fn;

public:
    template <typename Init>
    wrapped_function(let::tag<>, Init&& arg)
        : _fn(std::forward<Init>(arg)) {}

    let::value operator()(let::exec::context& ctx, const let::value& args) const {
        using sig_type = typename signature_of<Func>::type;
        return call_wrapped<sig_type>::call(_fn, ctx, args);
    }
};

}

template <typename Func>
detail::wrapped_function<std::decay_t<Func>> wrap_function(Func&& fn) {
    return {let::tag<>{}, std::forward<Func>(fn)};
}

}  // namespace let

#endif // LET_UTIL_WRAP_FN_HPP_INCLUDED