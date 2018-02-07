#ifndef LET_LIBS_LIBS_HPP_INCLUDED
#define LET_LIBS_LIBS_HPP_INCLUDED

#include <let/exec/context.hpp>
#include <let/exec/kernel.hpp>

#include "Enum.hpp"
#include "IO.hpp"

namespace let::libs {

template <typename... Libs> void add_libraries(exec::context& ctx) {
    (Libs::eval(ctx), ...);
}

template <typename... Libs> exec::context create_context() {
    auto kernel_ctx = let::exec::build_kernel_context();
    add_libraries<Libs...>(kernel_ctx);
    return std::move(kernel_ctx);
}

} // namespace let::libs

#endif // LET_LIBS_LIBS_HPP_INCLUDED