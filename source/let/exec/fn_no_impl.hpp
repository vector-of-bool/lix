#ifndef LET_EXEC_FN_NO_IMPL_HPP_INCLUDED
#define LET_EXEC_FN_NO_IMPL_HPP_INCLUDED

#include <memory>
#include <type_traits>

#include <let/value_fwd.hpp>

namespace let::exec {

class stack_element;
class context;

namespace detail {

class erased_fn_base;

} // namespace detail

class function {
    std::shared_ptr<const detail::erased_fn_base> _func;

public:
    template <typename Function,
              typename = std::enable_if_t<!std::is_same<std::decay_t<Function>, function>::value>>
    function(Function&& fn);

    let::value call_ll(context&, const let::value&) const;
};

}  // namespace let::exec

#endif // LET_EXEC_FN_NO_IMPL_HPP_INCLUDED