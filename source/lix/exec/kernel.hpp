#ifndef LIX_EXEC_KERNEL_HPP_INCLUDED
#define LIX_EXEC_KERNEL_HPP_INCLUDED

#include <lix/exec/context.hpp>

namespace lix {

namespace exec {

context build_bootstrap_context();
context build_kernel_context();

} // namespace exec

} // namespace lix

#endif // LIX_EXEC_KERNEL_HPP_INCLUDED