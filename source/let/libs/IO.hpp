#ifndef LET_LIBS_IO_HPP_INCLUDED
#define LET_LIBS_IO_HPP_INCLUDED

#include <let/exec/context.hpp>

namespace let::libs {

struct IO {
    static void eval(let::exec::context&);
};

} // namespace let::libs

#endif // LET_LIBS_IO_HPP_INCLUDED