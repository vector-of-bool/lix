#include "value.hpp"

#include <sstream>

std::string lix::to_string(const lix::value& val) {
    std::stringstream strm;
    strm << val;
    return strm.str();
}

namespace {

struct hash_visitor {
    std::size_t operator()(const lix::map&) const {
        throw std::runtime_error{"Cannot hash map objects"};
    }
    std::size_t operator()(const lix::list&) const {
        throw std::runtime_error{"Cannot hash list objects"};
    }
    std::size_t operator()(const lix::exec::closure&) const {
        throw std::runtime_error{"Cannot hash closure objects"};
    }
    std::size_t operator()(const lix::exec::function&) const {
        throw std::runtime_error{"Cannot hash function objects"};
    }
    std::size_t operator()(const lix::boxed&) const {
        throw std::runtime_error{"Cannot hash opaque boxed objects"};
    }
    std::size_t operator()(const lix::exec::detail::cons&) const {
        assert(false && "Impossible code");
        std::terminate();
    }
    std::size_t operator()(const lix::exec::detail::binding_slot&) const {
        assert(false && "Impossible code");
        std::terminate();
    }
    template <typename Val>
    std::size_t operator()(const Val& v) const {
        return std::hash<Val>()(v);
    }
};

}  // namespace

std::size_t std::hash<lix::value>::operator()(const lix::value& val) const {
    return val.visit(hash_visitor());
}

namespace {

struct inspect_visitor {
    std::string operator()(const lix::string& str) { return "\"" + str + "\""; }
    std::string operator()(const lix::exec::function&) { return "<native-function>"; }
    std::string operator()(const lix::exec::closure&) { return "<closure>"; }
    std::string operator()(const lix::exec::detail::cons&) { return "<cons>"; }
    template <typename T>
    std::string operator()(const T& val) {
        std::stringstream strm;
        strm << val;
        return strm.str();
    }
};

}  // namespace

std::string lix::inspect(const lix::value& val) { return val.visit(inspect_visitor()); }