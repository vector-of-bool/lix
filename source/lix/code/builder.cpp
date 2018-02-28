#include "builder.hpp"

#include <deque>

using namespace lix;
using namespace lix::code;

namespace lix::code::detail {

struct block_impl {};

struct code_impl {
    // We use deque for stable references
    std::deque<instr> bytecode;
};

}  // namespace lix::code::detail

instr& code_builder::_push_instr(instr&& i) {
    _code.push_back(std::move(i));
    return _code.back();
}