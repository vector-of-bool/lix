#ifndef LET_RAISE_HPP_INCLUDED
#define LET_RAISE_HPP_INCLUDED

#include <let/value.hpp>

#include <stdexcept>
#include <string>

namespace let {

class raised_exception : public std::exception {
    let::value  _value;
    std::string _message;

public:
    explicit raised_exception(let::value v);

    const char*       what() const noexcept override { return _message.data(); }
    const let::value& value() const noexcept { return _value; }
};

[[noreturn]] void raise(let::value v);

}  // namespace let

#endif  // LET_RAISE_HPP_INCLUDED