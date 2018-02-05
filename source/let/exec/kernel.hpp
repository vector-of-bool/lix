#ifndef LET_EXEC_KERNEL_HPP_INCLUDED
#define LET_EXEC_KERNEL_HPP_INCLUDED

#include <let/exec/context.hpp>

namespace let {

namespace exec {

context build_bootstrap_context();
context build_kernel_context();

} // namespace exec

} // namespace let

#endif // LET_EXEC_KERNEL_HPP_INCLUDED