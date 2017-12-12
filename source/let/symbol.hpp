#ifndef LET_SYMBOL_HPP_INCLUDED
#define LET_SYMBOL_HPP_INCLUDED

#include <string>

namespace let {

class symbol {
    std::string _str;

public:
    explicit symbol(std::string str)
        : _str(std::move(str)) {}

    const std::string& string() const noexcept { return _str; }
};

}  // namespace let

#endif  // LET_SYMBOL_HPP_INCLUDED