#ifndef LIX_CODE_BUILDER_HPP_INCLUDED
#define LIX_CODE_BUILDER_HPP_INCLUDED

#include <lix/code/code.hpp>
#include <lix/code/instr.hpp>

#include <deque>

namespace lix::code {

class code_builder {
    std::deque<instr> _code;

    instr& _push_instr(instr&&);

public:
    code_builder()                    = default;
    ~code_builder()                   = default;
    code_builder(const code_builder&) = delete;
    code_builder(code_builder&&)      = default;

    code save() {
        auto ret = code(_code.begin(), _code.end());
        _code.clear();
        return std::move(ret);
    }

    template <typename Instr>
    Instr& push_instr(Instr&& new_inst) {
        auto& ret = _push_instr(instr(std::move(new_inst)));
        return std::get<Instr>(ret.instr_var());
    }

    inst_offset_t current_offset() const noexcept { return {_code.size()}; }
};

}  // namespace lix::code

#endif  // LIX_CODE_BUILDER_HPP_INCLUDED