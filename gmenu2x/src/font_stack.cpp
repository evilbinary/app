#include "font_stack.h"

#include <cassert>
#include <cstddef>
#include <iterator>
#include <type_traits>
#include <unordered_map>

#include "debug.h"
#include "split_by_char.h"
#include "surface.h"

namespace {

// Decodes UTF-8 into a 0-terminated vector of code points.
// Only supports BMP as that's what SDL_ttf supports.
std::vector<std::uint16_t> DecodeUtf8(compat::string_view utf8) {
	std::vector<std::uint16_t> result;
	for (std::size_t i = 0; i < utf8.size(); ++i) {
		std::uint16_t ch = static_cast<unsigned char>(utf8[i]);
		if (ch >= 0xF0) {
			ch = static_cast<std::uint16_t>(utf8[i] & 0x07) << 18;
			ch |= static_cast<std::uint16_t>(utf8[++i] & 0x3F) << 12;
			ch |= static_cast<std::uint16_t>(utf8[++i] & 0x3F) << 6;
			ch |= static_cast<std::uint16_t>(utf8[++i] & 0x3F);
		} else if (ch >= 0xE0) {
			ch = static_cast<std::uint16_t>(utf8[i] & 0x0F) << 12;
			ch |= static_cast<std::uint16_t>(utf8[++i] & 0x3F) << 6;
			ch |= static_cast<std::uint16_t>(utf8[++i] & 0x3F);
		} else if (ch >= 0xC0) {
			ch = static_cast<std::uint16_t>(utf8[i] & 0x1F) << 6;
			ch |= static_cast<std::uint16_t>(utf8[++i] & 0x3F);
		}
		result.push_back(ch);
	}
	result.push_back(0);
	return result;
}

bool FontSpecsEq(const std::vector<Font> &fonts,
                 const std::vector<FontSpec> &specs) {
	if (fonts.size() != specs.size()) return false;
	for (std::size_t i = 0; i < fonts.size(); ++i)
		if (fonts[i].spec() != specs[i]) return false;
	return true;
}

template <std::size_t N>
void BuildCodePointToFontMap(const std::vector<Font> &fonts,
                             std::array<const Font *, N> *map) {
	for (std::size_t cp = 0; cp < N; ++cp) {
		(*map)[cp] = &fonts[0];
		for (const auto &font : fonts) {
			if (!font.HasGlyph(cp)) continue;
			(*map)[cp] = &font;
			break;
		}
	}
}

std::uint8_t *get_pixel8(const SDL_Surface *s, int row, int col) {
	const std::uintptr_t row_addr =
	    reinterpret_cast<std::uintptr_t>(s->pixels) + row * s->pitch;

	assert(row < s->h);
	assert(col < s->w);

	return reinterpret_cast<std::uint8_t *>(row_addr) + col;
}

std::uint32_t *get_pixel32(const SDL_Surface *s, int row, int col) {
	const std::uintptr_t row_addr =
	    reinterpret_cast<std::uintptr_t>(s->pixels) + row * s->pitch;

	assert(row < s->h);
	assert(col < s->w);

	return reinterpret_cast<std::uint32_t *>(row_addr) + col;
}

SDL_Surface *drawOutline(const SDL_Surface *s) {
	SDL_Surface *raw =
	    SDL_CreateRGBSurface(SDL_SWSURFACE , s->w + 2, s->h + 2, 32,
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	                         0xff << 8, 0xff << 16, 0xff << 24, 0xff
#else
	                         0xff << 16, 0xff << 8, 0xff, 0xff << 24
#endif
	    );

	for (unsigned int row = 0; row < raw->h; row++) {
		for (unsigned int col = 0; col < raw->w; col++) {
			std::uint8_t center_a, north_a, south_a, east_a, west_a, outline_a;

			if (row > 0 && col > 0 && row <= s->h && col <= s->w)
				center_a = *get_pixel8(s, row - 1, col - 1);
			else
				center_a = 0;

			outline_a = center_a;

			if (row >= 2 && col >= 1 && col <= s->w) {
				north_a = *get_pixel8(s, row - 2, col - 1);
				outline_a = std::max(outline_a, north_a);
			}

			if (row <= s->h - 1 && col >= 1 && col <= s->w) {
				south_a = *get_pixel8(s, row, col - 1);
				outline_a = std::max(outline_a, south_a);
			}

			if (row >= 1 && row <= s->h && col <= s->w - 1) {
				east_a = *get_pixel8(s, row - 1, col);
				outline_a = std::max(outline_a, east_a);
			}

			if (row >= 1 && row <= s->h && col >= 2) {
				west_a = *get_pixel8(s, row - 1, col - 2);
				outline_a = std::max(outline_a, west_a);
			}

			*get_pixel32(raw, row, col) = (center_a << 16) | (center_a << 8) |
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
			                              (center_a << 24) | outline_a;
#else
			                              (outline_a << 24) | center_a;
#endif
		}
	}

	return raw;
}

}  // namespace

bool FontStack::LoadFonts(std::initializer_list<FontSpec> specs) {
	// Build a map of fonts that we currently have.
	std::unordered_map<FontSpec, Font *> existing_fonts;
	existing_fonts.reserve(fonts_.size());
	for (auto &font : fonts_) existing_fonts[font.spec()] = &font;

	// Load new fonts and build a map of the ones loaded successfully.
	// Also build a list of all successfully loaded font specs.
	std::unordered_map<FontSpec, Font> new_fonts;
	std::vector<FontSpec> loaded_specs;
	new_fonts.reserve(specs.size());
	loaded_specs.reserve(specs.size());
	for (const auto &font_spec : specs) {
		if (existing_fonts.find(font_spec) != existing_fonts.end()) {
			loaded_specs.push_back(font_spec);
			continue;
		}
		Font new_font;
		if (!new_font.Load(font_spec)) continue;
		new_fonts[font_spec] = std::move(new_font);
		loaded_specs.push_back(font_spec);
	}

	if (FontSpecsEq(fonts_, loaded_specs)) return false;

	// Replace the fonts with new fonts.
	std::vector<Font> fonts;
	fonts.reserve(loaded_specs.size());
	line_spacing_ = 0;
	for (const auto &font_spec : loaded_specs) {
		const auto existing_font_it = existing_fonts.find(font_spec);
		if (existing_font_it != existing_fonts.end())
			fonts.push_back(std::move(*existing_font_it->second));
		else
			fonts.push_back(std::move(new_fonts.at(font_spec)));
		line_spacing_ = std::max(line_spacing_, fonts.back().getLineSpacing());
	}
	fonts_ = std::move(fonts);

	BuildCodePointToFontMap(fonts_, &code_point_to_font_);

	return true;
}

void FontStack::ForEachSlice(
    const std::vector<std::uint16_t> &code_points,
    std::function<void(const FontStack::Slice &slice)> fn) const {
	if (code_points[0] == 0) return;
	if (fonts_.size() == 1) {
		fn(Slice{code_points.data(), code_points.size() - 1, &fonts_[0]});
		return;
	}
	const Font *prev_font = code_point_to_font_[code_points[0]];
	Slice cur_slice{code_points.data(), 1, prev_font};
	for (std::size_t i = 1; i < code_points.size(); ++i) {
		auto &cp = code_points[i];
		if (cp == 0) break;
		const Font *cur_font = code_point_to_font_[cp];
		if (cur_font == prev_font) {
			++cur_slice.text_size;
		} else {
			fn(cur_slice);
			cur_slice = Slice{&cp, 1, cur_font};
			prev_font = cur_font;
		}
	}
	fn(cur_slice);
}

void FontStack::ForEachSliceZeroTerminated(
    std::vector<std::uint16_t> &code_points,
    std::function<void(const Slice &slice)> fn) const {
	ForEachSlice(code_points, [&fn](const Slice &slice) {
		// Safe cast, slice.text points to an offset of `code_points`.
		auto &end = const_cast<std::uint16_t *>(slice.text)[slice.text_size];
		const std::uint16_t cp = end;
		end = 0;
		fn(slice);
		end = cp;
	});
}
void FontStack::ForEachSlice(
    compat::string_view text,
    std::function<void(const FontStack::Slice &slice)> fn) const {
	auto code_points = DecodeUtf8(text);
	ForEachSlice(code_points, std::move(fn));
}

void FontStack::ForEachSliceZeroTerminated(
    compat::string_view text,
    std::function<void(const FontStack::Slice &slice)> fn) const {
	auto code_points = DecodeUtf8(text);
	ForEachSliceZeroTerminated(code_points, std::move(fn));
}

int FontStack::getTextWidth(compat::string_view text) const {
	std::size_t start = 0;
	int max_width = 0;
	for (compat::string_view line : SplitByChar(text, '\n')) {
		ForEachSliceZeroTerminated(line, [&max_width](const Slice &slice) {
			int w;
			TTF_SizeUNICODE(slice.font->font, slice.text, &w, nullptr);
			max_width = std::max(max_width, w);
		});
	}
	return max_width;
}

int FontStack::getTextHeight(compat::string_view text) const {
	std::size_t start = 0;
	int height = 0;
	for (compat::string_view line : SplitByChar(text, '\n')) {
		int line_spacing = fonts_[0].getLineSpacing();
		ForEachSlice(line, [&line_spacing](const Slice &slice) {
			line_spacing = std::max(line_spacing, slice.font->getLineSpacing());
		});
		height += line_spacing;
	}
	return height;
}

int FontStack::write(Surface &surface, compat::string_view text, int x, int y,
                     Font::HAlign halign, Font::VAlign valign) const {
	int max_width = 0;
	for (compat::string_view line : SplitByChar(text, '\n')) {
		int line_spacing = line.empty() ? fonts_[0].getLineSpacing() : 0;
		int line_width = 0;
		ForEachSliceZeroTerminated(line, [&](const Slice &slice) {
			line_width += slice.font->writeLine(surface, slice.text, x + line_width,
			                                    y, halign, valign);
			line_spacing = std::max(line_spacing, slice.font->getLineSpacing());
		});
		max_width = std::max(max_width, line_width);
		y += line_spacing;
	}
	return max_width;
}

std::unique_ptr<OffscreenSurface> FontStack::render(
    compat::string_view text) const {
	std::vector<SDL_Surface *> surfaces;
	int width = 0, height = 0;
	ForEachSliceZeroTerminated(text, [&](const Slice &slice) {
		SDL_Surface *s = TTF_RenderUNICODE_Shaded(slice.font->font, slice.text,
		                                          SDL_Color{}, SDL_Color{});
		if (s == nullptr) {
			ERROR("TTF_RenderUNICODE_Shaded: %s\n", SDL_GetError());
			SDL_ClearError();
			return;
		}
		surfaces.push_back(s);
		width += s->w;
		height = std::max(height, s->h);
	});
	if (surfaces.empty()) return std::unique_ptr<OffscreenSurface>();

	SDL_Surface *concatenated;
	if (surfaces.size() == 1) {
		concatenated = surfaces[0];
	} else {
		const SDL_PixelFormat *fmt = surfaces[0]->format;
		concatenated =
		    SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, fmt->BitsPerPixel,
		                         fmt->Rmask, fmt->Gmask, fmt->Bmask, fmt->Amask);
		int x = 0;
		for (SDL_Surface *src : surfaces) {
			SDL_Rect dest_rect = {
			    static_cast<decltype(SDL_Rect().x)>(x),
			    static_cast<decltype(SDL_Rect().y)>(height - src->h), 0, 0};
			SDL_BlitSurface(src, nullptr, concatenated, &dest_rect);
			x += src->w;
			SDL_FreeSurface(src);
		}
	}

	SDL_Surface *result = drawOutline(concatenated);
	SDL_FreeSurface(concatenated);
	return std::unique_ptr<OffscreenSurface>(new OffscreenSurface(result));
}
