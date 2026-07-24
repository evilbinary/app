#include "font.h"

#include "debug.h"
#include "split_by_char.h"
#include "surface.h"
#include "utilities.h"

#include <SDL.h>
#include <SDL_ttf.h>

#include <algorithm>
#include <cassert>
#include <vector>

Font::Font(Font &&other) noexcept
    : font(other.font),
      lineSpacing(other.lineSpacing),
      spec_(std::move(other.spec_)) {
	other.font = nullptr;
}

Font &Font::operator=(Font &&other) noexcept {
	if (font) {
		TTF_CloseFont(font);
		TTF_Quit();
	}
	font = other.font;
	other.font = nullptr;
	lineSpacing = other.lineSpacing;
	spec_ = std::move(other.spec_);
	return *this;
}

bool Font::Load(FontSpec spec)
{
	spec_ = std::move(spec);

	/* Note: TTF_Init and TTF_Quit perform reference counting, so call them
	 * both unconditionally for each font. */
	if (TTF_Init() < 0) {
		ERROR("Unable to init SDL_ttf library\n");
		return false;
	}

	font = TTF_OpenFont(spec_.path.c_str(), spec_.size);
	if (font) {
		INFO("Loaded font '%s'\n", spec_.path.c_str());
	} else {
		WARNING("Unable to open font '%s'\n", spec_.path.c_str());
		SDL_ClearError();
		return false;
	}

	lineSpacing = TTF_FontLineSkip(font);
	return true;
}

Font::~Font()
{
	if (font) {
		TTF_CloseFont(font);
		TTF_Quit();
	}
}

int Font::writeLine(Surface& surface, const std::uint16_t *text, int x, int y,
                    HAlign halign, VAlign valign) const {
	if (*text == 0) {
		// SDL_ttf will return a nullptr when rendering the empty string.
		return 0;
	}

	switch (valign) {
	case VAlignTop:
		break;
	case VAlignMiddle:
		y -= lineSpacing / 2;
		break;
	case VAlignBottom:
		y -= lineSpacing;
		break;
	}

	SDL_Color color = { 0, 0, 0, 0 };
	SDL_Surface *s = TTF_RenderUNICODE_Blended(font, text, color);
	if (!s) {
		ERROR("Font rendering failed: %s\n", SDL_GetError());
		SDL_ClearError();
		return 0;
	}
	const int width = s->w;

	switch (halign) {
	case HAlignLeft:
		break;
	case HAlignCenter:
		x -= width / 2;
		break;
	case HAlignRight:
		x -= width;
		break;
	}

	SDL_Rect rect = { (Sint16) x, (Sint16) (y - 1), 0, 0 };
	SDL_BlitSurface(s, NULL, surface.raw, &rect);

	/* Note: rect.x / rect.y are reset everytime because SDL_BlitSurface
	 * will modify them if negative */
	rect.x = x;
	rect.y = y + 1;
	SDL_BlitSurface(s, NULL, surface.raw, &rect);

	rect.x = x - 1;
	rect.y = y;
	SDL_BlitSurface(s, NULL, surface.raw, &rect);

	rect.x = x + 1;
	rect.y = y;
	SDL_BlitSurface(s, NULL, surface.raw, &rect);
	SDL_FreeSurface(s);

	rect.x = x;
	rect.y = y;
	color.r = 0xff;
	color.g = 0xff;
	color.b = 0xff;

	s = TTF_RenderUNICODE_Blended(font, text, color);
	if (!s) {
		ERROR("Font rendering failed: %s\n", SDL_GetError());
		SDL_ClearError();
		return width;
	}
	SDL_BlitSurface(s, NULL, surface.raw, &rect);
	SDL_FreeSurface(s);

	return width;
}
