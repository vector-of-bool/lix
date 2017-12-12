#ifndef LET_PARSER_PARSE_HPP_INCLUDED
#define LET_PARSER_PARSE_HPP_INCLUDED

#include "node.hpp"

#include <string_view>

namespace let {

ast::node parse_string(std::string_view str);

} // namespace let

#endif // LET_PARSER_PARSE_HPP_INCLUDED