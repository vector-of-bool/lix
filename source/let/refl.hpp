#ifndef LET_REFL_HPP_INCLUDED
#define LET_REFL_HPP_INCLUDED

#include <cinttypes>
#include <cstdlib>
#include <memory>
#include <optional>
#include <type_traits>

#include "util.hpp"

#define LET_CAT_1(a, ...) a##__VA_ARGS__
#define LET_CAT(a, ...) LET_CAT_1(a, __VA_ARGS__)

#define LET_EMPTY

#define LET_EVAL1(...) LET_EVAL2(LET_EVAL2(__VA_ARGS__))
#define LET_EVAL2(...) LET_EVAL3(LET_EVAL3(__VA_ARGS__))
#define LET_EVAL3(...) LET_EVAL4(LET_EVAL4(__VA_ARGS__))
#define LET_EVAL4(...) LET_EVAL5(LET_EVAL5(__VA_ARGS__))
#define LET_EVAL5(...) LET_EVAL6(LET_EVAL6(__VA_ARGS__))
#define LET_EVAL6(...) LET_EVAL7(LET_EVAL7(__VA_ARGS__))
#define LET_EVAL7(...) __VA_ARGS__

#define LET_MAP_END(...)
#define LET_MAP_GET_END(...) 0, LET_MAP_END

#define LET_MAP_NEXT_1(test, next, ...) next LET_EMPTY
#define LET_MAP_NEXT_0(test, next) LET_MAP_NEXT_1(test, next, 0)
#define LET_MAP_NEXT(test, next) LET_MAP_NEXT_0(LET_MAP_GET_END test, next)

#define LET_MAP_A(f, c, counter, x, peek, ...)                                                     \
    f(c, counter, x) LET_MAP_NEXT(peek, LET_MAP_B)(f, c, counter + 1, peek, __VA_ARGS__)
#define LET_MAP_B(f, c, counter, x, peek, ...)                                                     \
    f(c, counter, x) LET_MAP_NEXT(peek, LET_MAP_A)(f, c, counter + 1, peek, __VA_ARGS__)

#define LET_MAP(f, c, ...) LET_EVAL1(LET_MAP_A(f, c, 0, __VA_ARGS__, (), 0))

namespace let {

namespace refl {

namespace detail {

template <typename... Voids> struct counter {
    static constexpr std::size_t size = sizeof...(Voids);
};

template <typename T> struct identity { using type = T; };

template <typename Value, typename Parent> identity<Value> typeof_memptr(Value Parent::*);
template <typename Ret, typename Parent, typename... Args>
identity<Ret(Args...)> typeof_memptr(Ret (Parent::*)(Args...));

}  // namespace detail

template <typename Target,
          typename From,
          typename = std::enable_if_t<std::is_convertible<From, Target>::value>>
Target convert(From&& fr) {
    return Target(std::forward<From>(fr));
}

template <typename Owner, std::size_t Index> struct member_info;

template <typename T> struct conversions {
    using from = let::tag<>;
    using to = let::tag<>;
};

template <typename... MemInfo> struct member_list {};

template <typename T> struct ct_base_type_info {};

template <typename T, typename Void = void> struct is_reflected : std::false_type {};
template <typename T>
struct is_reflected<T, std::void_t<typename ct_base_type_info<T>::type>> : std::true_type {};

struct type_id {
    type_id() = default;
    type_id(const type_id&) = delete;
    type_id& operator=(const type_id&) = delete;
};

inline constexpr bool operator==(const type_id& lhs, const type_id& rhs) {
    return &lhs == &rhs;
}

inline constexpr bool operator!=(const type_id& lhs, const type_id& rhs) {
    return &lhs != &rhs;
}

template <typename T> const type_id& get_type_id() {
    static type_id ret;
    return ret;
}

template <typename T> using remove_cvr_t = std::remove_cv_t<std::remove_reference_t<T>>;

template <typename T> class ct_type_info {
public:
    using type = T;
    using base_type = remove_cvr_t<type>;

private:
    static_assert(is_reflected<base_type>::value,
                  "Cannot get compile-type type info for non-reflected type");

public:
    using base_info = ct_base_type_info<base_type>;

    constexpr static auto member_count = base_info::member_count;
    template <std::size_t N> using nth_member = typename base_info::template nth_member<N>;
    static const std::string& name() {
        static const std::string name = _build_name();
        return name;
    }

    template <typename Fun> static void foreach_member(Fun&& fn) {
        _foreach_member(std::forward<Fun>(fn), std::integral_constant<std::size_t, 0>{});
    }

    static const type_id& id() noexcept {
        return get_type_id<T>();
    }

public:
    enum : bool {
        is_const = std::is_const<std::remove_reference_t<T>>::value,
        is_volatile = std::is_volatile<std::remove_reference_t<T>>::value,
        is_lref = std::is_lvalue_reference<std::remove_cv_t<T>>::value,
        is_rref = std::is_rvalue_reference<std::remove_cv_t<T>>::value,
    };

private:
    template <typename Fn>
    static void _foreach_member(Fn&&, std::integral_constant<std::size_t, member_count>) {
        // Base case
    }
    template <typename Fn, std::size_t S>
    static void _foreach_member(Fn&& fn, std::integral_constant<std::size_t, S>) {
        std::forward<Fn>(fn)(nth_member<S>{});
        _foreach_member(std::forward<Fn>(fn), std::integral_constant<std::size_t, S + 1>{});
    }

    static std::string _build_name() {
        std::string ret = base_info::name;
        if (is_const) {
            ret += " const";
        }
        if (is_volatile) {
            ret += " volatile";
        }
        if (is_lref) {
            ret += "&";
        }
        if (is_rref) {
            ret += "&&";
        }
        return ret;
    }
};

namespace detail {

class rtti_base {
public:
    virtual ~rtti_base() = default;
    virtual const std::string& name() const noexcept = 0;
    virtual const let::refl::type_id& id() const noexcept = 0;
    virtual std::shared_ptr<rtti_base> without_qualifiers() const = 0;
    virtual bool is_const() const noexcept = 0;
    virtual bool is_volatile() const noexcept = 0;
    virtual bool is_lref() const noexcept = 0;
    virtual bool is_rref() const noexcept = 0;
    virtual void
    unsafe_try_convert(const type_id& out_type, void* out_opt, const void* in) const = 0;
};

template <typename T> class rtti_impl : public rtti_base {
    using ct_info = ct_type_info<T>;
    const std::string& name() const noexcept override {
        return ct_info::name();
    }
    const let::refl::type_id& id() const noexcept override {
        return ct_info::id();
    }
    std::shared_ptr<rtti_base> without_qualifiers() const override {
        return std::make_shared<rtti_impl<remove_cvr_t<T>>>();
    }
    bool is_const() const noexcept override {
        return ct_info::is_const;
    }
    bool is_volatile() const noexcept override {
        return ct_info::is_volatile;
    }
    bool is_lref() const noexcept override {
        return ct_info::is_lref;
    }
    bool is_rref() const noexcept override {
        return ct_info::is_rref;
    }
    void unsafe_try_convert(const type_id& out_type, void* out_opt, const void* in) const override {
        using base_type = remove_cvr_t<T>;
        using out_candidates = typename conversions<base_type>::to;
        const base_type& in_ref = *reinterpret_cast<const base_type*>(in);
        unsafe_try_convert(out_type, out_opt, in_ref, out_candidates());
    }

    template <typename... OutCandidates>
    void unsafe_try_convert(const type_id& out_type,
                            void* out_opt [[maybe_unused]],
                            const T& in,
                            let::tag<OutCandidates...>) const {
        (unsafe_try_convert_1(out_type, out_opt, in, let::tag<OutCandidates>()), ...);
    }
    template <typename OutCandidate>
    void unsafe_try_convert_1(const type_id& out_type,
                              void* out_opt,
                              const T& in,
                              let::tag<OutCandidate>) const {
        if (out_type == get_type_id<OutCandidate>()) {
            // This is the one!
            auto& out_opt_ref = *reinterpret_cast<std::optional<OutCandidate>*>(out_opt);
            using let::refl::convert;
            out_opt_ref.emplace(convert<OutCandidate>(in));
        }
    }
};

template <typename Void, typename... Info>
using make_member_list_t = ::let::refl::member_list<Info...>;

}  // namespace detail

class rt_type_info {
    std::shared_ptr<const detail::rtti_base> _impl;

    explicit rt_type_info(std::shared_ptr<const detail::rtti_base> ptr)
        : _impl(ptr) {
    }

public:
    template <typename T>
    explicit rt_type_info(ct_type_info<T>)
        : _impl(std::make_shared<detail::rtti_impl<T>>()) {
    }

    const std::string& name() const noexcept {
        return _impl->name();
    }

    const let::refl::type_id& id() const noexcept {
        return _impl->id();
    }

    template <typename T> static rt_type_info for_type() {
        return rt_type_info(ct_type_info<T>{});
    }

    rt_type_info without_qualifiers() const {
        return rt_type_info{_impl->without_qualifiers()};
    }

    bool is_const() const noexcept {
        return _impl->is_const();
    }
    bool is_volatile() const noexcept {
        return _impl->is_volatile();
    }
    bool is_lref() const noexcept {
        return _impl->is_lref();
    }
    bool is_rref() const noexcept {
        return _impl->is_rref();
    }
    bool is_reference() const noexcept {
        return is_lref() || is_rref();
    }

    template <typename Target>
    void unsafe_try_convert(std::optional<Target>& out, const void* vptr) {
        auto&& target_type = get_type_id<Target>();
        _impl->unsafe_try_convert(target_type, std::addressof(out), vptr);
    }
};

inline bool operator==(const rt_type_info& lhs, const rt_type_info& rhs) {
    return lhs.id() == rhs.id();
}

inline bool operator!=(const rt_type_info& lhs, const rt_type_info& rhs) {
    return !(lhs == rhs);
}

}  // namespace refl

using integer = std::int64_t;

namespace refl {

template <> struct conversions<let::integer> {
    using from = let::tag<std::int8_t,
                          std::int16_t,
                          std::int32_t,
                          std::int64_t,
                          std::uint8_t,
                          std::uint16_t,
                          std::uint32_t>;
    using to = from;
};

}  // namespace refl

}  // namespace let

#define LET_REFL_DECLARE_MEMINFO(parent_type, idx, memname)                                        \
    template <> struct member_info<parent_type, idx> {                                             \
        constexpr static const char* name = #memname;                                              \
        constexpr static auto pointer = &parent_type::memname;                                     \
        using type = typename decltype(::let::refl::detail::typeof_memptr(pointer))::type;         \
        using type_info = ::let::refl::ct_type_info<type>;                                         \
    };

#define LET_VOID_PARAM(...) , void
#define LET_MEMBER_LIST_HELPER(dummy, idx, dummy2) , nth_member<idx>

#define LET_TYPEINFO(type_param, ...)                                                              \
    namespace let::refl {                                                                          \
    LET_MAP(LET_REFL_DECLARE_MEMINFO, type_param, __VA_ARGS__)                                     \
    template <> struct ct_base_type_info<type_param> {                                             \
        using type = type_param;                                                                   \
        constexpr static const char* name = #type_param;                                           \
        constexpr static auto member_count                                                         \
            = ::let::refl::detail::counter<void LET_MAP(LET_VOID_PARAM, ~, __VA_ARGS__)>::size     \
            - 1;                                                                                   \
        template <std::size_t N> using nth_member = ::let::refl::member_info<type_param, N>;       \
        using members                                                                              \
            = ::let::refl::detail::make_member_list_t<void LET_MAP(LET_MEMBER_LIST_HELPER,         \
                                                                   ~,                              \
                                                                   __VA_ARGS__)>;                  \
    };                                                                                             \
    }                                                                                              \
    static_assert(true)

#define LET_BASIC_TYPEINFO(type_param)                                                             \
    namespace let::refl {                                                                          \
    template <> struct ct_base_type_info<type_param> {                                             \
        using type = type_param;                                                                   \
        constexpr static const char* name = #type_param;                                           \
        constexpr static auto member_count = 0;                                                    \
        template <typename Fun> static void foreach_member(Fun&&) {                                \
        }                                                                                          \
        template <std::size_t> struct nth_member;                                                  \
        using members = ::let::refl::member_list<>;                                                \
    };                                                                                             \
    }                                                                                              \
    static_assert(true)

// LET_BASIC_TYPEINFO(std::int8_t);
// LET_BASIC_TYPEINFO(std::uint8_t);
// LET_BASIC_TYPEINFO(std::int16_t);
// LET_BASIC_TYPEINFO(std::uint16_t);
// LET_BASIC_TYPEINFO(std::int32_t);
// LET_BASIC_TYPEINFO(std::uint32_t);
LET_BASIC_TYPEINFO(let::integer);
// LET_BASIC_TYPEINFO(std::uint64_t);
LET_BASIC_TYPEINFO(char);
LET_BASIC_TYPEINFO(bool);

LET_BASIC_TYPEINFO(std::string);

#endif  // LET_REFL_HPP_INCLUDED