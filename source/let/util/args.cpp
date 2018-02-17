#include "args.hpp"

#include <let/parser/node.hpp>

#include <cassert>

const let::ast::node& let::macro_argument_parser::nth(const std::size_t n) const noexcept {
    assert(n < count());
    return _args.nodes[n];
}

let::opt_ref<const let::ast::node>
let::macro_argument_parser::keyword_get(const std::string_view& kw) const noexcept {
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

std::size_t let::macro_argument_parser::count() const noexcept { return _args.nodes.size(); }

namespace {

const let::tuple& get_as_tup(const let::value& val) {
    auto tup = val.as_tuple();
    if (!tup) {
        throw std::runtime_error{"Argument object must be a tuple"};
    }
    return *tup;
}

}  // namespace

let::argument_parser::argument_parser(const let::value& value)
    : _args(get_as_tup(value)) {}

std::size_t       let::argument_parser::count() const noexcept { return _args.size(); }
const let::value& let::argument_parser::nth(const std::size_t n) const noexcept {
    assert(n < count());
    return _args[n];
}

namespace {

const let::list& get_as_list(const let::value& val) {
    auto list = val.as_list();
    if (!list) {
        throw std::runtime_error{"Argument object must be a list"};
    }
    return *list;
}

}  // namespace

let::keyword_parser::keyword_parser(const let::value& value)
    : _list(get_as_list(value)) {}


let::opt_ref<const let::value> let::keyword_parser::get(const std::string_view& key) const noexcept {
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
    return let::nullopt;
}
