#ifndef LIX_RAISE_HPP_INCLUDED
#define LIX_RAISE_HPP_INCLUDED

#include <lix/value.hpp>

#include <stdexcept>
#include <string>

namespace lix {

class raised_exception : public std::exception {
    lix::value               _value;
    std::string              _message;
    std::vector<std::string> _traceback;

public:
    explicit raised_exception(lix::value v, std::vector<std::string> traceback = {});

    const char*       what() const noexcept override { return _message.data(); }
    const lix::value& value() const noexcept { return _value; }
    auto&             traceback() const noexcept { return _traceback; }
};

[[noreturn]] void raise(lix::value v, std::vector<std::string> traceback = {});

}  // namespace lix

#endif  // LIX_RAISE_HPP_INCLUDED