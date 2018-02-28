#ifndef LIX_EXEC_CLOSURE_HPP_INCLUDED
#define LIX_EXEC_CLOSURE_HPP_INCLUDED

#include <lix/code/code.hpp>
#include <lix/code/types.hpp>
#include <lix/value_fwd.hpp>

#include <ostream>
#include <vector>

namespace lix::exec {

class context;

class closure {
    lix::code::code         _code;
    lix::code::iterator     _first_instr;
    std::vector<lix::value> _captures;

public:
    closure(code::code c, code::iterator iter, std::vector<lix::value> caps)
        : _code(c)
        , _first_instr(iter)
        , _captures(std::move(caps)) {}

    const code::code& code() const { return _code; }
    code::iterator    code_begin() const { return _first_instr; }
    auto&             captures() const { return _captures; }
};

inline std::ostream& operator<<(std::ostream& o, const closure&) {
    o << "<lix::exec::closure>";
    return o;
}

}  // namespace lix::exec

#endif  // LIX_EXEC_CLOSURE_HPP_INCLUDED