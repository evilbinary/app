#ifndef __COMPAT_FILESYSTEM__
#define __COMPAT_FILESYSTEM__

#ifdef __has_include
#if defined(__cplusplus) && __cplusplus >= 201703L && __has_include(<filesystem>)
#define HAVE_STD_FILESYSTEM
#endif
#endif

#ifdef HAVE_STD_FILESYSTEM
#include <filesystem>
namespace compat {
namespace filesystem = ::std::filesystem;
}
#else
#include <experimental/filesystem>
namespace compat {
namespace filesystem = ::std::experimental::filesystem;
}
#endif

#endif  // __COMPAT_FILESYSTEM__
