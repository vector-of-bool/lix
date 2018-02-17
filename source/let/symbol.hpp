#ifndef LET_SYMBOL_HPP_INCLUDED
#define LET_SYMBOL_HPP_INCLUDED

#include <ostream>
#include <string>
#include <string_view>
#include <functional>

#include "refl.hpp"

namespace let {

namespace detail {

const std::string* get_intern_symbol_string(const std::string_view&);

} // namespace detail

class symbol {
    const std::string* _str;

public:
    explicit symbol(std::string_view str)
        : _str(detail::get_intern_symbol_string(str)) {
    }

    const std::string& string() const noexcept {
        return *_str;
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

#define DEF_REL_OP(op, oper_type)                                                                  \
    inline bool operator op(const symbol& lhs, const symbol& rhs) {                                \
        return oper_type<const void*>{}(&lhs.string(), &rhs.string());                               \
    }                                                                                              \
    static_assert(true)

DEF_REL_OP(==, std::equal_to);
DEF_REL_OP(!=, std::not_equal_to);
DEF_REL_OP(<, std::less);
DEF_REL_OP(>, std::greater);
DEF_REL_OP(<=, std::less_equal);
DEF_REL_OP(>=, std::greater_equal);

#undef DEF_REL_OP

inline namespace literals {

inline let::symbol operator""_sym(const char* cptr, std::size_t) { return let::symbol(cptr); }

} // namespace literals

}  // namespace let

#endif  // LET_SYMBOL_HPP_INCLUDED