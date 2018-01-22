#ifndef LET_PARSER_TOKEN_HPP_INCLUDED
#define LET_PARSER_TOKEN_HPP_INCLUDED

#include <let/string_view.hpp>

#include <cassert>
#include <ostream>
#include <tuple>
#include <vector>


namespace let {

namespace lex {

enum class token_type {
    word,
    integer,
    decimal,
    newline,
    eof,

    // Symbols
    dot,
    plus,
    minus,
    div,
    mul,
    l_paren,
    r_paren,
    l_bracket,
    r_bracket,
    bar,
    semicolon,
    comma,
};

/**
 * Represents a location in source code. The line and column are one-based, not
 * zero-based
 */
struct source_location {
    /// The line. (Starts at one)
    int line = 1;
    /// The column. (Starts at one)
    int col = 1;
    // Tie. For comparisons
    auto tie() const& noexcept {
        return std::tie(line, col);
    }
};

/**
 * Compare two locations.
 */
inline bool operator<(source_location lhs, source_location rhs) {
    return lhs.tie() < rhs.tie();
}

/**
 * Compare two locations.
 */
inline bool operator==(source_location lhs, source_location rhs) {
    return lhs.tie() == rhs.tie();
}

/**
 * Pretty-print a source location
 */
inline std::ostream& operator<<(std::ostream& os, source_location l) {
    os << "line " << l.line << ", column " << l.col;
    return os;
}

/**
 * Represents a half-open range of characters in source.
 */
struct source_range {
    /**
     * The first character
     */
    source_location start;
    /**
     * The past-the-end character
     */
    source_location end;
    /**
     * Construct a source range from two locations
     */
    constexpr source_range(source_location start, source_location end)
        : start(start)
        , end(end) {
        // In debug, check that we're sane
        assert(start < end || start == end);
    }
};

/**
 * Pretty-print a source range
 */
inline std::ostream& operator<<(std::ostream& os, source_range l) {
    os << "[" << l.start << " : " << l.end << ")";
    return os;
}

/**
 * View of a token in source. We are a "view" because we have a non-owning
 * string_view into the actual source string.
 */
class token_view {
    /**
     * The full string representing the token
     */
    let::string_view _str;
    /**
     * The source range where the token appears
     */
    source_range _range;
    /**
     * The basic token type
     */
    token_type _type;

public:
    /**
     * Construct a new token view.
     */
    constexpr explicit token_view(let::string_view str, source_range r, token_type t) noexcept
        : _str(str),
          _range(r),
          _type(t) {
    }

    /**
     * Get the string spelling of the token
     */
    constexpr let::string_view string() const noexcept {
        return _str;
    }

    /**
     * Get the source range of the token.
     */
    constexpr source_range range() const noexcept {
        return _range;
    }

    /**
     * Get the type of the token
     */
    constexpr token_type type() const noexcept {
        return _type;
    }
};

inline std::ostream& operator<<(std::ostream& o, const token_view& tok) {
    o << "<let::ast::token_view spelling=\"" << tok.string() << "\" range=" << tok.range()
      << " type=" << int(tok.type()) << '>';
    return o;
}

/**
 * Tokenize a string_view range of a source document.
 *
 * @return A list of tokens into the file.
 *
 * @note The input range must outlive the returned token views.
 */
std::vector<token_view> tokenize(let::string_view::iterator, let::string_view::iterator);

/**
 * Tokenize a string_view of a source document.
 */
inline std::vector<token_view> tokenize(let::string_view str) {
    return tokenize(str.begin(), str.end());
}

}  // namespace lex

}  // namespace let

#endif  // LET_PARSER_TOKEN_HPP_INCLUDED
