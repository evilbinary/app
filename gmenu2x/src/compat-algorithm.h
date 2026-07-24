#ifndef __COMPAT_ALGORITHM__
#define __COMPAT_ALGORITHM__

#include <algorithm>

namespace compat {

#if __cplusplus < 201703L
template <class T>
constexpr const T& clamp(const T& v, const T& lo, const T& hi) {
  return (v < lo) ? lo : (hi < v) ? hi : v;
}

template <class T, class Compare>
constexpr const T& clamp(const T& v, const T& lo, const T& hi, Compare comp) {
  return comp(v, lo) ? lo : comp(hi, v) ? hi : v;
}
#else
template <class T>
constexpr const T& clamp(const T& v, const T& lo, const T& hi) {
  return std::clamp(v, lo, hi);
}

template <class T, class Compare>
constexpr const T& clamp(const T& v, const T& lo, const T& hi, Compare comp) {
  return std::clamp(v, lo, hi, comp);
}
#endif

}  // namespace compat

#endif  // __COMPAT_ALGORITHM__
