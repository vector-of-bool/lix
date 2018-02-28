#ifndef LIX_EXEC_EXEC_HPP_INCLUDED
#define LIX_EXEC_EXEC_HPP_INCLUDED

#include <lix/exec/context.hpp>
#include <lix/exec/stack.hpp>
#include <lix/raise.hpp>

#include <memory>
#include <stdexcept>

namespace lix::exec {

namespace detail {

class executor_impl;

}  // namespace detail

class executor {
    std::unique_ptr<detail::executor_impl> _impl;

public:
    executor();
    explicit executor(const code::code&);
    executor(const code::code&, code::iterator);
    executor(const closure&, const lix::value&);
    ~executor();
    executor(executor&&);
    executor& operator=(executor&&);

    std::optional<lix::value> execute_n(exec::context&, std::size_t n);
    lix::value                execute_all(exec::context&);
};

}  // namespace lix::exec

#endif  // LIX_EXEC_EXEC_HPP_INCLUDED