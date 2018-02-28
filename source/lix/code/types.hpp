#ifndef LIX_CODE_TYPES_HPP_INCLUDED
#define LIX_CODE_TYPES_HPP_INCLUDED

#include <cinttypes>
#include <ostream>

namespace lix::code {

struct slot_ref_t {
    std::size_t index;
};

struct inst_offset_t {
    std::size_t index;
};

inline constexpr bool operator==(inst_offset_t l, inst_offset_t r) { return l.index == r.index; }
inline constexpr bool operator!=(inst_offset_t l, inst_offset_t r) { return l.index != r.index; }

inline constexpr bool operator==(slot_ref_t l, slot_ref_t r) { return l.index == r.index; }
inline constexpr bool operator!=(slot_ref_t l, slot_ref_t r) { return l.index != r.index; }

inline std::ostream& operator<<(std::ostream& o, slot_ref_t ref) {
    o << '$' << ref.index;
    return o;
}

inline std::ostream& operator<<(std::ostream& o, inst_offset_t off) {
    o << '%' << off.index;
    return o;
}

}  // namespace lix::code

#endif  // LIX_CODE_TYPES_HPP_INCLUDED