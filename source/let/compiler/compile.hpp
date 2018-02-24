#ifndef LET_COMPILER_COMPILE_HPP_INCLUDED
#define LET_COMPILER_COMPILE_HPP_INCLUDED

#include <let/code/code.hpp>
#include <let/util/opt_ref.hpp>

namespace let {

namespace ast {

class node;
class meta;

}  // namespace ast

class compile_error : public std::runtime_error {
    int _line   = 0;
    int _column = 0;

public:
    compile_error(const std::string& what, opt_ref<const ast::meta>);

    auto line() const { return _line; }
    auto column() const { return _column; }
};

code::code compile(const ast::node&);

}  // namespace let

#endif  // LET_COMPILER_COMPILE_HPP_INCLUDED