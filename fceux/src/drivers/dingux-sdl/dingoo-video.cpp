/* FCE Ultra - NES/Famicom Emulator
 *
 * Copyright notice for this file:
 *  Copyright (C) 2002 Xodnizel  
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/// \file
/// \brief Handles the graphical game display for the SDL implementation.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <SDL/SDL_image.h>

#include "dingoo.h"
#include "dingoo-video.h"
#include "scaler.h"
#include "menu.h"
#include "configfile_fk.h"

#include "../common/vidblit.h"
#include "../../fceu.h"
#include "../../version.h"

#include "dface.h"

#include "../common/configSys.h"

// GLOBALS
SDL_Surface *screen;
SDL_Surface *hw_screen;
SDL_Surface *nes_screen; // 256x224

extern Config *g_config;

// STATIC GLOBALS
static int s_curbpp;
static int s_srendline, s_erendline;
static int s_tlines;
static int s_inited;
static bool s_VideoModeSet = false;

static int s_clipSides;
int s_fullscreen;
static int noframe;

static int FDSTimer = 0;
int FDSSwitchRequested = 0;

#define NWIDTH	(256 - (s_clipSides ? 16 : 0))
#define NOFFSET	(s_clipSides ? 8 : 0)

/* Blur effect taken from vidblit.cpp */
uint32 palettetranslate[65536 * 4];
static uint32 CBM[3] = { 63488, 2016, 31 };
static uint16 s_psdl[256];

struct Color {
	uint8 r;
	uint8 g;
	uint8 b;
	uint8 unused;
};

static struct Color s_cpsdl[256];

#define BLUR_RED	30
#define BLUR_GREEN	30
#define BLUR_BLUE	20

#ifdef SDL_TRIPLEBUF
#  define DINGOO_MULTIBUF SDL_TRIPLEBUF
#else
#  define DINGOO_MULTIBUF SDL_DOUBLEBUF
#endif

/**
 * Attempts to destroy the graphical video display.  Returns 0 on
 * success, -1 on failure.
 */

//draw input aids if we are fullscreen
bool FCEUD_ShouldDrawInputAids() {
	return s_fullscreen != 0;
}

int KillVideo() {
	// return failure if the video system was not initialized
	if (s_inited == 0)
		return -1;

	deinit_menu_SDL();

	TTF_Quit();

	SDL_FreeSurface(nes_screen);
	s_inited = 0;
	return 0;
}

/**
 * These functions determine an appropriate scale factor for fullscreen/
 */
inline double GetXScale(int xres) {
	return ((double) xres) / NWIDTH;
}
inline double GetYScale(int yres) {
	return ((double) yres) / s_tlines;
}
void FCEUD_VideoChanged() {
	int buf;
	g_config->getOption("SDL.PAL", &buf);
	if (buf)
		PAL = 1;
	else
		PAL = 0;
}
/**
 * Attempts to initialize the graphical video display.  Returns 0 on
 * success, -1 on failure.
 */
int InitVideo(FCEUGI *gi) {
	FCEUI_printf("Initializing video...\n");

	// load the relevant configuration variables
	g_config->getOption("SDL.Fullscreen", &s_fullscreen);
	g_config->getOption("SDL.ClipSides", &s_clipSides);

	// check the starting, ending, and total scan lines
	FCEUI_GetCurrentVidSystem(&s_srendline, &s_erendline);
	s_tlines = s_erendline - s_srendline + 1;

	int brightness;
	g_config->getOption("SDL.Brightness", &brightness);

	s_inited = 1;
	FDSSwitchRequested = 0;

	//int desbpp;
	//g_config->getOption("SDL.SpecialFilter", &s_eefx);

	SetPaletteBlitToHigh((uint8 *) s_cpsdl);

	//Init video subsystem
	if (!(SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO))
		if (SDL_InitSubSystem(SDL_INIT_VIDEO) == -1) {
			fprintf(stderr,"%s",SDL_GetError());
		}

	// initialize dingoo video mode
	if (!s_VideoModeSet) {
		uint32 vm = 0; // 0 - 320x240, 1 - 400x240, 2 - 480x272

		hw_screen = SDL_SetVideoMode(RES_HW_SCREEN_HORIZONTAL, RES_HW_SCREEN_VERTICAL, 16, SDL_HWSURFACE | DINGOO_MULTIBUF);
		screen = SDL_CreateRGBSurface(SDL_SWSURFACE, 320, 240, 16, 0, 0, 0, 0);
		s_VideoModeSet = true;
	}

	if (TTF_Init()) {
		fprintf(stderr, "Error TTF_Init: %s\n", TTF_GetError());
		exit(EXIT_FAILURE);
	}

	// a hack to bind inner buffer to nes_screen surface
	extern uint8 *XBuf;

	nes_screen = SDL_CreateRGBSurfaceFrom(XBuf, 256, 224, 8, 256, 0, 0, 0, 0);
	if(!nes_screen)
		printf("Error in SDL_CreateRGBSurfaceFrom\n");
	SDL_SetPalette(nes_screen, SDL_LOGPAL, (SDL_Color *)s_cpsdl, 0, 256);

	SDL_ShowCursor(0);

	/* clear screen */
	dingoo_clear_video();


	init_menu_SDL();

	return 0;
}

/**
 * Toggles the full-screen display.
 */
void ToggleFS() {
	dingoo_clear_video();
}

/* Taken from /src/drivers/common/vidblit.cpp */
static void CalculateShift(uint32 *CBM, int *cshiftr, int *cshiftl)
{
	int a, x, z, y;
	cshiftl[0] = cshiftl[1] = cshiftl[2] = -1;
	for (a = 0; a < 3; a++) {
		for (x = 0, y = -1, z = 0; x < 32; x++) {
			if (CBM[a] & (1 << x)) {
				if (cshiftl[a] == -1)
					cshiftl[a] = x;
				z++;
			}
		}
		cshiftr[a] = (8 - z);
	}
}

void SetPaletteBlitToHigh(uint8 *src)
{
	int cshiftr[3];
	int cshiftl[3];
	int x, y;

	CalculateShift(CBM, cshiftr, cshiftl);

	for (x = 0; x < 65536; x++) {
		uint16 lower, upper;

		lower = (src[((x & 255) << 2)] >> cshiftr[0]) << cshiftl[0];
		lower |= (src[((x & 255) << 2) + 1] >> cshiftr[1]) << cshiftl[1];
		lower |= (src[((x & 255) << 2) + 2] >> cshiftr[2]) << cshiftl[2];
		upper = (src[((x >> 8) << 2)] >> cshiftr[0]) << cshiftl[0];
		upper |= (src[((x >> 8) << 2) + 1] >> cshiftr[1]) << cshiftl[1];
		upper |= (src[((x >> 8) << 2) + 2] >> cshiftr[2]) << cshiftl[2];

		palettetranslate[x] = lower | (upper << 16);
	}
}

/**
 * Sets the color for a particular index in the palette.
 */
void FCEUD_SetPalette(uint8 index, uint8 r, uint8 g, uint8 b)
{
	s_cpsdl[index].r = r;
	s_cpsdl[index].g = g;
	s_cpsdl[index].b = b;

	//uint32 col = (r << 16) | (g << 8) | b;
	//s_psdl[index] = (uint16)COL32_TO_16(col);
	s_psdl[index] = dingoo_video_color15(r, g, b);

	if (index == 255)
		SetPaletteBlitToHigh((uint8 *) s_cpsdl);
}

/**
 * Gets the color for a particular index in the palette.
 */
void FCEUD_GetPalette(uint8 index, uint8 *r, uint8 *g, uint8 *b)
{
	*r = s_cpsdl[index].r;
	*g = s_cpsdl[index].g;
	*b = s_cpsdl[index].b;
}

uint16 * FCEUD_GetPaletteArray16()
{
	return s_psdl;
}

/** 
 * Pushes the palette structure into the underlying video subsystem.
 */
static void RedoPalette() {
}

// XXX soules - console lock/unlock unimplemented?

///Currently unimplemented.
void LockConsole() {
}

///Currently unimplemented.
void UnlockConsole() {
}

#define READU16(x)  (uint16) ((uint16)(x)[0] | (uint16)(x)[1] << 8) 

/// Nearest neighboor optimized with possible out of screen coordinates (for cropping)
void flip_NNOptimized_AllowOutOfScreen(SDL_Surface *src_surface, SDL_Surface *dst_surface, int new_w, int new_h) {
	int w1 = src_surface->w;
	//int h1 = src_surface->h;
	int w2 = new_w;
	int h2 = new_h;
	int x_ratio = (int) ((src_surface->w << 16) / w2);
	int y_ratio = (int) ((src_surface->h << 16) / h2);
	int x2, y2;

	/// --- Compute padding for centering when out of bounds ---
	int y_padding = (RES_HW_SCREEN_VERTICAL - new_h) / 2;
	int x_padding = 0;
	if (w2 > RES_HW_SCREEN_HORIZONTAL) {
		x_padding = (w2 - RES_HW_SCREEN_HORIZONTAL) / 2 + 1;
	}
	int x_padding_ratio = x_padding * w1 / w2;
	//printf("src_surface->h=%d, h2=%d\n", src_surface->h, h2);

	for (int i = 0; i < h2; i++) {
		if (i >= RES_HW_SCREEN_VERTICAL) {
			continue;
		}

		uint16_t *t = static_cast<uint16_t*>(dst_surface->pixels) + ((i + y_padding) * ((w2 > RES_HW_SCREEN_HORIZONTAL) ? RES_HW_SCREEN_HORIZONTAL : w2));
		y2 = (i * y_ratio) >> 16;
		uint16_t *p = static_cast<uint16_t*>(src_surface->pixels) + (y2 * w1 + x_padding_ratio);
		int rat = 0;
		for (int j = 0; j < w2; j++) {
			if (j >= RES_HW_SCREEN_HORIZONTAL) {
				continue;
			}
			x2 = rat >> 16;
			*t++ = p[x2];
			rat += x_ratio;
			//printf("y=%d, x=%d, y2=%d, x2=%d, (y2*src_surface->w)+x2=%d\n", i, j, y2, x2, (y2 * src_surface->w) + x2);
		}
	}
}

/// Nearest neighboor optimized with possible out of screen coordinates (for cropping)
void flip_NNOptimized_AllowOutOfScreen_NES(uint8_t *nes_px, SDL_Surface *dst_surface, int new_w, int new_h) {
	int w1 = 256; //NWIDTH;
	int h1 = s_tlines;
	int w2 = new_w;
	int h2 = new_h;
	int x_ratio = (int) ((w1 << 16) / w2);
	int y_ratio = (int) ((h1 << 16) / h2);
	int x2, y2;

	/// --- Compute padding for centering when out of bounds ---
	int y_padding = (RES_HW_SCREEN_VERTICAL - new_h) / 2;
	int x_padding = 0;
	if (w2 > RES_HW_SCREEN_HORIZONTAL) {
		x_padding = (w2 - RES_HW_SCREEN_HORIZONTAL) / 2 + 1;
	}
	int x_padding_ratio = x_padding * w1 / w2;
	//printf("src_surface->h=%d, h2=%d\n", src_surface->h, h2);

	for (int i = 0; i < h2; i++) {
		if (i >= RES_HW_SCREEN_VERTICAL) {
			continue;
		}

		uint16_t *t = static_cast<uint16_t*>(dst_surface->pixels) + ((i + y_padding) * ((w2 > RES_HW_SCREEN_HORIZONTAL) ? RES_HW_SCREEN_HORIZONTAL : w2));
		y2 = (i * y_ratio) >> 16;
		uint8_t *p = (uint8_t *) (nes_px + (y2 * w1 + x_padding_ratio) * sizeof (uint8_t));
		int rat = 0;
		for (int j = 0; j < w2; j++) {
			if (j >= RES_HW_SCREEN_HORIZONTAL) {
				continue;
			}
			x2 = rat>>16;
			*t++ = s_psdl[p[x2]];
			rat += x_ratio;
			//printf("y=%d, x=%d, y2=%d, x2=%d, (y2*src_surface->w)+x2=%d\n", i, j, y2, x2, (y2 * src_surface->w) + x2);
		}
	}
}

/// Nearest neighboor optimized with possible out of screen coordinates (for cropping)
void flip_Downscale_LeftRightGaussianFilter_NES(uint8_t *nes_px, SDL_Surface *dst_surface, int new_w, int new_h) {
	int w1 = 256; //NWIDTH;
	int h1 = s_tlines;
	int w2 = new_w;
	int h2 = new_h;
	int x_ratio = (int) ((w1 << 16) / w2);
	int y_ratio = (int) ((h1 << 16) / h2);
	int x1, y1;

	/// --- Compute padding for centering when out of bounds ---
	int y_padding = (RES_HW_SCREEN_VERTICAL - new_h) / 2;
	int x_padding = 0;
	if (w2 > RES_HW_SCREEN_HORIZONTAL) {
		x_padding = (w2 - RES_HW_SCREEN_HORIZONTAL) / 2 + 1;
	}
	int x_padding_ratio = x_padding * w1 / w2;
	//printf("src_surface->h=%d, h2=%d\n", src_surface->h, h2);

	/// --- Interp params ---
	int px_diff_prev_x = 0;
	int px_diff_next_x = 0;
	uint32_t ponderation_factor;
	uint8_t left_px_missing, right_px_missing;

	uint16_t *cur_p;
	uint16_t *cur_p_left;
	uint16_t *cur_p_right;
	uint32_t red_comp, green_comp, blue_comp;

	for (int i = 0; i < h2; i++) {
		if (i >= RES_HW_SCREEN_VERTICAL) {
			continue;
		}

		uint16_t *t = static_cast<uint16_t*>(dst_surface->pixels) + ((i + y_padding) * ((w2 > RES_HW_SCREEN_HORIZONTAL) ? RES_HW_SCREEN_HORIZONTAL : w2));
		y1 = (i * y_ratio) >> 16;
		uint8_t *p = (uint8_t *) (nes_px + (y1 * w1 + x_padding_ratio) * sizeof (uint8_t));
		int rat = 0;
		for (int j = 0; j < w2; j++) {
			if (j >= RES_HW_SCREEN_HORIZONTAL) {
				continue;
			}

			// ------ current x value ------
			x1 = rat >> 16;
			px_diff_next_x = ((rat + x_ratio) >> 16) - x1;

			// ------ adapted bilinear with 3x3 gaussian blur -------
			cur_p = &s_psdl[*(p + x1)];
			if (px_diff_prev_x > 1 || px_diff_next_x > 1 ) {
				red_comp= ((*cur_p) & 0xF800) << 1;
				green_comp = ((*cur_p) & 0x07E0) << 1;
				blue_comp = ((*cur_p) & 0x001F) << 1;

				left_px_missing = (px_diff_prev_x > 1 && x1 > 0);
				right_px_missing = (px_diff_next_x > 1 && x1 + 1 < w1);
				ponderation_factor = 2 + left_px_missing + right_px_missing;

				// ---- Interpolate current and left ----
				if (left_px_missing) {
					cur_p_left = &s_psdl[*(p + x1 - 1)];
					red_comp += ((*cur_p_left) & 0xF800);
					green_comp += ((*cur_p_left) & 0x07E0);
					blue_comp += ((*cur_p_left) & 0x001F);
				}

				// ---- Interpolate current and right ----
				if (right_px_missing) {
					cur_p_right = &s_psdl[*(p + x1 + 1)];
					red_comp += ((*cur_p_right) & 0xF800);
					green_comp += ((*cur_p_right) & 0x07E0);
					blue_comp += ((*cur_p_right) & 0x001F);
				}

				/// --- Compute new px value ---
				if (ponderation_factor == 4) {
					red_comp = (red_comp >> 2) & 0xF800;
					green_comp = (green_comp >> 2) & 0x07C0;
					blue_comp = (blue_comp >> 2) & 0x001F;
				} else if (ponderation_factor == 2) {
					red_comp = (red_comp >> 1) & 0xF800;
					green_comp = (green_comp >> 1) & 0x07C0;
					blue_comp = (blue_comp >> 1) & 0x001F;
				} else {
					red_comp = (red_comp / ponderation_factor) & 0xF800;
					green_comp = (green_comp / ponderation_factor) & 0x07C0;
					blue_comp = (blue_comp / ponderation_factor) & 0x001F;
				}

				/// --- write pixel ---
				*t++ = red_comp + green_comp + blue_comp;
			} else {
			  *t++ = s_psdl[p[x1]];
			}

			/// save number of pixels to interpolate
			px_diff_prev_x = px_diff_next_x;

			// ------ next pixel ------
			rat += x_ratio;
			//printf("y=%d, x=%d, y2=%d, x2=%d, (y2*src_surface->w)+x2=%d\n", i, j, y2, x2, (y2 * src_surface->w) + x2);
		}
	}
}

/**
 * Pushes the given buffer of bits to the screen.
 */
void BlitScreen(uint8 *XBuf) {
	int x, x2, y, y2;
	// Taken from fceugc
	// FDS switch disk requested - need to eject, select, and insert
	// but not all at once!
	if (FDSSwitchRequested) {
		switch (FDSSwitchRequested) {
		case 1:
			FDSSwitchRequested++;
			FCEUI_FDSInsert(); // eject disk
			FDSTimer = 0;
			break;
		case 2:
			if (FDSTimer > 60) {
				FDSSwitchRequested++;
				FDSTimer = 0;
				FCEUI_FDSSelect(); // select other side
				FCEUI_FDSInsert(); // insert disk
			}
			break;
		case 3:
			if (FDSTimer > 200) {
				FDSSwitchRequested = 0;
				FDSTimer = 0;
			}
			break;
		}
		FDSTimer++;
	}

	// TODO - Move these to its own file?
	if (SDL_MUSTLOCK(hw_screen)) SDL_LockSurface(hw_screen);

	register uint8 *pBuf = XBuf;

	static int prev_aspect_ratio = aspect_ratio;

	/* Clear screen if AR changed */
	if (prev_aspect_ratio != aspect_ratio) {
		prev_aspect_ratio = aspect_ratio;
		dingoo_clear_video();
	}

	//printf("s_tlines = %d, s_srendline=%d, NOFFSET = %d, NWIDTH=%d\n", s_tlines, s_srendline, NOFFSET, NWIDTH);
	
	switch (aspect_ratio) {
		case ASPECT_RATIOS_TYPE_STRETCHED: 
		/* Stretched NN*/
		flip_NNOptimized_AllowOutOfScreen_NES(pBuf, hw_screen, hw_screen->w, hw_screen->h);
		break;

		case ASPECT_RATIOS_TYPE_CROPPED:
		/* Cropped but not centered yes for some games */
		pBuf += (s_srendline * 256) + NOFFSET;
		flip_NNOptimized_AllowOutOfScreen_NES(pBuf, hw_screen, NWIDTH, s_tlines);
		break;

		default:
		aspect_ratio = ASPECT_RATIOS_TYPE_CROPPED;
		/* Cropped NN*/
		flip_NNOptimized_AllowOutOfScreen_NES(pBuf, hw_screen, NWIDTH, s_tlines);
		break;
	}

	if (SDL_MUSTLOCK(hw_screen)) SDL_UnlockSurface(hw_screen);
	SDL_Flip(hw_screen);
}

/**
 *  Converts an x-y coordinate in the window manager into an x-y
 *  coordinate on FCEU's screen.
 */
uint32 PtoV(uint16 x, uint16 y) {
	y = (uint16) ((double) y);
	x = (uint16) ((double) x);
	if (s_clipSides) {
		x += 8;
	}
	y += s_srendline;
	return (x | (y << 16));
}

bool disableMovieMessages = false;
bool FCEUI_AviDisableMovieMessages() {
	if (disableMovieMessages)
		return true;

	return false;
}

void FCEUI_SetAviDisableMovieMessages(bool disable) {
	disableMovieMessages = disable;
}

//clear all screens (for multiple-buffering)
void dingoo_clear_video(void) {
	memset(hw_screen->pixels, 0, hw_screen->w*hw_screen->h*hw_screen->format->BytesPerPixel);
}
