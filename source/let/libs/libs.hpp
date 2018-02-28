#ifndef LET_LIBS_LIBS_HPP_INCLUDED
#define LET_LIBS_LIBS_HPP_INCLUDED

#include <let/exec/context.hpp>
#include <let/exec/kernel.hpp>

#include <let/libs/Enum.hpp>
#include <let/libs/File.hpp>
#include <let/libs/IO.hpp>
#include <let/libs/Keyword.hpp>
#include <let/libs/Map.hpp>
#include <let/libs/Path.hpp>
#include <let/libs/Regex.hpp>
#include <let/libs/String.hpp>

namespace let::libs {

template <typename... Libs>
void add_libraries(exec::context& ctx) {
    (Libs::eval(ctx), ...);
}

template <typename... Libs>
exec::context create_context() {
    auto kernel_ctx = let::exec::build_kernel_context();
    add_libraries<Libs...>(kernel_ctx);
    return std::move(kernel_ctx);
}

}  // namespace let::libs

#endif  // LET_LIBS_LIBS_HPP_INCLUDED