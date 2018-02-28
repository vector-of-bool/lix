#ifndef LIX_STRING_VIEW_HPP_INCLUDED
#define LIX_STRING_VIEW_HPP_INCLUDED

#ifdef _MSC_VER
#include <string_view>
namespace lix {
using string_view = std::string_view;
}  // namespace lix
#elif __has_include(<string_view>)
#include <string_view>
namespace lix {
using string_view = std::string_view;
}  // namespace lix
#elif __has_include(<experimental/string_view>)
#include <experimental/string_view>
namespace lix {
using string_view = std::experimental::string_view;
}  // namespace lix
#else
#error No string_view available. Let requires this type to operate.
#endif

#endif // LIX_STRING_VIEW_HPP_INCLUDED