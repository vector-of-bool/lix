#ifndef LIX_CODE_CODE_HPP_INCLUDED
#define LIX_CODE_CODE_HPP_INCLUDED

#include <array>
#include <memory>
#include <vector>
#include <ostream>

namespace lix::code {

class instr;

namespace detail {

struct code_impl;

}  // namespace detail

class code {
public:
    using iterator       = const instr*;
    using const_iterator = iterator;

private:
    std::shared_ptr<const detail::code_impl> _impl;

    void _prep_impl(std::vector<instr>&&);

public:
    ~code();
    iterator begin() const;
    iterator end() const;
    iterator cbegin() const { return begin(); }
    iterator cend() const { return end(); }

    template <typename Iter>
    code(Iter first, Iter last) {
        std::vector<instr> new_code(first, last);
        _prep_impl(std::move(new_code));
    }

    std::size_t size() const;
};

template <typename... Ts>
code make_code(Ts&&... ts) {
    std::array<instr, sizeof...(Ts)> arr({instr(std::forward<Ts>(ts))...});
    return code(arr.begin(), arr.end());
}

std::ostream& operator<<(std::ostream& o, const code&);

using iterator = code::iterator;

}  // namespace lix::code

#endif  // LIX_CODE_CODE_HPP_INCLUDED