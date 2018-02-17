#ifndef LET_REFL_GET_MEMBER_HPP_INCLUDED
#define LET_REFL_GET_MEMBER_HPP_INCLUDED

#include <let/value.hpp>

namespace let::refl::detail {

struct getmem_check_helper_tag {};

}  // namespace let::refl::detail

template <typename MemInfo>
let::value let::refl::detail::rt_meminfo_impl<MemInfo>::get(const void* ptr_) const noexcept {
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

inline let::value let::refl::rt_member_info::get_unsafe(const void* ptr) const noexcept {
    return _impl->get(ptr);
}

#endif  // LET_REFL_GET_MEMBER_HPP_INCLUDED