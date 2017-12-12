#ifndef LET_PARSER_NODE_HPP_INCLUDED
#define LET_PARSER_NODE_HPP_INCLUDED

#include <variant>

#include "triple.hpp"
#include "literal.hpp"

namespace let {

namespace ast {

class node {
    using node_var = std::variant<triple, literal>;
};

} // namespace ast

} // namespace let

#endif // LET_PARSER_NODE_HPP_INCLUDED