#ifndef LET_VARIANT_HPP_INCLUDED
#define LET_VARIANT_HPP_INCLUDED

#ifdef _MSC_VER
#include <variant>
namespace let {
using std::variant;
}  // namespace let
#elif __has_include(<variant>)
#include <variant>
namespace let {
using std::variant;
}  // namespace let
#elif __has_include(<experimental/variant>)
#include <experimental/variant>
namespace let {
using std::experimental::variant;
}  // namespace let
#else
#error No variant header. We require this type to operate.
#endif

#endif  // LET_VARIANT_HPP_INCLUDED