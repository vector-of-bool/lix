#ifndef LIX_COMPILER_COMPILE_HPP_INCLUDED
#define LIX_COMPILER_COMPILE_HPP_INCLUDED

#include <lix/code/code.hpp>
#include <lix/util/opt_ref.hpp>

namespace lix {

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

}  // namespace lix

#endif  // LIX_COMPILER_COMPILE_HPP_INCLUDED