#ifndef LET_RAISE_HPP_INCLUDED
#define LET_RAISE_HPP_INCLUDED

#include <let/value.hpp>

#include <stdexcept>
#include <string>

namespace let {

class raised_exception : public std::exception {
    let::value               _value;
    std::string              _message;
    std::vector<std::string> _traceback;

public:
    explicit raised_exception(let::value v, std::vector<std::string> traceback = {});

    const char*       what() const noexcept override { return _message.data(); }
    const let::value& value() const noexcept { return _value; }
    auto&             traceback() const noexcept { return _traceback; }
};

[[noreturn]] void raise(let::value v, std::vector<std::string> traceback = {});

}  // namespace let

#endif  // LET_RAISE_HPP_INCLUDED