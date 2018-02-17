#include "raise.hpp"


let::raised_exception::raised_exception(let::value v)
    : _value(std::move(v)) {
    auto val_str = to_string(value());
    _message     = "Raised value: " + val_str;
}

void let::raise(let::value v) { throw raised_exception(std::move(v)); }