#ifndef LET_BOXED_HPP_INCLUDED
#define LET_BOXED_HPP_INCLUDED

#include <functional>
#include <memory>
#include <optional>
#include <ostream>
#include <type_traits>
#include <utility>

#include "refl.hpp"
#include "value_fwd.hpp"

namespace let {

namespace detail {

class boxed_storage_base {
public:
    virtual std::shared_ptr<boxed_storage_base> clone() const     = 0;
    virtual let::refl::rt_type_info             type_info() const = 0;
    virtual ~boxed_storage_base()                                 = default;
    virtual const void* dataptr() const noexcept                  = 0;
};

template <typename T>
struct unref_type {
    using type = T;
};

template <typename T>
struct unref_type<std::reference_wrapper<T>> {
    using type = T;
};

template <typename RealType, typename Stored>
class boxed_storage : public boxed_storage_base {
    Stored _value;

    static RealType&       unref(RealType& ref) { return ref; }
    static const RealType& unref(const RealType& ref) { return ref; }

    std::shared_ptr<boxed_storage_base> clone() const override {
        return std::make_shared<boxed_storage<RealType, Stored>>(_value);
    }

    let::refl::rt_type_info type_info() const override {
        return let::refl::rt_type_info::for_type<RealType>();
    }

    const void* dataptr() const noexcept override { return std::addressof(unref(_value)); }

public:
    template <typename Other,
              typename = std::enable_if_t<!std::is_same<std::decay_t<Other>, boxed_storage>::value>>
    boxed_storage(Other&& o)
        : _value(std::forward<Other>(o)) {}
};

}  // namespace detail

template <typename T>
struct is_boxable : let::refl::is_reflected<std::decay_t<T>> {};

class boxed {
    std::shared_ptr<detail::boxed_storage_base> _item;

public:
    template <typename T,
              typename RealType = std::decay_t<typename detail::unref_type<std::decay_t<T>>::type>,
              typename          = std::enable_if_t<!std::is_same<RealType, boxed>::value
                                          && is_boxable<RealType>::value>>
    boxed(T&& value)
        : _item(std::make_shared<detail::boxed_storage<RealType, std::decay_t<T>>>(
              std::forward<T>(value))) {}

    boxed(const boxed& other)
        : _item(other._item) {}

    let::refl::rt_type_info type_info() const { return _item->type_info(); }

    const void* get_dataptr() const noexcept { return _item->dataptr(); }
    let::value  get_member(std::string_view member_name) const;
};

class bad_box_cast : std::logic_error {
public:
    bad_box_cast(std::string from, std::string to)
        : std::logic_error{"Invalid let::box_cast from '" + from + "' to '" + to + "'"} {}
};

template <typename T>
T* box_cast(boxed* b) {
    if (b == nullptr) {
        return nullptr;
    }
    if (b->type_info().id() == let::refl::ct_type_info<T>::id()) {
        return reinterpret_cast<T*>(b->get_dataptr());
    }
    return nullptr;
}

template <typename T>
const T* box_cast(const boxed* b) {
    if (b == nullptr) {
        return nullptr;
    }
    if (b->type_info().id() == let::refl::ct_type_info<T>::id()) {
        return reinterpret_cast<const T*>(b->get_dataptr());
    }
    return nullptr;
}

template <typename T>
T* mut_box_cast(const boxed* b) {
    if (b == nullptr) {
        return nullptr;
    }
    if (b->type_info().id() == let::refl::ct_type_info<T>::id()) {
        return const_cast<T*>(reinterpret_cast<const T*>(b->get_dataptr()));
    }
    return nullptr;
}

template <typename T>
T& box_cast(boxed& b) {
    auto ptr = box_cast<T>(&b);
    if (!ptr) {
        throw bad_box_cast(b.type_info().name(), let::refl::ct_type_info<T>::name());
    }
    return *ptr;
}

template <typename T>
const T& box_cast(const boxed& b) {
    auto ptr = box_cast<T>(&b);
    if (!ptr) {
        throw bad_box_cast(b.type_info().name(), let::refl::ct_type_info<T>::name());
    }
    return *ptr;
}

template <typename T>
T& mut_box_cast(const boxed& b) {
    auto ptr = mut_box_cast<T>(&b);
    if (!ptr) {
        throw bad_box_cast(b.type_info().name(), let::refl::ct_type_info<T>::name());
    }
    return *ptr;
}

namespace detail {

template <typename Target, typename FromCandidate>
void try_one_convert(const boxed& box, std::optional<Target>& opt, tag<FromCandidate>) {
    if (let::refl::get_type_id<FromCandidate>() == box.type_info().id()) {
        // We hold the candidate from type!
        using let::refl::convert;
        opt.emplace(convert<Target>(box_cast<FromCandidate>(box)));
    }
}

template <typename Target, typename... Froms>
void convert_to(const boxed& box, std::optional<Target>& out, tag<Froms...>) {
    (try_one_convert(box, out, let::tag<Froms>()), ...);
}

}  // namespace detail

template <typename T>
std::optional<T> try_box_convert(const boxed& b) {
    std::optional<T> ret;
    using froms = typename let::refl::conversions<T>::from;
    detail::convert_to(b, ret, froms());
    if (!ret) {
        // No "from" conversions are defined for T, but maybe our boxed type has
        // a "to" conversion for T
        b.type_info().unsafe_try_convert(ret, b.get_dataptr());
    }
    return ret;
}

template <typename T>
T box_convert(const boxed& b) {
    auto opt = try_box_convert<T>(b);
    if (!opt) {
        if constexpr (let::refl::is_reflected<T>::value) {
            throw bad_box_cast{b.type_info().name(), let::refl::ct_type_info<T>::name()};
        } else {
            throw bad_box_cast{b.type_info().name(), "<Unregistered type>"};
        }
    }
    return *opt;
}

inline std::ostream& operator<<(std::ostream& o, const boxed&) {
    o << "<let::boxed>";
    return o;
}

}  // namespace let

#ifndef LET_VALUE_HPP_INCLUDED
#include "refl_get_member.hpp"
#endif

#endif  // LET_BOXED_HPP_INCLUDED