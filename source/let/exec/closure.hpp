#ifndef LET_EXEC_CLOSURE_HPP_INCLUDED
#define LET_EXEC_CLOSURE_HPP_INCLUDED

#include <let/code/code.hpp>
#include <let/code/types.hpp>
#include <let/value_fwd.hpp>

#include <ostream>
#include <vector>

namespace let::exec {

class context;

class closure {
    let::code::code         _code;
    let::code::iterator     _first_instr;
    std::vector<let::value> _captures;

public:
    closure(code::code c, code::iterator iter, std::vector<let::value> caps)
        : _code(c)
        , _first_instr(iter)
        , _captures(std::move(caps)) {}

    const code::code& code() const { return _code; }
    code::iterator    code_begin() const { return _first_instr; }
    auto&             captures() const { return _captures; }
};

inline std::ostream& operator<<(std::ostream& o, const closure&) {
    o << "<let::exec::closure>";
    return o;
}

}  // namespace let::exec

#endif  // LET_EXEC_CLOSURE_HPP_INCLUDED