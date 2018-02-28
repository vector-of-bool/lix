#include "code.hpp"

#include <lix/code/instr.hpp>

#include <iomanip>
#include <vector>

using lix::code::code;

namespace lix::code::detail {

struct code_impl {
    std::vector<instr> is;
    code_impl(std::vector<instr>&& is_)
        : is(std::move(is_)) {}
};

}  // namespace lix::code::detail

code::~code() = default;

void code::_prep_impl(std::vector<instr>&& is) {
    _impl = std::make_shared<detail::code_impl>(std::move(is));
}

using code_iter = code::iterator;
code_iter   code::begin() const { return _impl->is.data(); }
code_iter   code::end() const { return begin() + _impl->is.size(); }
std::size_t code::size() const { return static_cast<std::size_t>(std::distance(begin(), end())); }

std::ostream& lix::code::operator<<(std::ostream& o, const code& c) {
    int counter = 0;
    o << std::setfill(' ');
    for (auto& inst : c) {
        o << "%" << std::left << std::setw(3) << counter++ << std::right << " ";
        o << inst;
        o << '\n';
    }
    return o;
}
