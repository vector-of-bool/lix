#include "value.hpp"

#include <sstream>

std::string let::to_string(const let::value& val) {
    std::stringstream strm;
    strm << val;
    return strm.str();
}