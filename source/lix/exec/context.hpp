#ifndef LIX_EXEC_CONTEXT_HPP_INCLUDED
#define LIX_EXEC_CONTEXT_HPP_INCLUDED

#include <lix/exec/closure.hpp>
#include <lix/exec/module.hpp>
#include <lix/exec/stack.hpp>

#include <lix/code/code.hpp>
#include <lix/code/instr.hpp>

#include <lix/boxed.hpp>

#include <cassert>
#include <map>
#include <optional>
#include <stack>
#include <vector>

namespace lix {

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

    std::optional<module> get_module(const std::string_view& name) const;

    void                      set_environment_value(const std::string&, lix::value);
    std::optional<lix::value> get_environment_value(const std::string& name) const;

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

}  // namespace lix

#endif  // LIX_EXEC_CONTEXT_HPP_INCLUDED