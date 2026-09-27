
// Externals
extern Config *g_config;

/* MENU COMMANDS */

// Fullscreen mode
static char *scale_tag[] = {
		"Original",
		"Aspect",
		"FS Fast",
		"FS Smooth"
};

// Use PAL or NTSC rate
static void pal_update(unsigned long key) {
	int val;

	if (key == DINGOO_RIGHT)
		val = 1;
	if (key == DINGOO_LEFT)
		val = 0;

	g_config->setOption("SDL.PAL", val);
}

static void sprite_limit_update(unsigned long key) {
	int val;

	if (key == DINGOO_RIGHT)
		val = 1;
	if (key == DINGOO_LEFT)
		val = 0;

	g_config->setOption("SDL.DisableSpriteLimit", val);
}

static void throttle_update(unsigned long key) {
	int val;

	if (key == DINGOO_RIGHT)
		val = 1;
	if (key == DINGOO_LEFT)
		val = 0;

	g_config->setOption("SDL.FPSThrottle", val);
}

static void showfps_update(unsigned long key) {
	int val;

	if (key == DINGOO_RIGHT)
		val = 1;
	if (key == DINGOO_LEFT)
		val = 0;

	g_config->setOption("SDL.ShowFPS", val);
}

// Custom palette
static void custom_update(unsigned long key) {
	const char *types[] = { ".pal", NULL };
	char palname[128] = "";

	#ifdef WIN32
	if (!RunFileBrowser("d:\\", palname, types, "Choose nes palette (.pal)")) 
	#else
	if (!RunFileBrowser("./palettes", palname, types, "Choose nes palette (.pal)")) 
	#endif
	{
		return;
		// CloseGame();
		// SDL_Quit();
		// exit(-1);
	}

	std::string cpalette = std::string(palname);
	g_config->setOption("SDL.Palette", cpalette);
}

static void fullscreen_update(unsigned long key)
{
	int val;
	g_config->getOption("SDL.Fullscreen", &val);

	if (key == DINGOO_RIGHT) val = val < 3 ? val+1 : 3;
	if (key == DINGOO_LEFT) val = val > 0 ? val-1 : 0;
   
	g_config->setOption("SDL.Fullscreen", val);
}

// Clip sides
static void clip_update(unsigned long key)
{
	int val, tmp;
	g_config->getOption("SDL.ClipSides", &tmp);

	if (key == DINGOO_RIGHT) val = 1;
	if (key == DINGOO_LEFT) val = 0;

	g_config->setOption("SDL.ClipSides", val);
}

// PPU emulation
static void newppu_update(unsigned long key)
{
	int val, tmp;
	g_config->getOption("SDL.NewPPU", &tmp);

	if (key == DINGOO_RIGHT) val = 1;
	if (key == DINGOO_LEFT) val = 0;

	g_config->setOption("SDL.NewPPU", val);
}

// NTSC TV's colors
static void ntsc_update(unsigned long key)
{
	int val;

	if (key == DINGOO_RIGHT) val = 1;
	if (key == DINGOO_LEFT) val = 0;

	g_config->setOption("SDL.NTSCpalette", val);
}

// NTSC Tint
static void tint_update(unsigned long key)
{
	int val;
	g_config->getOption("SDL.Tint", &val);

	if (key == DINGOO_RIGHT) val = val < 255 ? val+1 : 255;
	if (key == DINGOO_LEFT) val = val > 0 ? val-1 : 0;

	g_config->setOption("SDL.Tint", val);
}

// NTSC Hue
static void hue_update(unsigned long key)
{
	int val;
	g_config->getOption("SDL.Hue", &val);

	if (key == DINGOO_RIGHT) val = val < 255 ? val+1 : 255;
	if (key == DINGOO_LEFT) val = val > 0 ? val-1 : 0;

	g_config->setOption("SDL.Hue", val);
}

// Scanline start
static void slstart_update(unsigned long key)
{
	int val;
	g_config->getOption("SDL.ScanLineStart", &val);

	if (key == DINGOO_RIGHT) val = val < 239 ? val+1 : 239;
	if (key == DINGOO_LEFT) val = val > 0 ? val-1 : 0;

	g_config->setOption("SDL.ScanLineStart", val);
}

// Scanline end
static void slend_update(unsigned long key)
{
	int val;
	g_config->getOption("SDL.ScanLineEnd", &val);

	if (key == DINGOO_RIGHT) val = val < 239 ? val+1 : 239;
	if (key == DINGOO_LEFT) val = val > 0 ? val-1 : 0;

	g_config->setOption("SDL.ScanLineEnd", val);
}

/* VIDEO SETTINGS MENU */
static SettingEntry vd_menu[] = 
{
	{ "Video scaling", "Select video scale mode", "SDL.Fullscreen", fullscreen_update },
	{ "Show FPS", "Show frames per second", "SDL.ShowFPS", showfps_update },
	{ "FPS Throttle", "Use FPS throttling", "SDL.FPSThrottle", throttle_update },
	{ "Clip sides", "Clips left and right columns", "SDL.ClipSides", clip_update },
	{ "Sprite limit", "Use NES sprite limit", "SDL.DisableSpriteLimit", sprite_limit_update },
	{ "New PPU", "New PPU emulation engine", "SDL.NewPPU", newppu_update },
	{ "Scanline start", "The first drawn scanline", "SDL.ScanLineStart", slstart_update },
	{ "Scanline end", "The last drawn scanline", "SDL.ScanLineEnd", slend_update },
	{ "PAL timing", "Use PAL timing", "SDL.PAL", pal_update },
	{ "NTSC Palette", "Emulate NTSC TV's colors", "SDL.NTSCpalette", ntsc_update },
	{ "Tint", "Sets tint for NTSC color", "SDL.Tint", tint_update },
	{ "Hue", "Sets hue for NTSC color", "SDL.Hue", hue_update },
	{ "Custom palette", "Load custom palette", "SDL.Palette", custom_update },
};

int RunVideoSettings()
{
	// const int menu_size = sizeof(vd_menu) / sizeof(vd_menu[0]); //7;
	static int index = 0;
	static int spy = 72;
	int done = 0, y, i;

	char tmp[32];
	int  itmp;

	static const int menu_size = sizeof(vd_menu) / sizeof(vd_menu[0]);
	static const int max_entries = 8;
	static int offset_start = 0;
	static int offset_end = menu_size > max_entries ? max_entries : menu_size;

	g_dirty = 1;
	while (!done) {
		// Parse input
		readkey();
		if (parsekey(DINGOO_B)) done = 1;

		if (parsekey(DINGOO_UP, 1)) {
			if (index > 0) {
				index--;

				if (index >= offset_start)
					spy -= 15;

				if ((offset_start > 0) && (index < offset_start)) {
					offset_start--;
					offset_end--;
				}
			} else {
				index = menu_size-1;
				offset_end = menu_size;
				offset_start = menu_size <= max_entries ? 0 : offset_end - max_entries;
				spy = 72 + 15*(index - offset_start);
			}
		}

		if (parsekey(DINGOO_DOWN, 1)) {
			if (index < (menu_size - 1)) {
				index++;

				if (index < offset_end)
					spy += 15;

				if ((offset_end < menu_size) && (index >= offset_end)) {
					offset_end++;
					offset_start++;
				}
			} else {
				index = 0;
				offset_start = 0;
				offset_end = menu_size <= max_entries ? menu_size : max_entries;
				spy = 72;
			}
		}

		if (
			(index == menu_size - 1 && parsekey(DINGOO_A)) ||
			(index != menu_size - 1 && (parsekey(DINGOO_RIGHT, 1) || parsekey(DINGOO_LEFT, 1)))
		) {
			vd_menu[index].update(g_key);
		}
  
		// Draw stuff
		if( g_dirty ) 
		{
			draw_bg(g_bg);
			
			//Draw Top and Bottom Bars
			DrawChar(gui_screen, SP_SELECTOR, 0, 37);
			DrawChar(gui_screen, SP_SELECTOR, 81, 37);
			DrawChar(gui_screen, SP_SELECTOR, 0, 225);
			DrawChar(gui_screen, SP_SELECTOR, 81, 225);
			DrawText(gui_screen, "B - Go Back", 235, 225);
			DrawChar(gui_screen, SP_LOGO, 12, 9);
			
			// Draw selector
			DrawChar(gui_screen, SP_SELECTOR, 56, spy);
			DrawChar(gui_screen, SP_SELECTOR, 77, spy);

			DrawText(gui_screen, "Video Settings", 8, 37); 

			// Draw menu
			// for(i=0,y=72;i <= menu_size;i++,y+=15) {
			for (i = offset_start, y = 72; i < offset_end; i++, y += 15) {
				DrawText(gui_screen, vd_menu[i].name, 60, y);
		
				g_config->getOption(vd_menu[i].option, &itmp);
				if (!strncmp(vd_menu[i].name, "Video scaling", 5)) {
					sprintf(tmp, "%s", scale_tag[itmp]);
				}
				else if (!strncmp(vd_menu[i].name, "Custom palette", 6)) {
					std::string palname;
					g_config->getOption(vd_menu[i].option, &palname);

					// Remove path of string
					const int sz = static_cast<int> (palname.size());
					const int path_sz = palname.rfind("/", palname.size());

					if (path_sz == sz)
						strncpy(tmp, palname.c_str(), 32);
					else
						strncpy(tmp, palname.substr(path_sz + 1, sz - 1
								- path_sz).c_str(), 32);
				}
				else if (
					!strncmp(vd_menu[i].name, "Clip sides", 10)
					|| !strncmp(vd_menu[i].name, "New PPU", 7)
					|| !strncmp(vd_menu[i].name, "NTSC Palette", 12)
					|| !strcmp(vd_menu[i].name, "Show FPS")
					|| !strcmp(vd_menu[i].name, "FPS Throttle")
					|| !strcmp(vd_menu[i].name, "PAL timing")
					) {
					sprintf(tmp, "%s", itmp ? "on" : "off");
				}
				else if (
					!strcmp(vd_menu[i].name, "Sprite limit")
					) {
					sprintf(tmp, "%s", !itmp ? "on" : "off");
				}
				else sprintf(tmp, "%d", itmp);

				DrawText(gui_screen, tmp, 210, y);
			}

			// Draw info
			DrawText(gui_screen, vd_menu[index].info, 8, 225);

			// Draw offset marks
			if (offset_start > 0)
				DrawChar(gui_screen, SP_UPARROW, 157, 62);
			if (offset_end < menu_size)
				DrawChar(gui_screen, SP_DOWNARROW, 157, 203);

			g_dirty = 0;
		}

		SDL_Delay(16);
		
		// Update real screen
		FCEUGUI_Flip();
	}	

	// Clear screen
	dingoo_clear_video();

	g_dirty = 1;
	return 0;
}
