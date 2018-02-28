#include "fn.hpp"

#include <lix/exec/stack.hpp>

using namespace lix;
using namespace lix::exec;

lix::value function::call_ll(context& ctx, const lix::value& val) const {
    return _func->call(ctx, val);
}
