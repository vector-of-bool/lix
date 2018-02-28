#ifndef LIX_LIBS_LIBS_HPP_INCLUDED
#define LIX_LIBS_LIBS_HPP_INCLUDED

#include <lix/exec/context.hpp>
#include <lix/exec/kernel.hpp>

#include <lix/libs/Enum.hpp>
#include <lix/libs/File.hpp>
#include <lix/libs/IO.hpp>
#include <lix/libs/Keyword.hpp>
#include <lix/libs/Map.hpp>
#include <lix/libs/Path.hpp>
#include <lix/libs/Regex.hpp>
#include <lix/libs/String.hpp>

namespace lix::libs {

template <typename... Libs>
void add_libraries(exec::context& ctx) {
    (Libs::eval(ctx), ...);
}

template <typename... Libs>
exec::context create_context() {
    auto kernel_ctx = lix::exec::build_kernel_context();
    add_libraries<Libs...>(kernel_ctx);
    return std::move(kernel_ctx);
}

}  // namespace lix::libs

#endif  // LIX_LIBS_LIBS_HPP_INCLUDED