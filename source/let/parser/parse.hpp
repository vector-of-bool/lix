#ifndef LET_PARSER_PARSE_HPP_INCLUDED
#define LET_PARSER_PARSE_HPP_INCLUDED

#include <let/string_view.hpp>

#include "node.hpp"

namespace let::ast {

node parse(std::string_view::iterator first, std::string_view::iterator last);

inline node parse(std::string_view str) {
    return parse(str.begin(), str.end());
}

}  // namespace let::ast

#endif  // LET_PARSER_PARSE_HPP_INCLUDED
