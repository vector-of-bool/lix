#ifndef LIX_UTIL_ARGS_HPP_INCLUDED
#define LIX_UTIL_ARGS_HPP_INCLUDED

#include <lix/exec/closure.hpp>
#include <lix/parser/node.hpp>
#include <lix/util.hpp>
#include <lix/util/opt_ref.hpp>
#include <lix/value.hpp>

#include <tuple>

namespace lix {

namespace detail {

template <std::size_t I>
const lix::symbol& unpack_one(const lix::tuple& input, tag<lix::symbol>) {
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
const lix::string& unpack_one(const lix::tuple& input, tag<lix::string>) {
    if (input.size() <= I) {
        throw std::runtime_error{"Not enough arguments to unpack"};
    }
    auto sym_ptr = input[I].as_string();
    if (!sym_ptr) {
        throw std::runtime_error{"Argument is not a string"};
    }
    return *sym_ptr;
}

template <std::size_t I>
const lix::list& unpack_one(const lix::tuple& input, tag<lix::list>) {
    if (input.size() <= I) {
        throw std::runtime_error{"Not enough arguments to unpack"};
    }
    auto list_ptr = input[I].as_list();
    if (!list_ptr) {
        throw std::runtime_error{"Argument is not a list"};
    }
    return *list_ptr;
}

template <std::size_t I>
const lix::map& unpack_one(const lix::tuple& input, tag<lix::map>) {
    if (input.size() <= I) {
        throw std::runtime_error{"Not enough arguments to unpack"};
    }
    auto map_ptr = input[I].as_map();
    if (!map_ptr) {
        throw std::runtime_error{"Argument is not a map"};
    }
    return *map_ptr;
}

template <std::size_t I>
const lix::boxed& unpack_one(const lix::tuple& input, tag<lix::boxed>) {
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
const lix::value& unpack_one(const lix::tuple& input, tag<lix::value>) {
    if (input.size() <= I) {
        throw std::runtime_error{"Not enough arguments to unpack"};
    }
    return input[I];
}

template <std::size_t I, typename T, typename = std::enable_if_t<is_boxable<T>::value>>
const T& unpack_one(const lix::tuple& input, tag<const T>) {
    const auto& boxed = unpack_one<I>(input, tag<lix::boxed>());
    return box_cast<std::decay_t<T>>(boxed);
}

template <std::size_t I, typename T, typename = std::enable_if_t<is_boxable<T>::value>>
T& unpack_one(const lix::tuple& input, tag<T>) {
    const auto& boxed = unpack_one<I>(input, tag<lix::boxed>());
    return mut_box_cast<std::decay_t<T>>(boxed);
}

template <std::size_t I>
const lix::exec::function& unpack_one(const lix::tuple& input, tag<lix::exec::function>) {
    auto clos_ptr = input[I].as_function();
    if (!clos_ptr) {
        throw std::runtime_error{"Argument is not a funtion"};
    }
    return *clos_ptr;
}

template <std::size_t I>
const lix::exec::closure& unpack_one(const lix::tuple& input, tag<lix::exec::closure>) {
    auto clos_ptr = input[I].as_closure();
    if (!clos_ptr) {
        throw std::runtime_error{"Argument is not a closure"};
    }
    return *clos_ptr;
}

template <typename... Types, std::size_t... Is>
decltype(auto) do_unpack_arg_tuple(const lix::value& input, std::index_sequence<Is...>) {
    auto tup_ptr = input.as_tuple();
    if (!tup_ptr) {
        throw std::runtime_error{"Cannot unpack tuple of arguments from non-tuple"};
    }
    return std::tie(unpack_one<Is>(*tup_ptr, tag<Types>{})...);
}

}  // namespace detail

template <typename... Types>
decltype(auto) unpack_arg_tuple(const lix::value& input) {
    return lix::detail::do_unpack_arg_tuple<Types...>(input, std::index_sequence_for<Types...>{});
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
    template <typename T>
    const T& nth_as_req(std::size_t n) const {
        auto ref = nth_as<T>(n);
        if (!ref) {
            throw std::invalid_argument{"Macro argument " + std::to_string(n) + " is of incorrect type"};
        }
        return *ref;
    }
    opt_ref<const ast::node> keyword_get(const std::string_view& kw) const noexcept;
};

struct argument_parser {
private:
    const lix::tuple& _args;

public:
    explicit argument_parser(lix::tuple&& tup)       = delete;
    explicit argument_parser(const lix::tuple&& tup) = delete;
    explicit argument_parser(const lix::tuple& tup)
        : _args(tup) {}
    explicit argument_parser(lix::value&& tup)       = delete;
    explicit argument_parser(const lix::value&& tup) = delete;
    explicit argument_parser(const lix::value& tup);

    std::size_t       count() const noexcept;
    const lix::value& nth(std::size_t n) const noexcept;
    template <typename T>
    opt_ref<const T> nth_as(std::size_t n) const noexcept {
        return nth(n).as(tag<T>());
    }
    opt_ref<const lix::value> keyword_get(const std::string_view& kw) const noexcept;

    template <typename T>
    const T& nth_as_req(std::size_t n) const {
        auto ref = nth_as<T>(n);
        if (!ref) {
            throw std::invalid_argument{"Argument " + std::to_string(n) + " is of incorrect type"};
        }
        return *ref;
    }
};

struct keyword_parser {
private:
    const lix::list& _list;

public:
    explicit keyword_parser(lix::list&& list)       = delete;
    explicit keyword_parser(const lix::list&& list) = delete;
    explicit keyword_parser(const lix::list& list)
        : _list(list) {}
    explicit keyword_parser(lix::value&& list)       = delete;
    explicit keyword_parser(const lix::value&& list) = delete;
    explicit keyword_parser(const lix::value& list);

    opt_ref<const lix::value> get(const std::string_view& kw) const noexcept;
};

}  // namespace lix

#endif  // LIX_UTIL_ARGS_HPP_INCLUDED