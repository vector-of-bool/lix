#ifndef LET_EXEC_CONTEXT_HPP_INCLUDED
#define LET_EXEC_CONTEXT_HPP_INCLUDED

#include <let/exec/closure.hpp>
#include <let/exec/module.hpp>
#include <let/exec/stack.hpp>

#include <let/code/code.hpp>
#include <let/code/instr.hpp>

#include <let/boxed.hpp>

#include <cassert>
#include <map>
#include <optional>
#include <stack>
#include <vector>

namespace let {

namespace exec {

namespace detail {

class context_impl;

}  // namespace detail

class context {
    std::unique_ptr<detail::context_impl> _impl;

    void _push_environment();
    void _pop_environment();

public:
    context();
    ~context();
    context(context&&);
    context& operator=(context&&);

    std::optional<module> get_module(const std::string& name) const;

    void                      set_environment_value(const std::string&, let::value);
    std::optional<let::value> get_environment_value(const std::string& name) const;

    void register_module(const std::string& name, module mod);

    template <typename Func>
    auto push_environment(Func&& fn) {
        try {
            _push_environment();
            auto&& ret = std::forward<Func>(fn)();
            _pop_environment();
            return std::forward<decltype(ret)>(ret);
        } catch (...) {
            _pop_environment();
            throw;
        }
    }
};

}  // namespace exec

}  // namespace let

#endif  // LET_EXEC_CONTEXT_HPP_INCLUDED