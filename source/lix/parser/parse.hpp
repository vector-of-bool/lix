#ifndef LIX_PARSER_PARSE_HPP_INCLUDED
#define LIX_PARSER_PARSE_HPP_INCLUDED

#include <lix/string_view.hpp>

#include "node.hpp"

#include <stdexcept>

namespace lix::ast {

class parse_error : public std::runtime_error {
    std::string _line_str;
    int         _line_number;
    int         _column;
    std::string _message;

    std::string _what;

public:
    parse_error(const std::string& message, int line, int col, const std::string& line_string)
        : runtime_error("")
        , _line_str(line_string)
        , _line_number(line)
        , _column(col)
        , _message(message) {
        _what = line_string + "\n" + std::string(col, ' ') + "^\n" + "Syntax error: " + message;
    }

    const char* what() const noexcept override { return _what.data(); }
};

node parse(std::string_view::iterator first, std::string_view::iterator last);

inline node parse(std::string_view str) { return parse(str.begin(), str.end()); }

}  // namespace lix::ast

#endif  // LIX_PARSER_PARSE_HPP_INCLUDED
