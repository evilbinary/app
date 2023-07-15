#ifndef _FONT_SPEC_H_
#define _FONT_SPEC_H_

#include <cstddef>
#include <string>

struct FontSpec {
	std::string path;
	unsigned int size;
	bool operator==(const FontSpec &other) const {
		return other.path == path && other.size == size;
	}
	bool operator!=(const FontSpec &other) const { return !(*this == other); }
};

namespace std {

template <>
struct hash<FontSpec> {
	size_t operator()(const FontSpec &spec) const {
		return hash<string>()(spec.path) ^ hash<unsigned int>()(spec.size);
	}
};

}  // namespace std

#endif  // _FONT_SPEC_H_
