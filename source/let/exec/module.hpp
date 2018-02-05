#ifndef LET_EXEC_MODULE_HPP_INCLUDED
#define LET_EXEC_MODULE_HPP_INCLUDED

#include <let/compiler/macro.hpp>
#include <let/exec/closure.hpp>
#include <let/exec/fn.hpp>

#include <let/value.hpp>

#include <memory>
#include <optional>

namespace let::exec {

namespace detail {

struct module_impl;

}  // namespace detail

class module {
    std::shared_ptr<detail::module_impl> _impl;

    void _add_function(const std::string& name, function&& fn);
    void _add_function(const std::string& name, closure&& fn);
    void _add_macro(const std::string& name, let::macro_function&& m);

public:
    module();
    ~module();

    template <typename Func>
    void add_function(const std::string& name, Func&& fn) {
        _add_function(name, exec::function(std::forward<Func>(fn)));
    }

    void add_closure_function(const std::string& name, closure cl) {
        _add_function(name, std::move(cl));
    }

    template <typename Func>
    void add_macro(const std::string& name, Func&& fn) {
        _add_macro(name, let::macro_function(std::forward<Func>(fn)));
    }

    std::optional<std::variant<function, closure>> get_function(const std::string& name) const;
    macro_function*                                get_macro(const std::string& name) const;

    std::optional<let::value> get_attribute(const std::string& name);
    void                      set_attribute(const std::string& name, const let::value&);
};

}  // namespace let::exec

LET_BASIC_TYPEINFO(let::exec::module);

#endif  // LET_EXEC_MODULE_HPP_INCLUDED