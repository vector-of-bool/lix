#ifndef LET_PARSER_TOKEN_HPP_INCLUDED
#define LET_PARSER_TOKEN_HPP_INCLUDED

#include <cassert>
#include <ostream>
#include <string_view>
#include <tuple>
#include <vector>


namespace let {

namespace ast {

enum class token_type {
    bareword,
};

struct source_location {
    int line;
    int col;
    auto tie() const & noexcept { return std::tie(line, col); }
};

inline bool operator<(source_location lhs, source_location rhs) { return lhs.tie() < rhs.tie(); }

inline bool operator==(source_location lhs, source_location rhs) { return lhs.tie() == rhs.tie(); }

inline std::ostream& operator<<(std::ostream& os, source_location l) {
    os << "line " << l.line << ", column " << l.col;
    return os;
}

struct source_range {
    source_location start;
    source_location end;
    source_range(source_location start, source_location end)
        : start(start)
        , end(end) {
        assert(start < end);
    }
};

class token_view {
    std::string_view _str;
    std::string_view _file;
    source_range _range;

public:
    explicit token_view(std::string_view str, source_range r) noexcept
        : _str(str)
        , _range(r) {}

    std::string_view string() const noexcept { return _str; }
    source_range range() const noexcept { return _range; }
};

std::vector<token_view> tokenize(std::string_view::iterator, std::string_view::iterator);
inline std::vector<token_view> tokenize(std::string_view str) { return tokenize(str.begin(), str.end()); }

}  // namespace ast

}  // namespace let

#endif  // LET_PARSER_TOKEN_HPP_INCLUDED