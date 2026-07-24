#include "lvgl.h"
#include "widgets/lv_canvas.h"
#include "app.h"
#include "liblvgl/lv_port_indev.h"
#include "libsystem/fs_client.h"
#include "libsystem/fs_types.h"
#include "libsystem/systemd_client.h"
#include "libwindow/window.h"
#include "log.h"
#include "nes_core.h"
#include "stdio.h"
#include "string.h"
#include "stdint.h"

#define SYSTEM_OVERLAY_GAP 24
#define CONTENT_X 40
#define CONTENT_Y SYSTEM_OVERLAY_GAP
#define CONTENT_W 944
#define CONTENT_H (APP_DEFAULT_HEIGHT - CONTENT_Y - SYSTEM_OVERLAY_GAP)

#define SIDE_PANEL_X CONTENT_X
#define SIDE_PANEL_Y (CONTENT_Y + 106)
#define SIDE_PANEL_W 188
#define SIDE_PANEL_H (CONTENT_H - 106)
#define SCREEN_PANEL_X (SIDE_PANEL_X + SIDE_PANEL_W + 20)
#define SCREEN_PANEL_Y SIDE_PANEL_Y
#define SCREEN_PANEL_W (CONTENT_X + CONTENT_W - SCREEN_PANEL_X)
#define SCREEN_PANEL_H SIDE_PANEL_H
#define SCREEN_PANEL_PAD_X 16
#define SCREEN_PANEL_PAD_TOP 16
#define SCREEN_PANEL_PAD_BOTTOM 16
#define SCREEN_PANEL_INFO_H 30
#define NES_ROM_DIR "/root/system/apps/nes/roms"
#define NES_ROM_LIST_MAX 8U
#define NES_ROM_BUTTON_CAP 6U
#define NES_READ_CHUNK (64U * 1024U)

#define COLOR_BG          0xf6f1e4
#define COLOR_BG_ALT      0xe3edf9
#define COLOR_PANEL       0xfffbf2
#define COLOR_PANEL_ALT   0xf0e8d8
#define COLOR_LINE        0xd9cfbc
#define COLOR_TEXT        0x23314f
#define COLOR_DIM         0x6d728c
#define COLOR_ACCENT      0xd6524a
#define COLOR_ACCENT_SOFT 0xf7dad4
#define COLOR_GO          0x1f8a6b
#define COLOR_GO_SOFT     0xdaf3eb
#define COLOR_WARM        0xc9882e
#define COLOR_WARM_SOFT   0xf6ead3
#define COLOR_SCREEN      0x111318
#define COLOR_SCREEN_LINE 0x2c313d

enum {
	LINUX_KEY_TAB = 15,
	LINUX_KEY_Q = 16,
	LINUX_KEY_W = 17,
	LINUX_KEY_ENTER = 28,
	LINUX_KEY_A = 30,
	LINUX_KEY_S = 31,
	LINUX_KEY_D = 32,
	LINUX_KEY_J = 36,
	LINUX_KEY_K = 37,
	LINUX_KEY_Z = 44,
	LINUX_KEY_X = 45,
	LINUX_KEY_SPACE = 57,
	LINUX_KEY_UP = 103,
	LINUX_KEY_LEFT = 105,
	LINUX_KEY_RIGHT = 106,
	LINUX_KEY_DOWN = 108,
};

typedef struct nes_rom_entry {
	char name[FS_DIR_ENTRY_NAME_MAX];
	char path[FS_PATH_MAX];
	uint64_t size;
} nes_rom_entry_s;

typedef struct nes_state {
	fs_client_s *fs;
	systemd_client_s *systemd;
	lv_obj_t *screen_canvas;
	lv_obj_t *screen_message;
	lv_obj_t *status_value;
	lv_obj_t *detail_value;
	lv_obj_t *rom_value;
	lv_obj_t *fps_value;
	lv_obj_t *library_hint;
	lv_obj_t *rom_buttons[NES_ROM_BUTTON_CAP];
	lv_obj_t *rom_button_labels[NES_ROM_BUTTON_CAP];
	lv_obj_t *rom_more_label;
	lv_obj_t *key_anchor;
	nes_rom_entry_s roms[NES_ROM_LIST_MAX];
	uint32_t rom_count;
	int32_t current_rom_index;
	uint32_t current_rom_len;
	uint8_t touch_mask;
	uint32_t frame_counter;
	uint32_t fps_value_cached;
	uint64_t fps_window_start_ms;
} nes_state_s;

static nes_state_s g_nes = {0};
static lv_color_t g_screen_frame[NES_CORE_VISIBLE_WIDTH * NES_CORE_VISIBLE_HEIGHT];
static uint8_t g_rom_file_buf[NES_CORE_ROM_MAX_BYTES];

static void clear_static_flags(lv_obj_t *obj)
{
	if (obj == NULL) {
		return;
	}

	lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE |
			       LV_OBJ_FLAG_CLICK_FOCUSABLE);
}

static lv_obj_t *create_panel(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
			      lv_coord_t w, lv_coord_t h, uint32_t color)
{
	lv_obj_t *panel = lv_obj_create(parent);

	lv_obj_set_pos(panel, x, y);
	lv_obj_set_size(panel, w, h);
	lv_obj_set_style_bg_color(panel, lv_color_hex(color), 0);
	lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
	lv_obj_set_style_border_color(panel, lv_color_hex(COLOR_LINE), 0);
	lv_obj_set_style_border_width(panel, 1, 0);
	lv_obj_set_style_radius(panel, 28, 0);
	lv_obj_set_style_shadow_width(panel, 0, 0);
	lv_obj_set_style_pad_all(panel, 0, 0);
	clear_static_flags(panel);
	return panel;
}

static lv_obj_t *create_action_button(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
				      lv_coord_t w, lv_coord_t h, const char *text,
				      lv_obj_t **label_out)
{
	lv_obj_t *btn = lv_btn_create(parent);
	lv_obj_t *label = lv_label_create(btn);

	lv_obj_set_pos(btn, x, y);
	lv_obj_set_size(btn, w, h);
	lv_obj_set_style_radius(btn, 22, 0);
	lv_obj_set_style_bg_color(btn, lv_color_hex(COLOR_PANEL_ALT), 0);
	lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
	lv_obj_set_style_border_color(btn, lv_color_hex(COLOR_LINE), 0);
	lv_obj_set_style_border_width(btn, 1, 0);
	lv_obj_set_style_shadow_width(btn, 0, 0);
	lv_obj_set_style_pad_all(btn, 0, 0);
	lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

	lv_obj_center(label);
	lv_obj_set_width(label, w - 26);
	lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(COLOR_TEXT), 0);
	lv_label_set_text(label, text);
	if (label_out != NULL) {
		*label_out = label;
	}

	return btn;
}

static void nes_focus_keys(void)
{
	if (g_nes.key_anchor != NULL) {
		lv_group_focus_obj(g_nes.key_anchor);
	}
}

static char nes_lower_char(char ch)
{
	if (ch >= 'A' && ch <= 'Z') {
		return (char)(ch - 'A' + 'a');
	}
	return ch;
}

static uint8_t nes_path_has_suffix(const char *path, const char *suffix)
{
	uint32_t path_len = 0;
	uint32_t suffix_len = 0;

	if (path == NULL || suffix == NULL) {
		return 0U;
	}

	path_len = (uint32_t)strlen(path);
	suffix_len = (uint32_t)strlen(suffix);
	if (path_len < suffix_len) {
		return 0U;
	}

	for (uint32_t i = 0; i < suffix_len; i++) {
		if (nes_lower_char(path[path_len - suffix_len + i]) !=
		    nes_lower_char(suffix[i])) {
			return 0U;
		}
	}

	return 1U;
}

static int nes_compare_name(const char *lhs, const char *rhs)
{
	uint32_t idx = 0;

	if (lhs == NULL || rhs == NULL) {
		return 0;
	}

	while (lhs[idx] != '\0' && rhs[idx] != '\0') {
		char left = nes_lower_char(lhs[idx]);
		char right = nes_lower_char(rhs[idx]);

		if (left != right) {
			return (int)((uint8_t)left) - (int)((uint8_t)right);
		}
		idx++;
	}

	return (int)((uint8_t)lhs[idx]) - (int)((uint8_t)rhs[idx]);
}

static void nes_set_status(const char *text, uint32_t color)
{
	if (g_nes.status_value == NULL || text == NULL) {
		return;
	}

	lv_label_set_text(g_nes.status_value, text);
	lv_obj_set_style_text_color(g_nes.status_value, lv_color_hex(color), 0);
}

static void nes_set_detail(const char *text)
{
	if (g_nes.detail_value == NULL || text == NULL) {
		return;
	}

	lv_label_set_text(g_nes.detail_value, text);
}

static void nes_set_current_rom_label(const char *name)
{
	if (g_nes.rom_value == NULL) {
		return;
	}

	lv_label_set_text(g_nes.rom_value, name != NULL ? name : "--");
}

static void nes_set_fps_label(uint32_t fps)
{
	char text[32] = {0};

	if (g_nes.fps_value == NULL) {
		return;
	}

	snprintf(text, sizeof(text), "%u fps", (unsigned int)fps);
	lv_label_set_text(g_nes.fps_value, text);
}

static void nes_show_screen_message(const char *text)
{
	if (g_nes.screen_message == NULL) {
		return;
	}

	if (text == NULL || text[0] == '\0') {
		lv_obj_add_flag(g_nes.screen_message, LV_OBJ_FLAG_HIDDEN);
		lv_label_set_text(g_nes.screen_message, "");
		return;
	}

	lv_label_set_text(g_nes.screen_message, text);
	lv_obj_clear_flag(g_nes.screen_message, LV_OBJ_FLAG_HIDDEN);
	lv_obj_center(g_nes.screen_message);
}

static uint16_t nes_screen_zoom_value(void)
{
	lv_coord_t avail_w = SCREEN_PANEL_W - SCREEN_PANEL_PAD_X * 2;
	lv_coord_t avail_h = SCREEN_PANEL_H - SCREEN_PANEL_PAD_TOP -
			       SCREEN_PANEL_PAD_BOTTOM - SCREEN_PANEL_INFO_H;
	uint32_t zoom_x = 0U;
	uint32_t zoom_y = 0U;

	if (avail_w < 1) {
		avail_w = 1;
	}
	if (avail_h < 1) {
		avail_h = 1;
	}

	zoom_x = ((uint32_t)avail_w * 256U) / NES_CORE_VISIBLE_WIDTH;
	zoom_y = ((uint32_t)avail_h * 256U) / NES_CORE_VISIBLE_HEIGHT;
	if (zoom_x > zoom_y) {
		zoom_x = zoom_y;
	}
	if (zoom_x < 256U) {
		zoom_x = 256U;
	}

	return (uint16_t)zoom_x;
}

static lv_coord_t nes_zoomed_width(uint16_t zoom)
{
	return (lv_coord_t)(((uint32_t)NES_CORE_VISIBLE_WIDTH * zoom + 255U) / 256U);
}

static lv_coord_t nes_zoomed_height(uint16_t zoom)
{
	return (lv_coord_t)(((uint32_t)NES_CORE_VISIBLE_HEIGHT * zoom + 255U) / 256U);
}

static void nes_layout_screen_canvas(void)
{
	uint16_t zoom = 0U;
	lv_coord_t draw_w = 0;
	lv_coord_t draw_h = 0;
	lv_coord_t avail_h = 0;

	if (g_nes.screen_canvas == NULL) {
		return;
	}

	zoom = nes_screen_zoom_value();
	draw_w = nes_zoomed_width(zoom);
	draw_h = nes_zoomed_height(zoom);
	avail_h = SCREEN_PANEL_H - SCREEN_PANEL_PAD_TOP -
		  SCREEN_PANEL_PAD_BOTTOM - SCREEN_PANEL_INFO_H;

	lv_img_set_size_mode(g_nes.screen_canvas, LV_IMG_SIZE_MODE_REAL);
	lv_img_set_zoom(g_nes.screen_canvas, zoom);
	lv_obj_set_pos(g_nes.screen_canvas,
		      (SCREEN_PANEL_W - draw_w) / 2,
		      SCREEN_PANEL_PAD_TOP + (avail_h - draw_h) / 2);
}

static lv_color_t nes_rgb565_to_lv_color(uint16_t color)
{
	uint8_t r5 = (uint8_t)((color >> 11) & 0x1fU);
	uint8_t g6 = (uint8_t)((color >> 5) & 0x3fU);
	uint8_t b5 = (uint8_t)(color & 0x1fU);
	uint8_t r8 = (uint8_t)((r5 << 3) | (r5 >> 2));
	uint8_t g8 = (uint8_t)((g6 << 2) | (g6 >> 4));
	uint8_t b8 = (uint8_t)((b5 << 3) | (b5 >> 2));

	return lv_color_make(r8, g8, b8);
}

static void nes_fill_screen_black(void)
{
	memset(g_screen_frame, 0, sizeof(g_screen_frame));
	if (g_nes.screen_canvas != NULL) {
		lv_obj_invalidate(g_nes.screen_canvas);
	}
}

static void nes_apply_frame(void)
{
	const uint16_t *src = nes_core_get_visible_frame();

	if (src == NULL) {
		return;
	}

	for (uint32_t i = 0; i < NES_CORE_VISIBLE_WIDTH * NES_CORE_VISIBLE_HEIGHT; i++) {
		g_screen_frame[i] = nes_rgb565_to_lv_color(src[i]);
	}

	if (g_nes.screen_canvas != NULL) {
		lv_obj_invalidate(g_nes.screen_canvas);
	}
}

static uint8_t nes_scan_roms(void)
{
	fs_dir_list_response_s response = {0};
	int64_t ret = -1;

	g_nes.rom_count = 0U;
	if (g_nes.fs == NULL || g_nes.fs->ops.list_dir == NULL) {
		return 0U;
	}

	ret = (int64_t)g_nes.fs->ops.list_dir(g_nes.fs, NES_ROM_DIR, &response);
	if (ret < 0) {
		return 0U;
	}

	for (uint32_t i = 0; i < response.entry_count && g_nes.rom_count < NES_ROM_LIST_MAX; i++) {
		const fs_dir_entry_s *entry = &response.entries[i];
		nes_rom_entry_s *rom = NULL;

		if (fs_dir_entry_is_dir(entry) || !nes_path_has_suffix(entry->name, ".nes")) {
			continue;
		}

		rom = &g_nes.roms[g_nes.rom_count++];
		memset(rom, 0, sizeof(*rom));
		strncpy(rom->name, entry->name, sizeof(rom->name) - 1U);
		snprintf(rom->path, sizeof(rom->path), "%s/%s", NES_ROM_DIR, entry->name);
		rom->size = entry->size;
	}

	for (uint32_t i = 0; i + 1U < g_nes.rom_count; i++) {
		for (uint32_t j = i + 1U; j < g_nes.rom_count; j++) {
			if (nes_compare_name(g_nes.roms[i].name, g_nes.roms[j].name) > 0) {
				nes_rom_entry_s temp = g_nes.roms[i];

				g_nes.roms[i] = g_nes.roms[j];
				g_nes.roms[j] = temp;
			}
		}
	}

	return g_nes.rom_count > 0U;
}

static void nes_style_rom_button(uint32_t slot, uint8_t active)
{
	lv_obj_t *btn = g_nes.rom_buttons[slot];
	lv_obj_t *label = g_nes.rom_button_labels[slot];

	if (btn == NULL || label == NULL) {
		return;
	}

	lv_obj_set_style_bg_color(btn, lv_color_hex(active ? COLOR_ACCENT_SOFT : COLOR_PANEL_ALT), 0);
	lv_obj_set_style_border_color(btn, lv_color_hex(active ? COLOR_ACCENT : COLOR_LINE), 0);
	lv_obj_set_style_text_color(label, lv_color_hex(active ? COLOR_ACCENT : COLOR_TEXT), 0);
}

static void nes_refresh_library_ui(void)
{
	char text[160] = {0};

	for (uint32_t i = 0; i < NES_ROM_BUTTON_CAP; i++) {
		if (i < g_nes.rom_count) {
			lv_label_set_text(g_nes.rom_button_labels[i], g_nes.roms[i].name);
			lv_obj_clear_flag(g_nes.rom_buttons[i], LV_OBJ_FLAG_HIDDEN);
			nes_style_rom_button(i, (int32_t)i == g_nes.current_rom_index);
		} else {
			lv_obj_add_flag(g_nes.rom_buttons[i], LV_OBJ_FLAG_HIDDEN);
		}
	}

	if (g_nes.rom_count == 0U) {
		snprintf(text, sizeof(text),
			 "Scan %s\nPut .nes ROM files in this folder, then press Rescan.",
			 NES_ROM_DIR);
		lv_label_set_text(g_nes.library_hint, text);
		lv_label_set_text(g_nes.rom_more_label, "Library is empty.");
		lv_obj_clear_flag(g_nes.rom_more_label, LV_OBJ_FLAG_HIDDEN);
		return;
	}

	snprintf(text, sizeof(text),
		 "%u ROM%s from %s",
		 (unsigned int)g_nes.rom_count,
		 g_nes.rom_count == 1U ? "" : "s",
		 NES_ROM_DIR);
	lv_label_set_text(g_nes.library_hint, text);

	if (g_nes.rom_count > NES_ROM_BUTTON_CAP) {
		snprintf(text, sizeof(text),
			 "Showing the first %u entries.",
			 (unsigned int)NES_ROM_BUTTON_CAP);
		lv_label_set_text(g_nes.rom_more_label, text);
		lv_obj_clear_flag(g_nes.rom_more_label, LV_OBJ_FLAG_HIDDEN);
	} else {
		lv_obj_add_flag(g_nes.rom_more_label, LV_OBJ_FLAG_HIDDEN);
	}
}

static uint8_t nes_read_binary_file(const char *path, uint8_t *dst, uint32_t cap, uint32_t *len_out)
{
	uint64_t shm = 0;
	int64_t fd = -1;
	uint32_t total = 0;
	uint8_t ok = 0U;

	if (path == NULL || dst == NULL || cap == 0U || g_nes.fs == NULL || g_nes.systemd == NULL) {
		return 0U;
	}

	memset(dst, 0, cap);
	if (len_out != NULL) {
		*len_out = 0U;
	}

	fd = (int64_t)g_nes.fs->ops.open(g_nes.fs, (char *)path);
	if (fd < 0) {
		return 0U;
	}

	shm = g_nes.systemd->ops.alloc_shm(g_nes.systemd, NES_READ_CHUNK);
	if (shm == 0U) {
		goto out;
	}

	for (;;) {
		int64_t read_len = (int64_t)g_nes.fs->ops.read(g_nes.fs, (uint64_t)fd, shm, NES_READ_CHUNK);

		if (read_len < 0) {
			goto out;
		}
		if (read_len == 0) {
			break;
		}
		if (total + (uint32_t)read_len > cap) {
			goto out;
		}

		memcpy(dst + total, (void *)(uint64_t)shm, (uint32_t)read_len);
		total += (uint32_t)read_len;
	}

	ok = total > 0U ? 1U : 0U;
	if (ok && len_out != NULL) {
		*len_out = total;
	}

out:
	if (fd >= 0) {
		(void)g_nes.fs->ops.close(g_nes.fs, (uint64_t)fd);
	}
	if (shm != 0U) {
		(void)g_nes.systemd->ops.free_shm(g_nes.systemd, shm);
	}
	return ok;
}

static uint8_t nes_load_rom_index(uint32_t index)
{
	char error[128] = {0};
	char detail[192] = {0};
	uint32_t rom_len = 0U;

	if (index >= g_nes.rom_count) {
		return 0U;
	}

	if (!nes_read_binary_file(g_nes.roms[index].path, g_rom_file_buf, sizeof(g_rom_file_buf), &rom_len)) {
		nes_core_unload();
		g_nes.current_rom_index = -1;
		g_nes.current_rom_len = 0U;
		nes_fill_screen_black();
		nes_set_current_rom_label("--");
		nes_set_status("ROM read failed", COLOR_ACCENT);
		snprintf(detail, sizeof(detail),
			 "Could not open %s.\nVerify that the file is readable from %s.",
			 g_nes.roms[index].name,
			 NES_ROM_DIR);
		nes_set_detail(detail);
		nes_show_screen_message("ROM read failed");
		nes_refresh_library_ui();
		return 0U;
	}

	if (!nes_core_load_rom(g_rom_file_buf, rom_len, error, sizeof(error))) {
		nes_core_unload();
		g_nes.current_rom_index = -1;
		g_nes.current_rom_len = 0U;
		nes_fill_screen_black();
		nes_set_current_rom_label("--");
		nes_set_status("Unsupported cartridge", COLOR_ACCENT);
		snprintf(detail, sizeof(detail),
			 "%s\nFile: %s",
			 error,
			 g_nes.roms[index].name);
		nes_set_detail(detail);
		nes_show_screen_message(error);
		nes_refresh_library_ui();
		return 0U;
	}

	g_nes.current_rom_index = (int32_t)index;
	g_nes.current_rom_len = rom_len;
	g_nes.frame_counter = 0U;
	g_nes.fps_value_cached = 0U;
	g_nes.fps_window_start_ms = 0U;

	nes_core_set_buttons(0U);
	if (nes_core_run_frame(NULL)) {
		nes_apply_frame();
	}
	nes_show_screen_message(NULL);
	nes_set_current_rom_label(g_nes.roms[index].name);
	nes_set_status("Running", COLOR_GO);
		snprintf(detail, sizeof(detail),
			 "Loaded %s\n%llu KB cartridge image\nArrows move, J presses A, K presses B, Tab Select, Enter Start.",
			 g_nes.roms[index].name,
			 (unsigned long long)((g_nes.roms[index].size + 1023U) / 1024U));
	nes_set_detail(detail);
	nes_set_fps_label(60U);
	nes_refresh_library_ui();
	log_info("nes: loaded rom %s size=%u\n", g_nes.roms[index].name, (unsigned int)rom_len);
	return 1U;
}

static void nes_rescan_library(uint8_t autoload_first)
{
	char current_name[FS_DIR_ENTRY_NAME_MAX] = {0};
	int32_t restored_index = -1;

	if (g_nes.current_rom_index >= 0 &&
	    (uint32_t)g_nes.current_rom_index < g_nes.rom_count) {
		strncpy(current_name, g_nes.roms[g_nes.current_rom_index].name, sizeof(current_name) - 1U);
	}

	(void)nes_scan_roms();
	if (current_name[0] != '\0') {
		for (uint32_t i = 0; i < g_nes.rom_count; i++) {
			if (strcmp(current_name, g_nes.roms[i].name) == 0) {
				restored_index = (int32_t)i;
				break;
			}
		}
	}

	if (restored_index >= 0) {
		g_nes.current_rom_index = restored_index;
	} else if (g_nes.rom_count == 0U) {
		g_nes.current_rom_index = -1;
		g_nes.current_rom_len = 0U;
		nes_core_unload();
		nes_fill_screen_black();
		nes_show_screen_message("No ROM found");
		nes_set_current_rom_label("--");
		nes_set_status("Waiting for ROM", COLOR_WARM);
		nes_set_detail("Place .nes files under /root/system/apps/nes/roms and press Rescan.");
	} else if (autoload_first) {
		(void)nes_load_rom_index(0U);
	}

	nes_refresh_library_ui();
}

static uint8_t nes_keyboard_mask(void)
{
	uint8_t mask = 0U;

	if (lv_port_indev_linux_key_is_down(LINUX_KEY_UP) ||
	    lv_port_indev_linux_key_is_down(LINUX_KEY_W)) {
		mask |= NES_BUTTON_UP;
	}
	if (lv_port_indev_linux_key_is_down(LINUX_KEY_DOWN) ||
	    lv_port_indev_linux_key_is_down(LINUX_KEY_S)) {
		mask |= NES_BUTTON_DOWN;
	}
	if (lv_port_indev_linux_key_is_down(LINUX_KEY_LEFT) ||
	    lv_port_indev_linux_key_is_down(LINUX_KEY_A)) {
		mask |= NES_BUTTON_LEFT;
	}
	if (lv_port_indev_linux_key_is_down(LINUX_KEY_RIGHT) ||
	    lv_port_indev_linux_key_is_down(LINUX_KEY_D)) {
		mask |= NES_BUTTON_RIGHT;
	}
	if (lv_port_indev_linux_key_is_down(LINUX_KEY_J)) {
		mask |= NES_BUTTON_A;
	}
	if (lv_port_indev_linux_key_is_down(LINUX_KEY_K)) {
		mask |= NES_BUTTON_B;
	}
	if (lv_port_indev_linux_key_is_down(LINUX_KEY_TAB) ||
	    lv_port_indev_linux_key_is_down(LINUX_KEY_Q)) {
		mask |= NES_BUTTON_SELECT;
	}
	if (lv_port_indev_linux_key_is_down(LINUX_KEY_ENTER)) {
		mask |= NES_BUTTON_START;
	}

	return mask;
}

static void nes_virtual_button_visual(lv_obj_t *btn, uint8_t active)
{
	if (btn == NULL) {
		return;
	}

	lv_obj_set_style_bg_color(btn, lv_color_hex(active ? COLOR_ACCENT : COLOR_PANEL_ALT), 0);
	lv_obj_set_style_border_color(btn, lv_color_hex(active ? COLOR_ACCENT : COLOR_LINE), 0);
	lv_obj_set_style_translate_y(btn, active ? 2 : 0, 0);
}

static void nes_virtual_button_event_cb(lv_event_t *e)
{
	lv_event_code_t code = lv_event_get_code(e);
	lv_obj_t *btn = lv_event_get_target(e);
	uint8_t mask = (uint8_t)(uintptr_t)lv_event_get_user_data(e);

	if (code == LV_EVENT_PRESSED) {
		g_nes.touch_mask |= mask;
		nes_virtual_button_visual(btn, 1U);
		return;
	}
	if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
		g_nes.touch_mask &= (uint8_t)~mask;
		nes_virtual_button_visual(btn, 0U);
		nes_focus_keys();
	}
}

static void nes_rom_event_cb(lv_event_t *e)
{
	uint32_t slot = (uint32_t)(uintptr_t)lv_event_get_user_data(e);

	if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
		return;
	}

	(void)nes_load_rom_index(slot);
	nes_focus_keys();
}

static void nes_rescan_event_cb(lv_event_t *e)
{
	if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
		return;
	}

	nes_rescan_library(1U);
	nes_focus_keys();
}

static void nes_reset_event_cb(lv_event_t *e)
{
	if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
		return;
	}

	if (g_nes.current_rom_index >= 0) {
		(void)nes_load_rom_index((uint32_t)g_nes.current_rom_index);
	} else if (g_nes.rom_count > 0U) {
		(void)nes_load_rom_index(0U);
	}
	nes_focus_keys();
}

static void create_ui(void)
{
	lv_obj_t *scr = lv_scr_act();
	lv_obj_t *screen_panel = NULL;
	lv_obj_t *side_panel = NULL;
	lv_obj_t *label = NULL;

	lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_bg_color(scr, lv_color_hex(COLOR_BG), 0);
	lv_obj_set_style_bg_grad_color(scr, lv_color_hex(COLOR_BG_ALT), 0);
	lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, 0);

	label = lv_label_create(scr);
	lv_obj_set_pos(label, CONTENT_X, CONTENT_Y + 4);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(COLOR_ACCENT), 0);
	lv_label_set_text(label, "8-BIT EMULATION");

	label = lv_label_create(scr);
	lv_obj_set_pos(label, CONTENT_X, CONTENT_Y + 28);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_32, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(COLOR_TEXT), 0);
	lv_label_set_text(label, "NES");

	g_nes.library_hint = lv_label_create(scr);
	lv_obj_set_pos(g_nes.library_hint, CONTENT_X + 92, CONTENT_Y + 40);
	lv_obj_set_width(g_nes.library_hint, CONTENT_W - 92);
	lv_label_set_long_mode(g_nes.library_hint, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(g_nes.library_hint, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(g_nes.library_hint, lv_color_hex(COLOR_DIM), 0);
	lv_label_set_text(g_nes.library_hint, "--");

	side_panel = create_panel(scr, SIDE_PANEL_X, SIDE_PANEL_Y, SIDE_PANEL_W, SIDE_PANEL_H, COLOR_PANEL);

	screen_panel = create_panel(scr, SCREEN_PANEL_X, SCREEN_PANEL_Y, SCREEN_PANEL_W, SCREEN_PANEL_H, COLOR_PANEL);
	g_nes.screen_canvas = lv_canvas_create(screen_panel);
	lv_canvas_set_buffer(g_nes.screen_canvas, g_screen_frame,
			     NES_CORE_VISIBLE_WIDTH, NES_CORE_VISIBLE_HEIGHT,
			     LV_IMG_CF_TRUE_COLOR);
	lv_obj_set_style_border_width(g_nes.screen_canvas, 0, 0);
	lv_obj_set_style_bg_opa(g_nes.screen_canvas, LV_OPA_TRANSP, 0);
	clear_static_flags(g_nes.screen_canvas);
	nes_layout_screen_canvas();
	nes_fill_screen_black();

	g_nes.fps_value = lv_label_create(screen_panel);
	lv_obj_set_pos(g_nes.fps_value, 20, SCREEN_PANEL_H - 30);
	lv_obj_set_style_text_font(g_nes.fps_value, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(g_nes.fps_value, lv_color_hex(COLOR_GO), 0);
	lv_label_set_text(g_nes.fps_value, "0 fps");

	g_nes.screen_message = lv_label_create(screen_panel);
	lv_obj_set_width(g_nes.screen_message, 340);
	lv_label_set_long_mode(g_nes.screen_message, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(g_nes.screen_message, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_style_text_font(g_nes.screen_message, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(g_nes.screen_message, lv_color_hex(0xf3f5fb), 0);
	lv_label_set_text(g_nes.screen_message, "");
	lv_obj_center(g_nes.screen_message);
	lv_obj_add_flag(g_nes.screen_message, LV_OBJ_FLAG_HIDDEN);

	label = lv_label_create(side_panel);
	lv_obj_set_pos(label, 18, 18);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(COLOR_DIM), 0);
	lv_label_set_text(label, "CARTRIDGES");

	label = lv_label_create(side_panel);
	lv_obj_set_pos(label, 18, 40);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(COLOR_TEXT), 0);
	lv_label_set_text(label, "Library");

	for (uint32_t i = 0; i < NES_ROM_BUTTON_CAP; i++) {
		g_nes.rom_buttons[i] = create_action_button(side_panel, 18, 88 + (lv_coord_t)i * 48,
							     SIDE_PANEL_W - 36, 40, "--",
							     &g_nes.rom_button_labels[i]);
		lv_obj_add_event_cb(g_nes.rom_buttons[i], nes_rom_event_cb, LV_EVENT_CLICKED,
				    (void *)(uintptr_t)i);
	}

	g_nes.rom_more_label = lv_label_create(side_panel);
	lv_obj_set_pos(g_nes.rom_more_label, 18, 88 + (lv_coord_t)NES_ROM_BUTTON_CAP * 48);
	lv_obj_set_width(g_nes.rom_more_label, SIDE_PANEL_W - 36);
	lv_label_set_long_mode(g_nes.rom_more_label, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(g_nes.rom_more_label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(g_nes.rom_more_label, lv_color_hex(COLOR_DIM), 0);
	lv_label_set_text(g_nes.rom_more_label, "");

	{
		lv_obj_t *btn = create_action_button(side_panel, 18,
					     88 + (lv_coord_t)NES_ROM_BUTTON_CAP * 48 + 44,
					     SIDE_PANEL_W - 36, 42, "Rescan", &label);
		lv_obj_add_event_cb(btn, nes_rescan_event_cb, LV_EVENT_CLICKED, NULL);
	}

	{
		lv_obj_t *btn = create_action_button(side_panel, 18,
					     88 + (lv_coord_t)NES_ROM_BUTTON_CAP * 48 + 94,
					     SIDE_PANEL_W - 36, 42, "Reset", &label);
		lv_obj_add_event_cb(btn, nes_reset_event_cb, LV_EVENT_CLICKED, NULL);
	}

	g_nes.key_anchor = lv_btn_create(side_panel);
	lv_obj_set_pos(g_nes.key_anchor, 0, 0);
	lv_obj_set_size(g_nes.key_anchor, 1, 1);
	lv_obj_set_style_bg_opa(g_nes.key_anchor, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(g_nes.key_anchor, 0, 0);
	lv_obj_set_style_outline_width(g_nes.key_anchor, 0, LV_STATE_FOCUSED);
	lv_obj_set_style_shadow_width(g_nes.key_anchor, 0, 0);
	lv_obj_set_style_pad_all(g_nes.key_anchor, 0, 0);
	lv_obj_clear_flag(g_nes.key_anchor, LV_OBJ_FLAG_SCROLLABLE);
	nes_focus_keys();
}

static void nes_on_create(app_s *app)
{
	(void)app;

	memset(&g_nes, 0, sizeof(g_nes));
	g_nes.current_rom_index = -1;
	g_nes.fs = fs_client_get();
	g_nes.systemd = systemd_client_get();

	(void)nes_scan_roms();
	create_ui();
	nes_refresh_library_ui();

	if (g_nes.rom_count > 0U) {
		(void)nes_load_rom_index(0U);
	} else {
		nes_fill_screen_black();
		nes_show_screen_message("No ROM found");
		nes_set_current_rom_label("--");
		nes_set_status("Waiting for ROM", COLOR_WARM);
		nes_set_detail("Place .nes files under /root/system/apps/nes/roms, rebuild the image, then press Rescan.");
	}

	log_info("nes app ready roms=%u\n", (unsigned int)g_nes.rom_count);
}

static void nes_on_update(app_s *app, uint64_t mono_ms)
{
	uint8_t buttons = 0U;

	(void)app;
	if (!nes_core_is_loaded()) {
		return;
	}

	buttons = (uint8_t)(nes_keyboard_mask() | g_nes.touch_mask);
	nes_core_set_buttons(buttons);
	if (!nes_core_run_frame(NULL)) {
		return;
	}

	nes_apply_frame();
	if (g_nes.fps_window_start_ms == 0U) {
		g_nes.fps_window_start_ms = mono_ms;
	}
	g_nes.frame_counter++;
	if (mono_ms >= g_nes.fps_window_start_ms + 1000U) {
		g_nes.fps_value_cached = g_nes.frame_counter;
		g_nes.frame_counter = 0U;
		g_nes.fps_window_start_ms = mono_ms;
		nes_set_fps_label(g_nes.fps_value_cached);
	}
}

int main(int argc, char *argv[])
{
	app_s app = {0};
	app_config_s config = {
		.name = "nes",
		.x = APP_DEFAULT_X,
		.y = APP_DEFAULT_Y,
		.z = 0,
		.width = APP_DEFAULT_WIDTH,
		.height = APP_DEFAULT_HEIGHT,
		.window_flags = WINDOW_FLAG_FOCUSABLE,
		.enable_input = 1,
	};
	app_lifecycle_ops_s lifecycle = {
		.on_create = nes_on_create,
		.on_update = nes_on_update,
	};

	(void)argc;
	(void)argv;

	log_info("nes start!\n");
	app_init(&app, &config, &lifecycle, NULL);
	return app_run(&app);
}
