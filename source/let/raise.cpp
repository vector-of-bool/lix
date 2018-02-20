#include "raise.hpp"

let::raised_exception::raised_exception(let::value v, std::vector<std::string> traceback)
    : _value(std::move(v))
    , _traceback(std::move(traceback)) {
    auto val_str = to_string(value());
    _message     = "Raised value: " + val_str;
}

void let::raise(let::value v, std::vector<std::string> traceback) {
    throw raised_exception(std::move(v), std::move(traceback));
}