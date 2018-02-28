#ifndef LIX_EXEC_FN_NO_IMPL_HPP_INCLUDED
#define LIX_EXEC_FN_NO_IMPL_HPP_INCLUDED

#include <memory>
#include <type_traits>

#include <lix/value_fwd.hpp>

namespace lix::exec {

class context;

namespace detail {

class erased_fn_base;

}  // namespace detail

class function {
    std::shared_ptr<const detail::erased_fn_base> _func;

public:
    template <typename Function,
              typename = std::enable_if_t<!std::is_same<std::decay_t<Function>, function>::value>,
              typename = decltype(
                  std::declval<const std::decay_t<Function>&>()(std::declval<lix::exec::context&>(),
                                                                std::declval<const lix::value&>()))>
    function(Function&& fn);

    lix::value call_ll(context&, const lix::value&) const;
};

}  // namespace lix::exec

#endif  // LIX_EXEC_FN_NO_IMPL_HPP_INCLUDED