#include "module.hpp"

#include <map>

using namespace let;
using namespace let::exec;

namespace let::exec::detail {

struct module_impl {
    std::map<std::string, std::variant<function, closure>> functions;
    std::map<std::string, macro_function>                  macros;
};

}  // namespace let::exec::detail

module::module()
    : _impl(std::make_shared<detail::module_impl>()) {}

module::~module() = default;

void module::_add_function(const std::string& name, exec::function&& fn) {
    _impl->functions.emplace(name, std::move(fn));
}

void module::_add_function(const std::string& name, exec::closure&& cl) {
    _impl->functions.emplace(name, std::move(cl));
}

void module::_add_macro(const std::string& name, let::macro_function&& fn) {
    _impl->macros.emplace(name, std::move(fn));
}

std::optional<std::variant<function, closure>> module::get_function(const std::string& name) const {
    auto iter = _impl->functions.find(name);
    if (iter == end((_impl->functions))) {
        return std::nullopt;
    } else {
        return iter->second;
    }
}

macro_function* module::get_macro(const std::string& name) const {
    auto iter = _impl->macros.find(name);
    if (iter == _impl->macros.end()) {
        return nullptr;
    } else {
        return &iter->second;
    }
}