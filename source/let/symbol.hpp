#ifndef LET_SYMBOL_HPP_INCLUDED
#define LET_SYMBOL_HPP_INCLUDED

#include <ostream>
#include <string>
#include <string_view>

#include "refl.hpp"

namespace let {

class symbol {
    std::string _str;

public:
    explicit symbol(std::string str)
        : _str(std::move(str)) {
    }

    explicit symbol(const char* const ptr)
        : _str(ptr) {
    }

    explicit symbol(std::string_view str)
        : symbol(std::string(str)) {
    }

    const std::string& string() const noexcept {
        return _str;
    }
};

inline std::ostream& operator<<(std::ostream& o, const let::symbol& s) {
    if (s.string().find(' ') != std::string::npos) {
        o << ":\"" << s.string() << '"';
    } else {
        o << ":" << s.string();
    }
    return o;
}

#define DEF_REL_OP(op)                                                                             \
    inline bool operator op(const symbol& lhs, const symbol& rhs) {                                \
        return std::string_view(lhs.string()) op std::string_view(rhs.string());                   \
    }                                                                                              \
    inline bool operator op(const symbol& lhs, const std::string_view& rhs) {                      \
        return lhs.string() op rhs;                                                                \
    }                                                                                              \
    inline bool operator op(const std::string_view& lhs, const symbol& rhs) {                      \
        return lhs op rhs.string();                                                                \
    }                                                                                              \
    static_assert(true)

DEF_REL_OP(==);
DEF_REL_OP(!=);
DEF_REL_OP(<);
DEF_REL_OP(>);
DEF_REL_OP(<=);
DEF_REL_OP(>=);

#undef DEF_REL_OP

}  // namespace let

LET_BASIC_TYPEINFO(let::symbol);

#endif  // LET_SYMBOL_HPP_INCLUDED