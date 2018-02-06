#ifndef LET_UTIL_ARGS_HPP_INCLUDED
#define LET_UTIL_ARGS_HPP_INCLUDED

#include <let/exec/closure.hpp>
#include <let/parser/node.hpp>
#include <let/util.hpp>
#include <let/util/opt_ref.hpp>
#include <let/value.hpp>

#include <tuple>

namespace let {

namespace detail {

template <std::size_t I>
const let::symbol& unpack_one(const let::tuple& input, tag<let::symbol>) {
    if (input.size() <= I) {
        throw std::runtime_error{"Not enough arguments to unpack"};
    }
    auto sym_ptr = input[I].as_symbol();
    if (!sym_ptr) {
        throw std::runtime_error{"Argument is not a symbol"};
    }
    return *sym_ptr;
}

template <std::size_t I>
const let::list& unpack_one(const let::tuple& input, tag<let::list>) {
    if (input.size() <= I) {
        throw std::runtime_error{"Not enough arguments to unpack"};
    }
    auto sym_ptr = input[I].as_list();
    if (!sym_ptr) {
        throw std::runtime_error{"Argument is not a list"};
    }
    return *sym_ptr;
}

template <std::size_t I>
const let::boxed& unpack_one(const let::tuple& input, tag<let::boxed>) {
    if (input.size() <= I) {
        throw std::runtime_error{"Not enough arguments to unpack"};
    }
    auto box_ptr = input[I].as_boxed();
    if (!box_ptr) {
        throw std::runtime_error{"Argument is not boxed"};
    }
    return *box_ptr;
}

template <std::size_t I>
const let::exec::function& unpack_one(const let::tuple& input, tag<let::exec::function>) {
    auto clos_ptr = input[I].as_function();
    if (!clos_ptr) {
        throw std::runtime_error{"Argument is not a funtion"};
    }
    return *clos_ptr;
}

template <std::size_t I>
const let::exec::closure& unpack_one(const let::tuple& input, tag<let::exec::closure>) {
    auto clos_ptr = input[I].as_closure();
    if (!clos_ptr) {
        throw std::runtime_error{"Argument is not a closure"};
    }
    return *clos_ptr;
}

template <typename... Types, std::size_t... Is>
std::tuple<const Types&...> do_unpack_arg_tuple(const let::value& input,
                                                std::index_sequence<Is...>) {
    auto tup_ptr = input.as_tuple();
    if (!tup_ptr) {
        throw std::runtime_error{"Cannot unpack tuple of arguments from non-tuple"};
    }
    return std::tie(unpack_one<Is>(*tup_ptr, tag<Types>{})...);
}

}  // namespace detail

template <typename... Types>
std::tuple<const Types&...> unpack_arg_tuple(const let::value& input) {
    return let::detail::do_unpack_arg_tuple<Types...>(input, std::index_sequence_for<Types...>{});
}

struct macro_argument_parser {
private:
    const ast::list& _args;

public:
    explicit macro_argument_parser(ast::list&& args)       = delete;
    explicit macro_argument_parser(const ast::list&& args) = delete;
    explicit macro_argument_parser(const ast::list& args)
        : _args(args) {}

    std::size_t      count() const noexcept;
    const ast::node& nth(std::size_t n) const noexcept;
    template <typename T>
    opt_ref<const T> nth_as(std::size_t n) const noexcept {
        auto item = nth(n);
        if (auto ptr = item.as(tag<T>())) {
            return *ptr;
        } else {
            return nullopt;
        }
    }
    opt_ref<const ast::node> keyword_get(const std::string_view& kw) const noexcept;
};

struct argument_parser {
private:
    const let::tuple& _args;

public:
    explicit argument_parser(let::tuple&& tup)       = delete;
    explicit argument_parser(const let::tuple&& tup) = delete;
    explicit argument_parser(const let::tuple& tup)
        : _args(tup) {}
    explicit argument_parser(let::value&& tup)       = delete;
    explicit argument_parser(const let::value&& tup) = delete;
    explicit argument_parser(const let::value& tup);

    std::size_t       count() const noexcept;
    const let::value& nth(std::size_t n) const noexcept;
    template <typename T>
    opt_ref<const T> nth_as(std::size_t n) const noexcept {
        return nth(n).as(tag<T>());
    }
    opt_ref<const let::value> keyword_get(const std::string_view& kw) const noexcept;
};

}  // namespace let

#endif  // LET_UTIL_ARGS_HPP_INCLUDED