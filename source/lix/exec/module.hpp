#ifndef LIX_EXEC_MODULE_HPP_INCLUDED
#define LIX_EXEC_MODULE_HPP_INCLUDED

#include <lix/compiler/macro.hpp>
#include <lix/exec/closure.hpp>
#include <lix/exec/fn.hpp>

#include <lix/value.hpp>

#include <memory>
#include <optional>

namespace lix::exec {

namespace detail {

struct module_impl;

}  // namespace detail

class module {
    std::shared_ptr<detail::module_impl> _impl;

    void _add_function(const std::string& name, function&& fn);
    void _add_function(const std::string& name, closure&& fn);
    void _add_macro(const std::string& name, lix::macro_function&& m);

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
        _add_macro(name, lix::macro_function(std::forward<Func>(fn)));
    }

    std::optional<std::variant<function, closure>> get_function(const std::string_view& name) const;
    opt_ref<macro_function>                        get_macro(const std::string_view& name) const;

    std::optional<lix::value> get_attribute(const std::string& name);
    void                      set_attribute(const std::string& name, const lix::value&);
};

}  // namespace lix::exec

LIX_BASIC_TYPEINFO(lix::exec::module);

#endif  // LIX_EXEC_MODULE_HPP_INCLUDED