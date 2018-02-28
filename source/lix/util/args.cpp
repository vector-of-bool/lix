#include "args.hpp"

#include <lix/parser/node.hpp>

#include <cassert>

const lix::ast::node& lix::macro_argument_parser::nth(const std::size_t n) const noexcept {
    assert(n < count());
    return _args.nodes[n];
}

lix::opt_ref<const lix::ast::node>
lix::macro_argument_parser::keyword_get(const std::string_view& kw) const noexcept {
    if (count() == 0) {
        return nullopt;
    }
    auto tail_arg = nth(count() - 1);
    auto kwlist   = tail_arg.as_list();
    if (!kwlist) {
        return nullopt;
    }
    for (auto& node : kwlist->nodes) {
        auto tup = node.as_tuple();
        if (!tup) {
            continue;
        }
        if (tup->nodes.size() != 2) {
            continue;
        }
        auto kw_sym = tup->nodes[0].as_symbol();
        if (!kw_sym) {
            continue;
        }
        if (kw_sym->string() == kw) {
            return tup->nodes[1];
        }
    }
    return nullopt;
}

std::size_t lix::macro_argument_parser::count() const noexcept { return _args.nodes.size(); }

namespace {

const lix::tuple& get_as_tup(const lix::value& val) {
    auto tup = val.as_tuple();
    if (!tup) {
        throw std::runtime_error{"Argument object must be a tuple"};
    }
    return *tup;
}

}  // namespace

lix::argument_parser::argument_parser(const lix::value& value)
    : _args(get_as_tup(value)) {}

std::size_t       lix::argument_parser::count() const noexcept { return _args.size(); }
const lix::value& lix::argument_parser::nth(const std::size_t n) const noexcept {
    assert(n < count());
    return _args[n];
}

namespace {

const lix::list& get_as_list(const lix::value& val) {
    auto list = val.as_list();
    if (!list) {
        throw std::runtime_error{"Argument object must be a list"};
    }
    return *list;
}

}  // namespace

lix::keyword_parser::keyword_parser(const lix::value& value)
    : _list(get_as_list(value)) {}


lix::opt_ref<const lix::value> lix::keyword_parser::get(const std::string_view& key) const noexcept {
    for (auto& val : _list) {
        auto tup = val.as_tuple();
        if (!tup) {
            continue;
        }
        if (tup->size() != 2) {
            continue;
        }
        auto kw_sym = (*tup)[0].as_symbol();
        if (!kw_sym) {
            continue;
        }
        if (kw_sym->string() == key) {
            return (*tup)[1];
        }
    }
    return lix::nullopt;
}
