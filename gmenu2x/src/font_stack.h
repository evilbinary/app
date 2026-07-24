#ifndef _FONT_STACK_H_
#define _FONT_STACK_H_

#include <array>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <limits>
#include <vector>

#include "compat-string_view.h"
#include "font.h"
#include "font_spec.h"

class OffscreenSurface;

class FontStack {
 public:
	// Returns true if any of the fonts have changed.
	bool LoadFonts(std::initializer_list<FontSpec> specs);

	int getTextWidth(compat::string_view text) const;
	int getTextHeight(compat::string_view text) const;
	int getLineSpacing() const { return line_spacing_; }

	int write(Surface &surface, compat::string_view text, int x, int y,
	          Font::HAlign halign = Font::HAlignLeft,
	          Font::VAlign valign = Font::VAlignTop) const;

	std::unique_ptr<OffscreenSurface> render(compat::string_view text) const;

 private:
	struct Slice {
		const std::uint16_t *text;
		std::size_t text_size;
		const Font *font;
	};

	// Calls the given function for each span of same-font code points.
	void ForEachSlice(const std::vector<std::uint16_t> &code_points,
	                  std::function<void(const Slice &slice)> fn) const;

	// Same as above but accepts a string_view. Slices must not be captured.
	void ForEachSlice(compat::string_view text,
	                  std::function<void(const Slice &slice)> fn) const;

	// Same as `ForEachSlice` but `slice.text[slice.text_size]` is guaranteed to
	// be 0.
	void ForEachSliceZeroTerminated(
	    std::vector<std::uint16_t> &code_points,
	    std::function<void(const Slice &slice)> fn) const;

	// Same as above but accepts a string_view. Slices must not be captured.
	void ForEachSliceZeroTerminated(
	    compat::string_view text,
	    std::function<void(const Slice &slice)> fn) const;

	// Fonts in the order of priority. Lower index means higher priority.
	std::vector<Font> fonts_;

	// A map from code point to the font that contains it.
	// If no font contains a given code point, maps to the first font.
	//
	// Only covers BMP because SDL 1 does not support anything else.
	std::array<const Font *, std::numeric_limits<std::uint16_t>::max() + 1>
	    code_point_to_font_;

	// The maximum of line spacings of all fonts.
	int line_spacing_;
};

#endif  //_FONT_STACK_H_
