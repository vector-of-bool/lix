#ifndef LET_EXEC_CLOSURE_HPP_INCLUDED
#define LET_EXEC_CLOSURE_HPP_INCLUDED

#include <let/code/code.hpp>
#include <let/value_fwd.hpp>

#include <vector>

namespace let::exec {

class context;

class closure {
    let::code::code     _code;
    let::code::iterator _first_instr;

public:
    closure(code::code c, code::iterator iter)
        : _code(c)
        , _first_instr(iter) {}

    const code::code& code() const { return _code; }
    code::iterator    code_begin() const { return _first_instr; }
};

}  // namespace let::exec

#endif  // LET_EXEC_CLOSURE_HPP_INCLUDED