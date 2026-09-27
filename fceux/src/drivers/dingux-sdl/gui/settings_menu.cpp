#include "../config.h"

typedef struct _setting_entry {
	const char *name;
	const char *info;
	const std::string option;
	void (*update)(unsigned long);
} SettingEntry;

// #include "main_settings.cpp"
#include "video_settings.cpp"
#include "sound_settings.cpp"
#include "control_settings.cpp"

// #define SETTINGS_MENUSIZE 5

// static void cmd_main_settings(unsigned long key) {
// 	RunMainSettings();
// }

static void cmd_video_settings(unsigned long key) {
	RunVideoSettings();
}

static void cmd_sound_settings(unsigned long key) {
	RunSoundSettings();
}

static void cmd_control_settings(unsigned long key) {
	RunControlSettings();
}

static void cmd_config_save(unsigned long key) {
	extern Config *g_config;
	g_config->save();
}

static void gg_update(unsigned long key) {
	int val;

	if (key == DINGOO_RIGHT)
		val = 1;
	if (key == DINGOO_LEFT)
		val = 0;

	g_config->setOption("SDL.GameGenie", val);
}


// static MenuEntry
// 	settings_menu[] = {
// 		{ "Main Settings", "Change fceux main config", cmd_main_settings },
// 		{ "Video Settings", "Change video config", cmd_video_settings },
// 		{ "Sound Settings", "Change sound config", cmd_sound_settings },
// 		{ "Control Settings", "Change control config", cmd_control_settings },
// 		{ "Save config as default",	"Override default config", cmd_config_save }
// 	};

static SettingEntry settings_menu[] = 
{
	// { "Main Settings", "Change fceux main config", "NULL", cmd_main_settings },
	{ "Video", "Change video settings", "NULL", cmd_video_settings },
	{ "Audio", "Change sound settings", "NULL", cmd_sound_settings },
	{ "Input", "Change control settings", "NULL", cmd_control_settings },
	{ "Game Genie", "Emulate Game Genie", "SDL.GameGenie", gg_update },
	{ "Save settings",	"Save as default settings", "NULL", cmd_config_save }
};

int RunSettingsMenu() {
	static int index = 0;
	static int spy = 72;
	int done = 0, y, i;

	char tmp[32];
	int  itmp;

	static const int menu_size = sizeof(settings_menu) / sizeof(settings_menu[0]);
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

		// if (parsekey(DINGOO_A)) {
			// done = settings_menu[index].command();
		// }

		if (((index != menu_size - 2) && parsekey(DINGOO_A)) || (index == menu_size - 2 && (parsekey(DINGOO_RIGHT, 1) || parsekey(DINGOO_LEFT, 1))))
			settings_menu[index].update(g_key);
		// if (parsekey(DINGOO_A) || parsekey(DINGOO_RIGHT, 1) || parsekey(DINGOO_LEFT, 1))
			// settings_menu[index].update(g_key);

		// Must draw bg only when needed
		// Draw stuff
		if (g_dirty) {
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

			DrawText(gui_screen, "Settings", 8, 37);

			// Draw menu
			// for (i = 0, y = 72; i < SETTINGS_MENUSIZE; i++, y += 16) {
			for (i = offset_start, y = 72; i < offset_end; i++, y += 15) {
				DrawText(gui_screen, settings_menu[i].name, 60, y);

				sprintf(tmp, "");

				if (!strcmp(settings_menu[i].name, "Game Genie")) {
					g_config->getOption(settings_menu[i].option, &itmp);
					sprintf(tmp, "%s", itmp ? "on" : "off");
				}

				DrawText(gui_screen, tmp, 210, y);
			}

			// Draw info
			DrawText(gui_screen, settings_menu[index].info, 8, 225);

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

	// Must update emulation core and drivers
	UpdateEMUCore(g_config);
	FCEUD_DriverReset();

	// Clear screen
	dingoo_clear_video();

	g_dirty = 1;
}
