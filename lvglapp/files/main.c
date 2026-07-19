#include "lvgl.h"
#include "app.h"
#include "libwindow/window.h"
#include "log.h"
#include "libkernel/capcall.h"
#include "libsystem/fs_client.h"
#include "libsystem/statemgr_client.h"
#include "libsystem/systemd_client.h"
#include "stdio.h"
#include "string.h"
#include "stdint.h"

#define SYSTEM_OVERLAY_GAP 24
#define CONTENT_X 40
#define CONTENT_Y SYSTEM_OVERLAY_GAP
#define CONTENT_W 944
#define TOOLBAR_H 88
#define TOOLBAR_Y (CONTENT_Y + 74)
#define PANELS_Y (TOOLBAR_Y + TOOLBAR_H + 16)
#define PANELS_H (APP_DEFAULT_HEIGHT - PANELS_Y - SYSTEM_OVERLAY_GAP)
#define EXPLORER_W 376
#define DETAIL_W 552
#define PANEL_GAP 16
#define EXPLORER_INNER_X 22
#define EXPLORER_INNER_Y 96
#define EXPLORER_INNER_W (EXPLORER_W - (EXPLORER_INNER_X * 2))
#define EXPLORER_INNER_H (PANELS_H - EXPLORER_INNER_Y - 24)
#define DETAIL_INNER_X 24
#define DETAIL_PREVIEW_Y 164
#define DETAIL_PREVIEW_H (PANELS_H - DETAIL_PREVIEW_Y - 28)
#define DETAIL_PREVIEW_W (DETAIL_W - (DETAIL_INNER_X * 2))
#define FILE_PREVIEW_READ_MAX 1024U
#define FILE_PREVIEW_TEXT_MAX 3072U
#define FILEMGR_HOME_PATH "/root"
#define FILEMGR_THEME_REFRESH_INTERVAL_MS 100ULL

#define COLOR_BG           0xfff4ee
#define COLOR_BG_ALT       0xeaf4ff
#define COLOR_PANEL        0xfffffb
#define COLOR_PANEL_ALT    0xfff1eb
#define COLOR_PANEL_SOFT   0xe8f8f0
#define COLOR_LINE         0xf0ddd2
#define COLOR_TEXT         0x24324a
#define COLOR_DIM          0x7d8198
#define COLOR_ACCENT       0xff7d5c
#define COLOR_ACCENT_SOFT  0xffe6dd
#define COLOR_WARM         0xffb85e
#define COLOR_WARM_SOFT    0xfff0d2
#define COLOR_FILE         0x5d79d3
#define COLOR_FILE_SOFT    0xe8efff
#define COLOR_ALERT        0xf08d80

typedef enum filemgr_theme_mode {
	FILEMGR_THEME_LIGHT = 0,
	FILEMGR_THEME_DARK = 1,
} filemgr_theme_mode_e;

typedef struct filemgr_theme {
	uint32_t bg;
	uint32_t bg_alt;
	uint32_t panel;
	uint32_t panel_alt;
	uint32_t panel_soft;
	uint32_t line;
	uint32_t text;
	uint32_t dim;
	uint32_t accent;
	uint32_t accent_soft;
	uint32_t warm;
	uint32_t warm_soft;
	uint32_t file;
	uint32_t file_soft;
	uint32_t alert;
} filemgr_theme_s;

typedef enum filemgr_nav_action {
	FILEMGR_NAV_HOME = 0,
	FILEMGR_NAV_UP = 1,
	FILEMGR_NAV_REFRESH = 2,
} filemgr_nav_action_e;

typedef struct filemgr_state {
	fs_client_s *fs;
	systemd_client_s *systemd;
	uint64_t preview_shm;
	fs_dir_list_response_s listing;
	char current_path[FS_PATH_MAX];
	char selected_path[FS_PATH_MAX];
	char entry_paths[FS_DIR_LIST_MAX][FS_PATH_MAX];
	char preview_title[96];
	char preview_meta[96];
	char preview_path[FS_PATH_MAX];
	char preview_text[FILE_PREVIEW_TEXT_MAX];
	char status_text[24];
	uint32_t status_bg;
	uint32_t status_fg;
	uint8_t has_selection;
} filemgr_state_s;

static filemgr_state_s g_files = {0};
static statemgr_client_s *g_statemgr = NULL;
static uint32_t g_theme_mode = FILEMGR_THEME_LIGHT;
static uint64_t g_last_theme_ms = 0;
static uint64_t g_last_theme_revision = 0;

static const filemgr_theme_s g_filemgr_theme_light = {
	.bg = COLOR_BG,
	.bg_alt = COLOR_BG_ALT,
	.panel = COLOR_PANEL,
	.panel_alt = COLOR_PANEL_ALT,
	.panel_soft = COLOR_PANEL_SOFT,
	.line = COLOR_LINE,
	.text = COLOR_TEXT,
	.dim = COLOR_DIM,
	.accent = COLOR_ACCENT,
	.accent_soft = COLOR_ACCENT_SOFT,
	.warm = COLOR_WARM,
	.warm_soft = COLOR_WARM_SOFT,
	.file = COLOR_FILE,
	.file_soft = COLOR_FILE_SOFT,
	.alert = COLOR_ALERT,
};

static const filemgr_theme_s g_filemgr_theme_dark = {
	.bg = 0x171828,
	.bg_alt = 0x22253c,
	.panel = 0x20243a,
	.panel_alt = 0x2a3048,
	.panel_soft = 0x284146,
	.line = 0x46506f,
	.text = 0xfff6ef,
	.dim = 0xc7b7b0,
	.accent = 0xffa07c,
	.accent_soft = 0x4b2d2a,
	.warm = 0xf6c86f,
	.warm_soft = 0x4d3927,
	.file = 0x9ab8ff,
	.file_soft = 0x243552,
	.alert = 0xf3a096,
};

static const filemgr_theme_s *filemgr_theme(void)
{
	return g_theme_mode == FILEMGR_THEME_DARK ?
		&g_filemgr_theme_dark : &g_filemgr_theme_light;
}

static lv_obj_t *g_status_chip = NULL;
static lv_obj_t *g_path_label = NULL;
static lv_obj_t *g_summary_label = NULL;
static lv_obj_t *g_count_label = NULL;
static lv_obj_t *g_list_container = NULL;
static lv_obj_t *g_detail_title = NULL;
static lv_obj_t *g_detail_meta = NULL;
static lv_obj_t *g_detail_path = NULL;
static lv_obj_t *g_detail_preview_panel = NULL;
static lv_obj_t *g_detail_preview_label = NULL;

static void filemgr_render_list(void);
static uint8_t filemgr_navigate_to(const char *path);
static void filemgr_show_file_preview(uint32_t index);
static void filemgr_nav_event_cb(lv_event_t *e);
static void filemgr_cache_entry_paths(const char *base_path);
static void create_ui(void);

static void clear_static_flags(lv_obj_t *obj)
{
	if (obj == NULL) {
		return;
	}
	lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE |
			       LV_OBJ_FLAG_CLICK_FOCUSABLE);
}

static lv_obj_t *create_panel(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
			      lv_coord_t w, lv_coord_t h, uint32_t color, uint32_t border)
{
	lv_obj_t *panel = lv_obj_create(parent);

	lv_obj_set_pos(panel, x, y);
	lv_obj_set_size(panel, w, h);
	lv_obj_set_style_bg_color(panel, lv_color_hex(color), 0);
	lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
	lv_obj_set_style_border_color(panel, lv_color_hex(border), 0);
	lv_obj_set_style_border_width(panel, 0, 0);
	lv_obj_set_style_radius(panel, 28, 0);
	lv_obj_set_style_pad_all(panel, 0, 0);
	lv_obj_set_style_shadow_width(panel, 18, 0);
	lv_obj_set_style_shadow_color(panel, lv_color_hex(border), 0);
	lv_obj_set_style_shadow_opa(panel, 28, 0);
	clear_static_flags(panel);
	return panel;
}

static uint8_t filemgr_get_theme_revision(uint64_t *revision_out)
{
	uint64_t revision = 0;

	if (revision_out == NULL) {
		return 0U;
	}
	if (g_statemgr == NULL) {
		g_statemgr = statemgr_client_get();
	}
	if (g_statemgr == NULL || g_statemgr->ops.get_revision == NULL) {
		return 0U;
	}

	revision = g_statemgr->ops.get_revision(g_statemgr);
	if (revision == 0U) {
		return 0U;
	}

	*revision_out = revision;
	return 1U;
}

static uint8_t filemgr_read_theme_mode(uint32_t *theme_mode_out)
{
	statemgr_get_response_s response = {0};

	if (theme_mode_out == NULL) {
		return 0U;
	}
	if (g_statemgr == NULL) {
		g_statemgr = statemgr_client_get();
	}
	if (g_statemgr == NULL || g_statemgr->ops.get == NULL) {
		return 0U;
	}
	if (g_statemgr->ops.get(g_statemgr, "ui.theme.mode", &response) == 0U ||
	    !response.found || response.entry.type != STATEMGR_VALUE_TYPE_U32) {
		return 0U;
	}

	*theme_mode_out = (uint32_t)response.entry.value_u64 == FILEMGR_THEME_DARK ?
		FILEMGR_THEME_DARK : FILEMGR_THEME_LIGHT;
	return 1U;
}

static lv_obj_t *create_chip(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
			     const char *text, uint32_t bg, uint32_t fg)
{
	lv_obj_t *chip = lv_label_create(parent);

	lv_obj_set_pos(chip, x, y);
	lv_obj_set_style_bg_color(chip, lv_color_hex(bg), 0);
	lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, 0);
	lv_obj_set_style_radius(chip, 999, 0);
	lv_obj_set_style_pad_left(chip, 12, 0);
	lv_obj_set_style_pad_right(chip, 12, 0);
	lv_obj_set_style_pad_top(chip, 6, 0);
	lv_obj_set_style_pad_bottom(chip, 6, 0);
	lv_obj_set_style_text_font(chip, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(chip, lv_color_hex(fg), 0);
	lv_label_set_text(chip, text);
	clear_static_flags(chip);
	return chip;
}

static lv_obj_t *create_toolbar_button(lv_obj_t *parent, lv_coord_t x, const char *text,
				       filemgr_nav_action_e action)
{
	const filemgr_theme_s *theme = filemgr_theme();
	lv_obj_t *btn = lv_btn_create(parent);
	lv_obj_t *label = lv_label_create(btn);

	lv_obj_set_pos(btn, x, 20);
	lv_obj_set_size(btn, 112, 48);
	lv_obj_set_style_radius(btn, 18, 0);
	lv_obj_set_style_bg_color(btn, lv_color_hex(theme->panel_alt), 0);
	lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
	lv_obj_set_style_border_color(btn, lv_color_hex(theme->line), 0);
	lv_obj_set_style_border_width(btn, 1, 0);
	lv_obj_set_style_shadow_width(btn, 0, 0);
	lv_obj_set_style_outline_width(btn, 0, LV_STATE_FOCUSED);
	lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_event_cb(btn, filemgr_nav_event_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)action);

	lv_label_set_text(label, text);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->text), 0);
	lv_obj_center(label);
	return btn;
}

static char filemgr_lower_char(char ch)
{
	if (ch >= 'A' && ch <= 'Z') {
		return (char)(ch - 'A' + 'a');
	}
	return ch;
}

static void filemgr_normalize_path(const char *src, char *dst, uint32_t cap)
{
	char tmp[FS_PATH_MAX] = {0};
	uint32_t out = 0;
	uint8_t last_slash = 0;

	if (dst == NULL || cap == 0U) {
		return;
	}

	if (src == NULL || src[0] == '\0') {
		memset(dst, 0, cap);
		strncpy(dst, "/", cap - 1U);
		return;
	}
	strncpy(tmp, src, sizeof(tmp) - 1U);
	memset(dst, 0, cap);

	if (tmp[0] != '/' && out + 1U < cap) {
		dst[out++] = '/';
		last_slash = 1;
	}

	for (uint32_t i = 0; tmp[i] != '\0' && out + 1U < cap; i++) {
		char ch = tmp[i];

		if (ch == '/') {
			if (out == 0U || !last_slash) {
				dst[out++] = '/';
			}
			last_slash = 1;
			continue;
		}

		dst[out++] = ch;
		last_slash = 0;
	}

	if (out == 0U) {
		dst[out++] = '/';
	}
	while (out > 1U && dst[out - 1U] == '/') {
		out--;
	}
	dst[out] = '\0';
}

static void filemgr_parent_path(const char *path, char *out, uint32_t cap)
{
	uint32_t len = 0;

	filemgr_normalize_path(path, out, cap);
	if (strcmp(out, "/") == 0) {
		return;
	}

	len = strlen(out);
	while (len > 1U && out[len - 1U] != '/') {
		len--;
	}
	if (len <= 1U) {
		strncpy(out, "/", cap - 1U);
		return;
	}
	out[len - 1U] = '\0';
}

static uint8_t filemgr_join_path(const char *base, const char *name, char *out, uint32_t cap)
{
	if (base == NULL || name == NULL || out == NULL || cap == 0U) {
		return 0;
	}

	if (strcmp(base, "/") == 0) {
		snprintf(out, cap, "/%s", name);
	} else {
		snprintf(out, cap, "%s/%s", base, name);
	}
	filemgr_normalize_path(out, out, cap);
	return 1;
}

static int filemgr_compare_names(const char *lhs, const char *rhs)
{
	uint32_t idx = 0;

	if (lhs == NULL || rhs == NULL) {
		return 0;
	}

	while (lhs[idx] != '\0' && rhs[idx] != '\0') {
		char left = filemgr_lower_char(lhs[idx]);
		char right = filemgr_lower_char(rhs[idx]);

		if (left != right) {
			return (int)((uint8_t)left) - (int)((uint8_t)right);
		}
		idx++;
	}

	return (int)((uint8_t)filemgr_lower_char(lhs[idx])) -
	       (int)((uint8_t)filemgr_lower_char(rhs[idx]));
}

static int filemgr_compare_entries(const fs_dir_entry_s *lhs, const fs_dir_entry_s *rhs)
{
	uint32_t lhs_dir = fs_dir_entry_is_dir(lhs);
	uint32_t rhs_dir = fs_dir_entry_is_dir(rhs);
	uint32_t lhs_mount = fs_dir_entry_is_mount(lhs);
	uint32_t rhs_mount = fs_dir_entry_is_mount(rhs);

	if (lhs_dir != rhs_dir) {
		return lhs_dir ? -1 : 1;
	}
	if (lhs_mount != rhs_mount) {
		return lhs_mount ? -1 : 1;
	}
	return filemgr_compare_names(lhs->name, rhs->name);
}

static void filemgr_sort_entries(fs_dir_list_response_s *response)
{
	if (response == NULL || response->entry_count < 2U) {
		return;
	}

	for (uint32_t i = 0; i + 1U < response->entry_count; i++) {
		for (uint32_t j = i + 1U; j < response->entry_count; j++) {
			if (filemgr_compare_entries(&response->entries[i], &response->entries[j]) > 0) {
				fs_dir_entry_s tmp = response->entries[i];
				response->entries[i] = response->entries[j];
				response->entries[j] = tmp;
			}
		}
	}
}

static void filemgr_format_size(uint64_t size, char *out, uint32_t cap)
{
	if (out == NULL || cap == 0U) {
		return;
	}

	if (size < 1024ULL) {
		snprintf(out, cap, "%llu B", (unsigned long long)size);
		return;
	}
	if (size < 1024ULL * 1024ULL) {
		snprintf(out, cap, "%llu KB", (unsigned long long)((size + 1023ULL) / 1024ULL));
		return;
	}

	snprintf(out, cap, "%llu MB",
		 (unsigned long long)((size + (1024ULL * 1024ULL) - 1ULL) / (1024ULL * 1024ULL)));
}

static void filemgr_set_status(const char *text, uint32_t bg, uint32_t fg)
{
	if (text == NULL) {
		return;
	}

	strncpy(g_files.status_text, text, sizeof(g_files.status_text) - 1U);
	g_files.status_text[sizeof(g_files.status_text) - 1U] = '\0';
	g_files.status_bg = bg;
	g_files.status_fg = fg;

	if (g_status_chip == NULL) {
		return;
	}

	lv_label_set_text(g_status_chip, g_files.status_text);
	lv_obj_set_style_bg_color(g_status_chip, lv_color_hex(bg), 0);
	lv_obj_set_style_text_color(g_status_chip, lv_color_hex(fg), 0);
}

static void filemgr_set_preview(const char *title, const char *meta, const char *path,
				const char *body)
{
	strncpy(g_files.preview_title, title == NULL ? "" : title, sizeof(g_files.preview_title) - 1U);
	g_files.preview_title[sizeof(g_files.preview_title) - 1U] = '\0';
	strncpy(g_files.preview_meta, meta == NULL ? "" : meta, sizeof(g_files.preview_meta) - 1U);
	g_files.preview_meta[sizeof(g_files.preview_meta) - 1U] = '\0';
	strncpy(g_files.preview_path, path == NULL ? "" : path, sizeof(g_files.preview_path) - 1U);
	g_files.preview_path[sizeof(g_files.preview_path) - 1U] = '\0';
	if (g_detail_title != NULL) {
		lv_label_set_text(g_detail_title, g_files.preview_title);
	}
	if (g_detail_meta != NULL) {
		lv_label_set_text(g_detail_meta, g_files.preview_meta);
	}
	if (g_detail_path != NULL) {
		lv_label_set_text(g_detail_path, g_files.preview_path);
	}
	if (g_detail_preview_label != NULL) {
		lv_label_set_text(g_detail_preview_label, body == NULL ? "" : body);
	}
	if (g_detail_preview_panel != NULL) {
		lv_obj_scroll_to_y(g_detail_preview_panel, 0, LV_ANIM_OFF);
	}
}

static void filemgr_show_directory_overview(void)
{
	char title[128] = {0};
	char meta[96] = {0};
	char body[320] = {0};

	snprintf(title, sizeof(title), "%s", strcmp(g_files.current_path, "/") == 0 ? "Root" :
		 g_files.current_path);
	snprintf(meta, sizeof(meta), "%u item%s%s",
		 g_files.listing.total_count,
		 g_files.listing.total_count == 1U ? "" : "s",
		 g_files.listing.truncated ? " shown partially" : "");
	snprintf(body, sizeof(body),
		 "Choose a folder to drill down or tap a file to inspect its contents. "
		 "Directories are grouped first, followed by files.");
	filemgr_set_preview(title, meta, g_files.current_path, body);
}

static void filemgr_append_char(char *dst, uint32_t cap, uint32_t *offset, char ch)
{
	if (dst == NULL || offset == NULL || *offset + 1U >= cap) {
		return;
	}

	dst[*offset] = ch;
	(*offset)++;
	dst[*offset] = '\0';
}

static void filemgr_append_text(char *dst, uint32_t cap, uint32_t *offset, const char *text)
{
	if (dst == NULL || offset == NULL || text == NULL) {
		return;
	}

	for (uint32_t i = 0; text[i] != '\0' && *offset + 1U < cap; i++) {
		dst[*offset] = text[i];
		(*offset)++;
	}
	dst[*offset] = '\0';
}

static uint8_t filemgr_buffer_is_text(const uint8_t *buf, uint32_t len)
{
	uint32_t binary_count = 0;

	if (buf == NULL) {
		return 0;
	}
	if (len == 0U) {
		return 1;
	}

	for (uint32_t i = 0; i < len; i++) {
		uint8_t ch = buf[i];

		if (ch == '\n' || ch == '\r' || ch == '\t') {
			continue;
		}
		if (ch >= 0x20U && ch <= 0x7eU) {
			continue;
		}
		binary_count++;
		if (ch == 0U) {
			return 0;
		}
	}

	return binary_count * 6U < len;
}

static void filemgr_build_text_preview(const uint8_t *buf, uint32_t len, uint64_t total_size)
{
	uint32_t offset = 0;

	memset(g_files.preview_text, 0, sizeof(g_files.preview_text));
	if (buf == NULL || len == 0U) {
		filemgr_append_text(g_files.preview_text, sizeof(g_files.preview_text), &offset,
				   "(empty file)");
		return;
	}

	for (uint32_t i = 0; i < len && offset + 2U < sizeof(g_files.preview_text); i++) {
		uint8_t ch = buf[i];

		if (ch == '\r') {
			continue;
		}
		if (ch == '\n' || ch == '\t' || (ch >= 0x20U && ch <= 0x7eU)) {
			filemgr_append_char(g_files.preview_text, sizeof(g_files.preview_text), &offset,
					    (char)ch);
		} else {
			filemgr_append_char(g_files.preview_text, sizeof(g_files.preview_text), &offset, '.');
		}
	}

	if (total_size > len) {
		filemgr_append_text(g_files.preview_text, sizeof(g_files.preview_text), &offset,
				   "\n\n[preview truncated]");
	}
}

static void filemgr_build_hex_preview(const uint8_t *buf, uint32_t len, uint64_t total_size)
{
	uint32_t offset = 0;
	uint32_t preview_len = len > 128U ? 128U : len;

	memset(g_files.preview_text, 0, sizeof(g_files.preview_text));
	if (buf == NULL || len == 0U) {
		filemgr_append_text(g_files.preview_text, sizeof(g_files.preview_text), &offset,
				   "(empty file)");
		return;
	}

	for (uint32_t row = 0; row < preview_len && offset + 1U < sizeof(g_files.preview_text); row += 16U) {
		char line[128] = {0};
		uint32_t line_len = 0;

		line_len += (uint32_t)snprintf(line + line_len, sizeof(line) - line_len, "%04x  ", row);
		for (uint32_t col = 0; col < 16U; col++) {
			if (row + col < preview_len) {
				line_len += (uint32_t)snprintf(line + line_len, sizeof(line) - line_len,
							       "%02x ", buf[row + col]);
			} else {
				line_len += (uint32_t)snprintf(line + line_len, sizeof(line) - line_len,
							       "   ");
			}
		}
		line_len += (uint32_t)snprintf(line + line_len, sizeof(line) - line_len, " ");
		for (uint32_t col = 0; col < 16U && row + col < preview_len; col++) {
			uint8_t ch = buf[row + col];
			line_len += (uint32_t)snprintf(line + line_len, sizeof(line) - line_len, "%c",
							       (ch >= 0x20U && ch <= 0x7eU) ? ch : '.');
		}
		line_len += (uint32_t)snprintf(line + line_len, sizeof(line) - line_len, "\n");
		filemgr_append_text(g_files.preview_text, sizeof(g_files.preview_text), &offset, line);
	}

	if (total_size > preview_len) {
		filemgr_append_text(g_files.preview_text, sizeof(g_files.preview_text), &offset,
				   "\n[preview truncated]");
	}
}

static void filemgr_entry_event_cb(lv_event_t *e)
{
	const char *target_path = NULL;
	uint32_t index = FS_DIR_LIST_MAX;

	if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
		return;
	}

	target_path = (const char *)lv_event_get_user_data(e);
	if (target_path == NULL) {
		return;
	}

	for (uint32_t i = 0; i < g_files.listing.entry_count; i++) {
		if ((const void *)target_path == (const void *)g_files.entry_paths[i]) {
			index = i;
			break;
		}
	}
	if (index >= g_files.listing.entry_count) {
		return;
	}

	log_info("entry click idx=%d name=%s path=%s dir=%d mount=%d current=%s\n",
		 index,
		 g_files.listing.entries[index].name,
		 g_files.entry_paths[index],
		 fs_dir_entry_is_dir(&g_files.listing.entries[index]),
		 fs_dir_entry_is_mount(&g_files.listing.entries[index]),
		 g_files.current_path);

	if (fs_dir_entry_is_dir(&g_files.listing.entries[index])) {
		(void)filemgr_navigate_to(g_files.entry_paths[index]);
		return;
	}

	filemgr_show_file_preview(index);
}

static void filemgr_nav_event_cb(lv_event_t *e)
{
	filemgr_nav_action_e action = FILEMGR_NAV_HOME;
	char path[FS_PATH_MAX] = {0};

	if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
		return;
	}

	action = (filemgr_nav_action_e)(uintptr_t)lv_event_get_user_data(e);
	switch (action) {
		case FILEMGR_NAV_HOME:
			(void)filemgr_navigate_to(FILEMGR_HOME_PATH);
			break;
		case FILEMGR_NAV_UP:
			filemgr_parent_path(g_files.current_path, path, sizeof(path));
			(void)filemgr_navigate_to(path);
			break;
		case FILEMGR_NAV_REFRESH:
			(void)filemgr_navigate_to(g_files.current_path);
			break;
		default:
			break;
	}
}

static void filemgr_update_toolbar(void)
{
	char summary[128] = {0};
	char count_text[64] = {0};

	if (g_path_label != NULL) {
		lv_label_set_text(g_path_label, g_files.current_path);
	}

	snprintf(summary, sizeof(summary), "%u item%s available",
		 g_files.listing.total_count,
		 g_files.listing.total_count == 1U ? "" : "s");
	if (g_summary_label != NULL) {
		lv_label_set_text(g_summary_label, summary);
	}

	snprintf(count_text, sizeof(count_text), "%u entries%s",
		 g_files.listing.entry_count,
		 g_files.listing.truncated ? " (trimmed)" : "");
	if (g_count_label != NULL) {
		lv_label_set_text(g_count_label, count_text);
	}
}

static void filemgr_render_entry_row(uint32_t index)
{
	const filemgr_theme_s *theme = filemgr_theme();
	const fs_dir_entry_s *entry = &g_files.listing.entries[index];
	lv_obj_t *btn = NULL;
	lv_obj_t *badge = NULL;
	lv_obj_t *title = NULL;
	lv_obj_t *meta = NULL;
	char meta_text[96] = {0};
	uint32_t is_dir = fs_dir_entry_is_dir(entry);
	uint32_t is_mount = fs_dir_entry_is_mount(entry);
	uint32_t selected = 0;

	selected = g_files.has_selection &&
		   strcmp(g_files.entry_paths[index], g_files.selected_path) == 0;

	btn = lv_btn_create(g_list_container);
	lv_obj_set_size(btn, EXPLORER_INNER_W, 76);
	lv_obj_set_style_radius(btn, 22, 0);
	lv_obj_set_style_bg_color(btn, lv_color_hex(selected ? theme->accent_soft :
						       (is_dir ? theme->panel_soft : theme->panel_alt)), 0);
	lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
	lv_obj_set_style_border_color(btn, lv_color_hex(selected ? theme->accent : theme->line), 0);
	lv_obj_set_style_border_width(btn, selected ? 2 : 1, 0);
	lv_obj_set_style_shadow_width(btn, 0, 0);
	lv_obj_set_style_outline_width(btn, 0, LV_STATE_FOCUSED);
	lv_obj_set_style_pad_all(btn, 0, 0);
	lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_event_cb(btn, filemgr_entry_event_cb, LV_EVENT_CLICKED, g_files.entry_paths[index]);

	badge = create_chip(btn, 16, 14, is_mount ? "MOUNT" : (is_dir ? "DIR" : "FILE"),
			    is_mount ? theme->warm_soft : (is_dir ? theme->accent_soft : theme->file_soft),
			    is_mount ? theme->warm : (is_dir ? theme->accent : theme->file));
	title = lv_label_create(btn);
	lv_obj_set_pos(title, 96, 14);
	lv_obj_set_width(title, EXPLORER_INNER_W - 112);
	lv_label_set_long_mode(title, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(title, lv_color_hex(theme->text), 0);
	lv_label_set_text(title, entry->name);

	meta = lv_label_create(btn);
	lv_obj_set_pos(meta, 96, 42);
	lv_obj_set_width(meta, EXPLORER_INNER_W - 112);
	lv_label_set_long_mode(meta, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(meta, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(meta, lv_color_hex(theme->dim), 0);
	if (is_mount) {
		snprintf(meta_text, sizeof(meta_text), "Mounted directory");
	} else if (is_dir) {
		snprintf(meta_text, sizeof(meta_text), "Directory");
	} else {
		char size_text[32] = {0};
		filemgr_format_size(entry->size, size_text, sizeof(size_text));
		snprintf(meta_text, sizeof(meta_text), "File - %s", size_text);
	}
	lv_label_set_text(meta, meta_text);
	(void)badge;
}

static void filemgr_render_list(void)
{
	const filemgr_theme_s *theme = filemgr_theme();
	if (g_list_container == NULL) {
		return;
	}

	lv_obj_clean(g_list_container);
	if (g_files.listing.entry_count == 0U) {
		lv_obj_t *empty = lv_label_create(g_list_container);

		lv_obj_set_width(empty, EXPLORER_INNER_W - 16);
		lv_label_set_long_mode(empty, LV_LABEL_LONG_WRAP);
		lv_obj_set_style_text_font(empty, &lv_font_montserrat_16, 0);
		lv_obj_set_style_text_color(empty, lv_color_hex(theme->dim), 0);
		lv_label_set_text(empty, "This location is empty or does not expose child items yet.");
		return;
	}

	for (uint32_t i = 0; i < g_files.listing.entry_count; i++) {
		filemgr_render_entry_row(i);
	}
}

static void filemgr_show_error_preview(const char *title, const char *path, const char *message)
{
	const filemgr_theme_s *theme = filemgr_theme();
	filemgr_set_preview(title, "Unavailable", path, message);
	filemgr_set_status("Error", theme->warm_soft, theme->alert);
}

static void filemgr_show_file_preview(uint32_t index)
{
	const fs_dir_entry_s *entry = NULL;
	int64_t fd = -1;
	int64_t read_len = -1;
	char meta[96] = {0};
	char size_text[32] = {0};
	const uint8_t *buf = (const uint8_t *)(uint64_t)g_files.preview_shm;
	const char *preview_kind = "Binary preview";

	if (index >= g_files.listing.entry_count || g_files.fs == NULL || g_files.preview_shm == 0U) {
		return;
	}

	entry = &g_files.listing.entries[index];
	if (fs_dir_entry_is_dir(entry) || g_files.entry_paths[index][0] == '\0') {
		return;
	}

	fd = (int64_t)g_files.fs->ops.open(g_files.fs, g_files.entry_paths[index]);
	if (fd < 0) {
		filemgr_show_error_preview(entry->name, g_files.entry_paths[index],
					   "Failed to open this file.");
		return;
	}

	memset((void *)(uint64_t)g_files.preview_shm, 0, FILE_PREVIEW_READ_MAX);
	read_len = (int64_t)g_files.fs->ops.read(g_files.fs, (uint64_t)fd, g_files.preview_shm,
						 FILE_PREVIEW_READ_MAX);
	(void)g_files.fs->ops.close(g_files.fs, (uint64_t)fd);
	if (read_len < 0) {
		filemgr_show_error_preview(entry->name, g_files.entry_paths[index],
					   "Failed to read bytes from this file.");
		return;
	}

	filemgr_format_size(entry->size, size_text, sizeof(size_text));
	if (filemgr_buffer_is_text(buf, (uint32_t)read_len)) {
		filemgr_build_text_preview(buf, (uint32_t)read_len, entry->size);
		preview_kind = "Text preview";
	} else {
		filemgr_build_hex_preview(buf, (uint32_t)read_len, entry->size);
	}

	snprintf(meta, sizeof(meta), "%s - %s", size_text, preview_kind);
	strncpy(g_files.selected_path, g_files.entry_paths[index], sizeof(g_files.selected_path) - 1U);
	g_files.selected_path[sizeof(g_files.selected_path) - 1U] = '\0';
	g_files.has_selection = 1;
	filemgr_set_preview(entry->name, meta, g_files.entry_paths[index], g_files.preview_text);
	filemgr_set_status("Previewing", filemgr_theme()->accent_soft, filemgr_theme()->accent);
	filemgr_render_list();
}

static void filemgr_restore_ui_state(void)
{
	if (g_files.current_path[0] == '\0') {
		filemgr_set_preview("Files", "Mounted storage", FILEMGR_HOME_PATH,
				    "Choose a folder to browse the system image, ramdisk, and other mounted trees.");
		filemgr_set_status("Ready", filemgr_theme()->accent_soft, filemgr_theme()->accent);
		return;
	}

	filemgr_update_toolbar();
	filemgr_render_list();
	if (g_files.preview_title[0] != '\0') {
		filemgr_set_preview(g_files.preview_title, g_files.preview_meta,
				    g_files.preview_path, g_files.preview_text);
	} else {
		filemgr_show_directory_overview();
	}
	if (g_files.status_text[0] != '\0') {
		filemgr_set_status(g_files.status_text, g_files.status_bg, g_files.status_fg);
	} else {
		filemgr_set_status("Ready", filemgr_theme()->accent_soft, filemgr_theme()->accent);
	}
}

static void filemgr_rebuild_ui(void)
{
	lv_obj_t *scr = lv_scr_act();

	g_status_chip = NULL;
	g_path_label = NULL;
	g_summary_label = NULL;
	g_count_label = NULL;
	g_list_container = NULL;
	g_detail_title = NULL;
	g_detail_meta = NULL;
	g_detail_path = NULL;
	g_detail_preview_panel = NULL;
	g_detail_preview_label = NULL;
	lv_obj_clean(scr);
	create_ui();
	filemgr_restore_ui_state();
}

static void filemgr_refresh_theme(uint64_t mono_ms, uint8_t force)
{
	uint64_t revision = 0;
	uint32_t theme_mode = g_theme_mode;

	if (!force && mono_ms < g_last_theme_ms + FILEMGR_THEME_REFRESH_INTERVAL_MS) {
		return;
	}
	g_last_theme_ms = mono_ms;

	if (!filemgr_get_theme_revision(&revision)) {
		return;
	}
	if (!force && revision == g_last_theme_revision) {
		return;
	}
	if (!filemgr_read_theme_mode(&theme_mode)) {
		return;
	}
	g_last_theme_revision = revision;
	if (theme_mode == g_theme_mode) {
		return;
	}

	g_theme_mode = theme_mode;
	filemgr_rebuild_ui();
	log_info("files theme synced: mode=%u rev=%llu\n",
		 (unsigned int)g_theme_mode,
		 (unsigned long long)revision);
}

static void filemgr_cache_entry_paths(const char *base_path)
{
	for (uint32_t i = 0; i < FS_DIR_LIST_MAX; i++) {
		memset(g_files.entry_paths[i], 0, sizeof(g_files.entry_paths[i]));
	}

	if (base_path == NULL) {
		return;
	}

	for (uint32_t i = 0; i < g_files.listing.entry_count; i++) {
		(void)filemgr_join_path(base_path, g_files.listing.entries[i].name,
					g_files.entry_paths[i], sizeof(g_files.entry_paths[i]));
	}
}

static uint8_t filemgr_navigate_to(const char *path)
{
	fs_dir_list_response_s response = {0};
	const filemgr_theme_s *theme = filemgr_theme();
	char normalized[FS_PATH_MAX] = {0};
	int64_t ret = -1;

	if (g_files.fs == NULL || g_files.fs->ops.list_dir == NULL || path == NULL) {
		return 0;
	}

	filemgr_normalize_path(path, normalized, sizeof(normalized));
	log_info("navigate request: %s\n", normalized);
	ret = (int64_t)g_files.fs->ops.list_dir(g_files.fs, normalized, &response);
	if (ret < 0) {
		log_warn("list dir failed for %s\n", normalized);
		filemgr_show_error_preview("Path unavailable", normalized,
					   "This directory could not be enumerated by fsmgr.");
		return 0;
	}

	g_files.listing = response;
	filemgr_sort_entries(&g_files.listing);
	filemgr_cache_entry_paths(normalized);
	strncpy(g_files.current_path, normalized, sizeof(g_files.current_path) - 1U);
	g_files.current_path[sizeof(g_files.current_path) - 1U] = '\0';
	g_files.selected_path[0] = '\0';
	g_files.has_selection = 0;
	log_info("navigate done: %s entries=%d total=%d\n",
		 g_files.current_path,
		 g_files.listing.entry_count,
		 g_files.listing.total_count);

	filemgr_update_toolbar();
	filemgr_render_list();
	filemgr_show_directory_overview();
	filemgr_set_status("Ready", theme->accent_soft, theme->accent);
	return 1;
}

static void create_ui(void)
{
	const filemgr_theme_s *theme = filemgr_theme();
	lv_obj_t *scr = lv_scr_act();
	lv_obj_t *toolbar = NULL;
	lv_obj_t *explorer = NULL;
	lv_obj_t *detail = NULL;
	lv_obj_t *label = NULL;

	lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_bg_color(scr, lv_color_hex(theme->bg), 0);
	lv_obj_set_style_bg_grad_color(scr, lv_color_hex(theme->bg_alt), 0);
	lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, 0);

	label = lv_label_create(scr);
	lv_obj_set_pos(label, CONTENT_X, CONTENT_Y + 8);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->accent), 0);
	lv_label_set_text(label, "STORAGE");

	label = lv_label_create(scr);
	lv_obj_set_pos(label, CONTENT_X, CONTENT_Y + 34);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_32, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->text), 0);
	lv_label_set_text(label, "Files");

	label = lv_label_create(scr);
	lv_obj_set_pos(label, CONTENT_X + 168, CONTENT_Y + 44);
	lv_obj_set_width(label, CONTENT_W - 168);
	lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->dim), 0);
	lv_label_set_text(label, "Browse mounted storage and inspect file contents without leaving the desktop.");

	toolbar = create_panel(scr, CONTENT_X, CONTENT_Y + 74, CONTENT_W, TOOLBAR_H, theme->panel, theme->line);
	create_toolbar_button(toolbar, 20, "Home", FILEMGR_NAV_HOME);
	create_toolbar_button(toolbar, 144, "Up", FILEMGR_NAV_UP);
	create_toolbar_button(toolbar, 268, "Refresh", FILEMGR_NAV_REFRESH);

	g_path_label = lv_label_create(toolbar);
	lv_obj_set_pos(g_path_label, 404, 18);
	lv_obj_set_width(g_path_label, 320);
	lv_label_set_long_mode(g_path_label, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(g_path_label, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(g_path_label, lv_color_hex(theme->text), 0);
	lv_label_set_text(g_path_label, FILEMGR_HOME_PATH);

	g_summary_label = lv_label_create(toolbar);
	lv_obj_set_pos(g_summary_label, 404, 48);
	lv_obj_set_width(g_summary_label, 320);
	lv_label_set_long_mode(g_summary_label, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(g_summary_label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(g_summary_label, lv_color_hex(theme->dim), 0);
	lv_label_set_text(g_summary_label, "");

	g_status_chip = create_chip(toolbar, 760, 18, g_files.status_text[0] == '\0' ? "Ready" : g_files.status_text,
				     g_files.status_bg == 0U ? theme->accent_soft : g_files.status_bg,
				     g_files.status_fg == 0U ? theme->accent : g_files.status_fg);

	explorer = create_panel(scr, CONTENT_X, PANELS_Y, EXPLORER_W, PANELS_H, theme->panel, theme->line);
	label = lv_label_create(explorer);
	lv_obj_set_pos(label, 22, 18);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->dim), 0);
	lv_label_set_text(label, "DIRECTORY");

	label = lv_label_create(explorer);
	lv_obj_set_pos(label, 22, 40);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->text), 0);
	lv_label_set_text(label, "Explorer");

	g_count_label = lv_label_create(explorer);
	lv_obj_set_pos(g_count_label, 22, 72);
	lv_obj_set_style_text_font(g_count_label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(g_count_label, lv_color_hex(theme->dim), 0);
	lv_label_set_text(g_count_label, "");

	g_list_container = lv_obj_create(explorer);
	lv_obj_set_pos(g_list_container, EXPLORER_INNER_X, EXPLORER_INNER_Y);
	lv_obj_set_size(g_list_container, EXPLORER_INNER_W, EXPLORER_INNER_H);
	lv_obj_set_style_bg_color(g_list_container, lv_color_hex(theme->panel), 0);
	lv_obj_set_style_bg_opa(g_list_container, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(g_list_container, 0, 0);
	lv_obj_set_style_radius(g_list_container, 0, 0);
	lv_obj_set_style_pad_all(g_list_container, 0, 0);
	lv_obj_set_style_pad_row(g_list_container, 10, 0);
	lv_obj_set_style_pad_column(g_list_container, 0, 0);
	lv_obj_add_flag(g_list_container, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_scroll_dir(g_list_container, LV_DIR_VER);
	lv_obj_set_scrollbar_mode(g_list_container, LV_SCROLLBAR_MODE_AUTO);
	lv_obj_set_layout(g_list_container, LV_LAYOUT_FLEX);
	lv_obj_set_flex_flow(g_list_container, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_flex_align(g_list_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
			      LV_FLEX_ALIGN_START);

	detail = create_panel(scr, CONTENT_X + EXPLORER_W + PANEL_GAP, PANELS_Y, DETAIL_W, PANELS_H,
			     theme->panel, theme->line);
	label = lv_label_create(detail);
	lv_obj_set_pos(label, DETAIL_INNER_X, 18);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->dim), 0);
	lv_label_set_text(label, "PREVIEW");

	g_detail_title = lv_label_create(detail);
	lv_obj_set_pos(g_detail_title, DETAIL_INNER_X, 44);
	lv_obj_set_width(g_detail_title, DETAIL_PREVIEW_W);
	lv_label_set_long_mode(g_detail_title, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(g_detail_title, &lv_font_montserrat_24, 0);
	lv_obj_set_style_text_color(g_detail_title, lv_color_hex(theme->text), 0);
	lv_label_set_text(g_detail_title, "");

	g_detail_meta = lv_label_create(detail);
	lv_obj_set_pos(g_detail_meta, DETAIL_INNER_X, 88);
	lv_obj_set_width(g_detail_meta, DETAIL_PREVIEW_W);
	lv_label_set_long_mode(g_detail_meta, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(g_detail_meta, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(g_detail_meta, lv_color_hex(theme->accent), 0);
	lv_label_set_text(g_detail_meta, "");

	g_detail_path = lv_label_create(detail);
	lv_obj_set_pos(g_detail_path, DETAIL_INNER_X, 112);
	lv_obj_set_width(g_detail_path, DETAIL_PREVIEW_W);
	lv_label_set_long_mode(g_detail_path, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(g_detail_path, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(g_detail_path, lv_color_hex(theme->dim), 0);
	lv_label_set_text(g_detail_path, "");

	g_detail_preview_panel = create_panel(detail, DETAIL_INNER_X, DETAIL_PREVIEW_Y,
					     DETAIL_PREVIEW_W, DETAIL_PREVIEW_H,
					     theme->panel_alt, theme->line);
	lv_obj_add_flag(g_detail_preview_panel, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_scroll_dir(g_detail_preview_panel, LV_DIR_VER);
	lv_obj_set_scrollbar_mode(g_detail_preview_panel, LV_SCROLLBAR_MODE_AUTO);
	lv_obj_set_style_pad_all(g_detail_preview_panel, 18, 0);

	g_detail_preview_label = lv_label_create(g_detail_preview_panel);
	lv_obj_set_width(g_detail_preview_label, DETAIL_PREVIEW_W - 36);
	lv_label_set_long_mode(g_detail_preview_label, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(g_detail_preview_label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(g_detail_preview_label, lv_color_hex(theme->text), 0);
	lv_label_set_text(g_detail_preview_label, "");
}

static void filemgr_on_create(app_s *app)
{
	(void)app;

	g_files.fs = fs_client_get();
	g_files.systemd = systemd_client_get();
	if (filemgr_get_theme_revision(&g_last_theme_revision)) {
		(void)filemgr_read_theme_mode(&g_theme_mode);
	}
	if (g_files.systemd != NULL) {
		g_files.preview_shm = g_files.systemd->ops.alloc_shm(g_files.systemd, FILE_PREVIEW_READ_MAX);
	}

	create_ui();
	if (!filemgr_navigate_to(FILEMGR_HOME_PATH)) {
		filemgr_set_status("Limited", filemgr_theme()->warm_soft, filemgr_theme()->warm);
	}
	log_info("files ready\n");
}

static void filemgr_on_update(app_s *app, uint64_t mono_ms)
{
	(void)app;
	filemgr_refresh_theme(mono_ms, 0U);
}

static void filemgr_on_destroy(app_s *app)
{
	(void)app;

	if (g_files.systemd != NULL && g_files.preview_shm != 0U) {
		(void)g_files.systemd->ops.free_shm(g_files.systemd, g_files.preview_shm);
		g_files.preview_shm = 0;
	}
}

int main(int argc, char *argv[])
{
	app_s app = {0};
	app_config_s config = {
		.name = "files",
		.x = APP_DEFAULT_X,
		.y = APP_DEFAULT_Y,
		.z = 0,
		.width = APP_DEFAULT_WIDTH,
		.height = APP_DEFAULT_HEIGHT,
		.window_flags = WINDOW_FLAG_FOCUSABLE,
		.enable_input = 1,
	};
	app_lifecycle_ops_s lifecycle = {0};

	(void)argc;
	(void)argv;

	log_info("files start!\n");
	lifecycle.on_create = filemgr_on_create;
	lifecycle.on_update = filemgr_on_update;
	lifecycle.on_destroy = filemgr_on_destroy;
	app_init(&app, &config, &lifecycle, NULL);
	return app_run(&app);
}
