#ifndef LIX_VARIANT_HPP_INCLUDED
#define LIX_VARIANT_HPP_INCLUDED

#ifdef _MSC_VER
#include <variant>
namespace lix {
using std::variant;
}  // namespace lix
#elif __has_include(<variant>)
#include <variant>
namespace lix {
using std::variant;
}  // namespace lix
#elif __has_include(<experimental/variant>)
#include <experimental/variant>
namespace lix {
using std::experimental::variant;
}  // namespace lix
#else
#error No variant header. We require this type to operate.
#endif

#endif  // LIX_VARIANT_HPP_INCLUDED