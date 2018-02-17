#ifndef LET_EXEC_EXEC_HPP_INCLUDED
#define LET_EXEC_EXEC_HPP_INCLUDED

#include <let/exec/context.hpp>
#include <let/exec/stack.hpp>
#include <let/raise.hpp>

#include <memory>
#include <stdexcept>

namespace let::exec {

namespace detail {

class executor_impl;

}  // namespace detail

class executor {
    std::unique_ptr<detail::executor_impl> _impl;

public:
    executor();
    explicit executor(const code::code&);
    executor(const code::code&, code::iterator);
    executor(const closure&, const let::value&);
    ~executor();
    executor(executor&&);
    executor& operator=(executor&&);

    std::optional<let::value> execute_n(exec::context&, std::size_t n);
    let::value                execute_all(exec::context&);
};

}  // namespace let::exec

#endif  // LET_EXEC_EXEC_HPP_INCLUDED