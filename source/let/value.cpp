#include "value.hpp"

#include <sstream>

std::string let::to_string(const let::value& val) {
    std::stringstream strm;
    strm << val;
    return strm.str();
}

namespace {

struct hash_visitor {
    std::size_t operator()(const let::map&) const {
        throw std::runtime_error{"Cannot hash map objects"};
    }
    std::size_t operator()(const let::list&) const {
        throw std::runtime_error{"Cannot hash list objects"};
    }
    std::size_t operator()(const let::exec::closure&) const {
        throw std::runtime_error{"Cannot hash closure objects"};
    }
    std::size_t operator()(const let::exec::function&) const {
        throw std::runtime_error{"Cannot hash function objects"};
    }
    std::size_t operator()(const let::boxed&) const {
        throw std::runtime_error{"Cannot hash opaque boxed objects"};
    }
    std::size_t operator()(const let::exec::detail::cons&) const {
        assert(false && "Impossible code");
        std::terminate();
    }
    std::size_t operator()(const let::exec::detail::binding_slot&) const {
        assert(false && "Impossible code");
        std::terminate();
    }
    template <typename Val>
    std::size_t operator()(const Val& v) const {
        return std::hash<Val>()(v);
    }
};

}  // namespace

std::size_t std::hash<let::value>::operator()(const let::value& val) const {
    return val.visit(hash_visitor());
}

namespace {

struct inspect_visitor {
    std::string operator()(const let::string& str) { return "\"" + str + "\""; }
    std::string operator()(const let::exec::function&) { return "<native-function>"; }
    std::string operator()(const let::exec::closure&) { return "<closure>"; }
    std::string operator()(const let::exec::detail::cons&) { return "<cons>"; }
    template <typename T>
    std::string operator()(const T& val) {
        std::stringstream strm;
        strm << val;
        return strm.str();
    }
};

}  // namespace

std::string let::inspect(const let::value& val) { return val.visit(inspect_visitor()); }