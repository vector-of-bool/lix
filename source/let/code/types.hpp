#ifndef LET_CODE_TYPES_HPP_INCLUDED
#define LET_CODE_TYPES_HPP_INCLUDED

#include <cinttypes>
#include <ostream>

namespace let::code {

struct slot_ref_t {
    std::size_t index;
};

struct inst_offset_t {
    std::size_t index;
};

inline constexpr bool operator==(inst_offset_t l, inst_offset_t r) { return l.index == r.index; }
inline constexpr bool operator!=(inst_offset_t l, inst_offset_t r) { return l.index != r.index; }

inline std::ostream& operator<<(std::ostream& o, slot_ref_t ref) {
    o << '$' << ref.index;
    return o;
}

inline std::ostream& operator<<(std::ostream& o, inst_offset_t off) {
    o << '%' << off.index;
    return o;
}

}  // namespace let::code

#endif  // LET_CODE_TYPES_HPP_INCLUDED