#ifndef __COMPAT_STRING_VIEW__
#define __COMPAT_STRING_VIEW__

#ifdef __has_include
#if defined(__cplusplus) && __cplusplus >= 201703L && __has_include(<string_view>)
#define HAVE_STD_STRING_VIEW
#endif
#endif

#ifdef HAVE_STD_STRING_VIEW
#include <string_view>
namespace compat {
using string_view = ::std::string_view;
}
#else
#include <experimental/string_view>
namespace compat {
using string_view = ::std::experimental::string_view;
}
#endif

#endif  // __COMPAT_STRING_VIEW__
