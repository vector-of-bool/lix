#ifndef LET_COMPILER_COMPILE_HPP_INCLUDED
#define LET_COMPILER_COMPILE_HPP_INCLUDED

#include <let/code/code.hpp>

namespace let {

namespace ast {

class node;

} // namespace ast

code::code compile(const ast::node&);

} // namespace let

#endif // LET_COMPILER_COMPILE_HPP_INCLUDED