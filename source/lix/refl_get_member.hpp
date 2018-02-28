#ifndef LIX_REFL_GET_MEMBER_HPP_INCLUDED
#define LIX_REFL_GET_MEMBER_HPP_INCLUDED

#include <lix/value.hpp>

namespace lix::refl::detail {

struct getmem_check_helper_tag {};

}  // namespace lix::refl::detail

template <typename MemInfo>
lix::value lix::refl::detail::rt_meminfo_impl<MemInfo>::get(const void* ptr_) const noexcept {
    (void)ptr_;
    if constexpr (MemInfo::is_gettable) {
        auto& ref = *reinterpret_cast<const typename MemInfo::parent*>(ptr_);
        if constexpr (MemInfo::is_method) {
            return (ref.*(MemInfo::pointer))();
        } else {
            return ref.*(MemInfo::pointer);
        }
    } else {
        assert(false && "Call to rt_member_info::get() for non-gettable member");
        std::terminate();
    }
}

inline lix::value lix::refl::rt_member_info::get_unsafe(const void* ptr) const noexcept {
    return _impl->get(ptr);
}

#endif  // LIX_REFL_GET_MEMBER_HPP_INCLUDED