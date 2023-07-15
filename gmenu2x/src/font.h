// Original font class was replaced by an SDL_ttf based one by Paul Cercueil.
// License: GPL version 2 or later.

#ifndef FONT_H
#define FONT_H

#include <cstdint>
#include <memory>
#include <string>

#include <SDL_ttf.h>

#include "font_spec.h"

class FontStack;
class OffscreenSurface;
class Surface;

/**
 * Wrapper around a TrueType or other FreeType-supported font.
 * The wrapper is valid even if the font couldn't be loaded, but in that case
 * nothing will be drawn.
 */
class Font {
public:
	enum HAlign { HAlignLeft, HAlignRight,  HAlignCenter };
	enum VAlign { VAlignTop,  VAlignBottom, VAlignMiddle };

	Font() = default;

	// Returns `true` on success.
	bool Load(FontSpec spec);

	// Moveable but not copyable.
	Font(const Font &other) = delete;
	Font(Font &&other) noexcept;
	Font& operator=(const Font& other) = delete;
	Font& operator=(Font&& other) noexcept;

	~Font();

	int getLineSpacing() const
	{
		return lineSpacing;
	}

	bool HasGlyph(std::uint16_t code_point) const {
		return TTF_GlyphIsProvided(font, code_point);
	}

	const FontSpec& spec() const { return spec_; }

private:
	Font(TTF_Font *font);

	/**
	 * Draws a single line of text on a surface in this font.
	 * @return The width of the text in pixels.
	 */
	int writeLine(Surface& surface, const std::uint16_t *text, int x, int y,
	              HAlign halign, VAlign valign) const;

	TTF_Font *font;
	int lineSpacing;
	FontSpec spec_;

	friend class FontStack;
};

#endif /* FONT_H */
