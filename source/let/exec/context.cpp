#include "context.hpp"

using namespace let;
using namespace let::exec;

namespace let::exec::detail {

class context_impl {
public:
    std::map<std::string, module, std::less<>>     _modules;
    std::vector<std::map<std::string, let::value>> _environments;

    friend struct inst_evaluator;

    void register_module(const std::string& name, module mod) {
        const auto did_insert = _modules.emplace(name, std::move(mod)).second;
        if (!did_insert) {
            throw std::runtime_error{"Double-registered module"};
        }
    }
};

}  // namespace let::exec::detail

context::context()
    : _impl(std::make_unique<let::exec::detail::context_impl>()) {}

context::~context() = default;

context::context(context&&) = default;
context& context::operator=(context&&) = default;

void context::register_module(const std::string& name, let::exec::module mod) {
    _impl->register_module(name, mod);
}

std::optional<let::exec::module> context::get_module(const std::string_view& name) const {
    auto mod_iter = _impl->_modules.find(name);
    if (mod_iter == _impl->_modules.end()) {
        return std::nullopt;
    } else {
        return mod_iter->second;
    }
}

void context::set_environment_value(const std::string& name, let::value val) {
    if (_impl->_environments.empty()) {
        throw std::runtime_error{"No environment"};
    }
    _impl->_environments.back().emplace(name, std::move(val));
}

std::optional<let::value> context::get_environment_value(const std::string& name) const {
    auto iter = _impl->_environments.rbegin();
    auto end  = _impl->_environments.rend();
    while (iter != end) {
        auto found = iter->find(name);
        if (found != iter->end()) {
            return found->second;
        }
        ++iter;
    }
    return std::nullopt;
}

void context::_push_environment() { _impl->_environments.emplace_back(); }
void context::_pop_environment() { _impl->_environments.pop_back(); }
