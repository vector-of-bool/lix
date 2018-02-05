#include "fn.hpp"

#include <let/exec/stack.hpp>

using namespace let;
using namespace let::exec;

let::value function::call_ll(context& ctx, const let::value& val) const {
    return _func->call(ctx, val);
}
