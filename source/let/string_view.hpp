#ifndef LET_STRING_VIEW_HPP_INCLUDED
#define LET_STRING_VIEW_HPP_INCLUDED

#ifdef _MSC_VER
#include <string_view>
namespace let {
using string_view = std::string_view;
}  // namespace let
#elif __has_include(<string_view>)
#include <string_view>
namespace let {
using string_view = std::string_view;
}  // namespace let
#elif __has_include(<experimental/string_view>)
#include <experimental/string_view>
namespace let {
using string_view = std::experimental::string_view;
}  // namespace let
#else
#error No string_view available. Let requires this type to operate.
#endif

#endif // LET_STRING_VIEW_HPP_INCLUDED