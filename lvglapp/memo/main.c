#include "lvgl.h"
#include "app.h"
#include "libwindow/window.h"
#include "log.h"
#include "libsystem/statemgr_client.h"
#include "stdio.h"
#include "string.h"
#include "stdint.h"

#define SYSTEM_OVERLAY_GAP 24
#define CONTENT_X 40
#define CONTENT_Y SYSTEM_OVERLAY_GAP
#define CONTENT_W 944
#define SUMMARY_H 136
#define BODY_Y (CONTENT_Y + SUMMARY_H + 16)
#define BODY_H (APP_DEFAULT_HEIGHT - BODY_Y - SYSTEM_OVERLAY_GAP)
#define PANEL_GAP 16
#define COMPOSER_W 280
#define TASKS_W 392
#define TASK_EXPANDED_W (CONTENT_W - COMPOSER_W - PANEL_GAP)
#define NOTES_W (CONTENT_W - COMPOSER_W - TASKS_W - (PANEL_GAP * 2))
#define PANEL_INNER_X 18
#define PANEL_LIST_Y 108
#define PANEL_LIST_H (BODY_H - PANEL_LIST_Y - 18)
#define TASK_ROW_H 94
#define NOTE_CARD_H 132
#define METRIC_CARD_W 106
#define METRIC_CARD_H 92
#define THEME_REFRESH_INTERVAL_MS 100ULL

#define MEMO_TASK_MAX 6U
#define MEMO_NOTE_MAX 4U
#define MEMO_TEXT_MAX STATEMGR_STRING_MAX

#define COLOR_BG_LIGHT          0xfff3ea
#define COLOR_BG_ALT_LIGHT      0xeaf5ee
#define COLOR_PANEL_LIGHT       0xfffffb
#define COLOR_PANEL_ALT_LIGHT   0xfff2e6
#define COLOR_LINE_LIGHT        0xf0ddd2
#define COLOR_TEXT_LIGHT        0x2d2940
#define COLOR_DIM_LIGHT         0x7c7a8f
#define COLOR_ACCENT_LIGHT      0xff7d5c
#define COLOR_ACCENT_SOFT_LIGHT 0xffe6dd
#define COLOR_WARM_LIGHT        0xffb85e
#define COLOR_WARM_SOFT_LIGHT   0xfff0d2
#define COLOR_NOTE_LIGHT        0xfff6c8
#define COLOR_NOTE_ALT_LIGHT    0xffefd9

typedef enum memo_theme_mode {
	MEMO_THEME_LIGHT = 0,
	MEMO_THEME_DARK = 1,
} memo_theme_mode_e;

typedef enum memo_action {
	MEMO_ACTION_ADD_TASK = 1,
	MEMO_ACTION_ADD_NOTE = 2,
	MEMO_ACTION_CLEAR_INPUT = 3,
} memo_action_e;

typedef struct memo_theme {
	uint32_t bg;
	uint32_t bg_alt;
	uint32_t panel;
	uint32_t panel_alt;
	uint32_t line;
	uint32_t text;
	uint32_t dim;
	uint32_t accent;
	uint32_t accent_soft;
	uint32_t warm;
	uint32_t warm_soft;
	uint32_t note;
	uint32_t note_alt;
	uint32_t action_text;
} memo_theme_s;

typedef struct memo_task {
	char text[MEMO_TEXT_MAX];
	uint8_t done;
} memo_task_s;

typedef struct memo_state {
	memo_task_s tasks[MEMO_TASK_MAX];
	char notes[MEMO_NOTE_MAX][MEMO_TEXT_MAX];
	char draft[MEMO_TEXT_MAX];
	char status_text[32];
	uint32_t status_bg;
	uint32_t status_fg;
	uint8_t persistence_ready;
} memo_state_s;

static memo_state_s g_memo = {0};
static statemgr_client_s *g_statemgr = NULL;
static uint32_t g_theme_mode = MEMO_THEME_LIGHT;
static uint64_t g_last_theme_ms = 0;
static uint64_t g_last_theme_revision = 0;

static lv_obj_t *g_status_chip = NULL;
static lv_obj_t *g_input_ta = NULL;
static lv_obj_t *g_task_summary = NULL;
static lv_obj_t *g_note_summary = NULL;
static lv_obj_t *g_metric_values[3] = {0};
static lv_obj_t *g_task_list = NULL;
static lv_obj_t *g_note_list = NULL;

static const memo_theme_s g_memo_theme_light = {
	.bg = COLOR_BG_LIGHT,
	.bg_alt = COLOR_BG_ALT_LIGHT,
	.panel = COLOR_PANEL_LIGHT,
	.panel_alt = COLOR_PANEL_ALT_LIGHT,
	.line = COLOR_LINE_LIGHT,
	.text = COLOR_TEXT_LIGHT,
	.dim = COLOR_DIM_LIGHT,
	.accent = COLOR_ACCENT_LIGHT,
	.accent_soft = COLOR_ACCENT_SOFT_LIGHT,
	.warm = COLOR_WARM_LIGHT,
	.warm_soft = COLOR_WARM_SOFT_LIGHT,
	.note = COLOR_NOTE_LIGHT,
	.note_alt = COLOR_NOTE_ALT_LIGHT,
	.action_text = 0x182018,
};

static const memo_theme_s g_memo_theme_dark = {
	.bg = 0x171828,
	.bg_alt = 0x222b39,
	.panel = 0x20243a,
	.panel_alt = 0x2a3048,
	.line = 0x46506f,
	.text = 0xfff6ee,
	.dim = 0xc6b5ad,
	.accent = 0xffa07c,
	.accent_soft = 0x4a2f29,
	.warm = 0xf5c575,
	.warm_soft = 0x4b3928,
	.note = 0x5a4a30,
	.note_alt = 0x43352d,
	.action_text = 0x1f2434,
};

#define MEMO_ACTION_TASK_TOGGLE_BASE 100U
#define MEMO_ACTION_TASK_DELETE_BASE 200U
#define MEMO_ACTION_NOTE_DELETE_BASE 300U

static const memo_theme_s *memo_theme(void)
{
	return g_theme_mode == MEMO_THEME_DARK ?
		&g_memo_theme_dark : &g_memo_theme_light;
}

static void memo_render_task_list(void);
static void memo_render_note_list(void);
static void memo_refresh_dynamic_ui(void);
static void memo_rebuild_ui(void);
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

static lv_obj_t *create_metric_card(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
				    const char *title)
{
	const memo_theme_s *theme = memo_theme();
	lv_obj_t *card = create_panel(parent, x, y, METRIC_CARD_W, METRIC_CARD_H,
				      theme->panel_alt, theme->line);
	lv_obj_t *title_label = lv_label_create(card);
	lv_obj_t *value_label = lv_label_create(card);

	lv_obj_set_pos(title_label, 14, 14);
	lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(title_label, lv_color_hex(theme->dim), 0);
	lv_label_set_text(title_label, title);

	lv_obj_set_pos(value_label, 14, 42);
	lv_obj_set_width(value_label, METRIC_CARD_W - 28);
	lv_obj_set_style_text_font(value_label, &lv_font_montserrat_24, 0);
	lv_obj_set_style_text_color(value_label, lv_color_hex(theme->text), 0);
	lv_label_set_text(value_label, "0");
	return value_label;
}

static lv_obj_t *create_action_button(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
				      lv_coord_t w, lv_coord_t h, const char *text,
				      uint32_t bg, uint32_t border, uint32_t fg)
{
	lv_obj_t *btn = lv_btn_create(parent);
	lv_obj_t *label = lv_label_create(btn);

	lv_obj_set_pos(btn, x, y);
	lv_obj_set_size(btn, w, h);
	lv_obj_set_style_radius(btn, 18, 0);
	lv_obj_set_style_bg_color(btn, lv_color_hex(bg), 0);
	lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
	lv_obj_set_style_border_color(btn, lv_color_hex(border), 0);
	lv_obj_set_style_border_width(btn, 1, 0);
	lv_obj_set_style_shadow_width(btn, 0, 0);
	lv_obj_set_style_pad_all(btn, 0, 0);
	lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

	lv_obj_center(label);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(fg), 0);
	lv_label_set_text(label, text);
	return btn;
}

static void style_list_container(lv_obj_t *obj)
{
	const memo_theme_s *theme = memo_theme();

	if (obj == NULL) {
		return;
	}

	lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(obj, 0, 0);
	lv_obj_set_style_radius(obj, 0, 0);
	lv_obj_set_style_pad_all(obj, 0, 0);
	lv_obj_set_style_pad_row(obj, 10, 0);
	lv_obj_set_style_pad_column(obj, 0, 0);
	lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_scroll_dir(obj, LV_DIR_VER);
	lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_AUTO);
	lv_obj_set_layout(obj, LV_LAYOUT_FLEX);
	lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
			      LV_FLEX_ALIGN_START);
	lv_obj_set_style_width(obj, 6, LV_PART_SCROLLBAR);
	lv_obj_set_style_bg_color(obj, lv_color_hex(theme->warm), LV_PART_SCROLLBAR);
	lv_obj_set_style_bg_opa(obj, LV_OPA_40, LV_PART_SCROLLBAR);
	lv_obj_set_style_radius(obj, 999, LV_PART_SCROLLBAR);
	lv_obj_set_style_pad_right(obj, 6, LV_PART_SCROLLBAR);
	lv_obj_set_style_pad_top(obj, 8, LV_PART_SCROLLBAR);
	lv_obj_set_style_pad_bottom(obj, 8, LV_PART_SCROLLBAR);
}

static uint32_t memo_open_task_count(void)
{
	uint32_t count = 0;

	for (uint32_t i = 0; i < MEMO_TASK_MAX; i++) {
		if (g_memo.tasks[i].text[0] != '\0' && !g_memo.tasks[i].done) {
			count++;
		}
	}
	return count;
}

static uint32_t memo_done_task_count(void)
{
	uint32_t count = 0;

	for (uint32_t i = 0; i < MEMO_TASK_MAX; i++) {
		if (g_memo.tasks[i].text[0] != '\0' && g_memo.tasks[i].done) {
			count++;
		}
	}
	return count;
}

static uint32_t memo_note_count(void)
{
	uint32_t count = 0;

	for (uint32_t i = 0; i < MEMO_NOTE_MAX; i++) {
		if (g_memo.notes[i][0] != '\0') {
			count++;
		}
	}
	return count;
}

static char *memo_trim_inplace(char *text)
{
	char *start = text;
	char *end = NULL;

	if (text == NULL) {
		return NULL;
	}

	while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') {
		start++;
	}

	end = start + strlen(start);
	while (end > start &&
	       (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) {
		end--;
	}
	*end = '\0';

	if (start != text) {
		memmove(text, start, strlen(start) + 1U);
	}
	return text;
}

static void memo_capture_draft_from_input(void)
{
	const char *text = NULL;

	if (g_input_ta == NULL) {
		return;
	}

	text = lv_textarea_get_text(g_input_ta);
	if (text == NULL) {
		g_memo.draft[0] = '\0';
		return;
	}

	strncpy(g_memo.draft, text, sizeof(g_memo.draft) - 1U);
	g_memo.draft[sizeof(g_memo.draft) - 1U] = '\0';
}

static uint8_t memo_copy_trimmed_input(char *out, uint32_t cap)
{
	char buffer[MEMO_TEXT_MAX] = {0};
	const char *src = NULL;

	if (out == NULL || cap == 0U) {
		return 0U;
	}

	src = g_input_ta != NULL ? lv_textarea_get_text(g_input_ta) : g_memo.draft;
	if (src == NULL) {
		return 0U;
	}

	strncpy(buffer, src, sizeof(buffer) - 1U);
	buffer[sizeof(buffer) - 1U] = '\0';
	memo_trim_inplace(buffer);
	if (buffer[0] == '\0') {
		return 0U;
	}

	strncpy(out, buffer, cap - 1U);
	out[cap - 1U] = '\0';
	return 1U;
}

static void memo_make_task_text_key(uint32_t index, char *out, uint32_t cap)
{
	snprintf(out, cap, "memo.task%u.text", (unsigned int)index);
}

static void memo_make_task_done_key(uint32_t index, char *out, uint32_t cap)
{
	snprintf(out, cap, "memo.task%u.done", (unsigned int)index);
}

static void memo_make_note_key(uint32_t index, char *out, uint32_t cap)
{
	snprintf(out, cap, "memo.note%u.text", (unsigned int)index);
}

static uint8_t memo_read_state_string(const char *key, char *out, uint32_t cap)
{
	statemgr_get_response_s response = {0};

	if (out == NULL || cap == 0U) {
		return 0U;
	}
	out[0] = '\0';
	if (g_statemgr == NULL || g_statemgr->ops.get == NULL || key == NULL) {
		return 0U;
	}
	if (g_statemgr->ops.get(g_statemgr, key, &response) == 0U ||
	    !response.found || response.entry.type != STATEMGR_VALUE_TYPE_STRING) {
		return 0U;
	}

	strncpy(out, response.entry.string_value, cap - 1U);
	out[cap - 1U] = '\0';
	return out[0] != '\0';
}

static uint8_t memo_read_state_bool(const char *key, uint8_t *value_out)
{
	statemgr_get_response_s response = {0};

	if (value_out == NULL) {
		return 0U;
	}
	*value_out = 0U;
	if (g_statemgr == NULL || g_statemgr->ops.get == NULL || key == NULL) {
		return 0U;
	}
	if (g_statemgr->ops.get(g_statemgr, key, &response) == 0U || !response.found) {
		return 0U;
	}
	if (response.entry.type != STATEMGR_VALUE_TYPE_BOOL &&
	    response.entry.type != STATEMGR_VALUE_TYPE_U32) {
		return 0U;
	}

	*value_out = response.entry.value_u64 != 0U ? 1U : 0U;
	return 1U;
}

static uint8_t memo_write_state_string(const char *key, const char *value)
{
	statemgr_entry_s entry = {0};

	if (g_statemgr == NULL || g_statemgr->ops.set == NULL || key == NULL || value == NULL) {
		return 0U;
	}

	strncpy(entry.key, key, sizeof(entry.key) - 1U);
	entry.type = STATEMGR_VALUE_TYPE_STRING;
	strncpy(entry.string_value, value, sizeof(entry.string_value) - 1U);
	return g_statemgr->ops.set(g_statemgr, &entry) != 0U ? 1U : 0U;
}

static uint8_t memo_write_state_bool(const char *key, uint8_t value)
{
	statemgr_entry_s entry = {0};

	if (g_statemgr == NULL || g_statemgr->ops.set == NULL || key == NULL) {
		return 0U;
	}

	strncpy(entry.key, key, sizeof(entry.key) - 1U);
	entry.type = STATEMGR_VALUE_TYPE_BOOL;
	entry.value_u64 = value != 0U ? 1U : 0U;
	return g_statemgr->ops.set(g_statemgr, &entry) != 0U ? 1U : 0U;
}

static void memo_compact_tasks(void)
{
	uint32_t write_index = 0;

	for (uint32_t i = 0; i < MEMO_TASK_MAX; i++) {
		if (g_memo.tasks[i].text[0] == '\0') {
			continue;
		}
		if (write_index != i) {
			memcpy(&g_memo.tasks[write_index], &g_memo.tasks[i], sizeof(g_memo.tasks[i]));
			memset(&g_memo.tasks[i], 0, sizeof(g_memo.tasks[i]));
		}
		write_index++;
	}

	for (; write_index < MEMO_TASK_MAX; write_index++) {
		memset(&g_memo.tasks[write_index], 0, sizeof(g_memo.tasks[write_index]));
	}
}

static void memo_compact_notes(void)
{
	uint32_t write_index = 0;

	for (uint32_t i = 0; i < MEMO_NOTE_MAX; i++) {
		if (g_memo.notes[i][0] == '\0') {
			continue;
		}
		if (write_index != i) {
			strncpy(g_memo.notes[write_index], g_memo.notes[i], MEMO_TEXT_MAX - 1U);
			g_memo.notes[write_index][MEMO_TEXT_MAX - 1U] = '\0';
			memset(g_memo.notes[i], 0, sizeof(g_memo.notes[i]));
		}
		write_index++;
	}

	for (; write_index < MEMO_NOTE_MAX; write_index++) {
		memset(g_memo.notes[write_index], 0, sizeof(g_memo.notes[write_index]));
	}
}

static void memo_load_state(void)
{
	char key[STATEMGR_KEY_MAX] = {0};

	memset(g_memo.tasks, 0, sizeof(g_memo.tasks));
	memset(g_memo.notes, 0, sizeof(g_memo.notes));
	g_memo.persistence_ready = g_statemgr != NULL &&
				   g_statemgr->ops.get != NULL &&
				   g_statemgr->ops.set != NULL;

	if (!g_memo.persistence_ready) {
		return;
	}

	for (uint32_t i = 0; i < MEMO_TASK_MAX; i++) {
		memo_make_task_text_key(i, key, sizeof(key));
		(void)memo_read_state_string(key, g_memo.tasks[i].text, sizeof(g_memo.tasks[i].text));
		memo_make_task_done_key(i, key, sizeof(key));
		(void)memo_read_state_bool(key, &g_memo.tasks[i].done);
	}

	for (uint32_t i = 0; i < MEMO_NOTE_MAX; i++) {
		memo_make_note_key(i, key, sizeof(key));
		(void)memo_read_state_string(key, g_memo.notes[i], sizeof(g_memo.notes[i]));
	}

	memo_compact_tasks();
	memo_compact_notes();
}

static uint8_t memo_save_state(void)
{
	char key[STATEMGR_KEY_MAX] = {0};
	uint8_t all_ok = 1U;

	if (!g_memo.persistence_ready) {
		return 0U;
	}

	for (uint32_t i = 0; i < MEMO_TASK_MAX; i++) {
		memo_make_task_text_key(i, key, sizeof(key));
		if (!memo_write_state_string(key, g_memo.tasks[i].text)) {
			all_ok = 0U;
		}
		memo_make_task_done_key(i, key, sizeof(key));
		if (!memo_write_state_bool(key, g_memo.tasks[i].text[0] != '\0' ? g_memo.tasks[i].done : 0U)) {
			all_ok = 0U;
		}
	}

	for (uint32_t i = 0; i < MEMO_NOTE_MAX; i++) {
		memo_make_note_key(i, key, sizeof(key));
		if (!memo_write_state_string(key, g_memo.notes[i])) {
			all_ok = 0U;
		}
	}

	return all_ok;
}

static void memo_set_status(const char *text, uint32_t bg, uint32_t fg)
{
	if (text == NULL) {
		g_memo.status_text[0] = '\0';
	} else {
		strncpy(g_memo.status_text, text, sizeof(g_memo.status_text) - 1U);
		g_memo.status_text[sizeof(g_memo.status_text) - 1U] = '\0';
	}
	g_memo.status_bg = bg;
	g_memo.status_fg = fg;

	if (g_status_chip != NULL) {
		lv_obj_set_style_bg_color(g_status_chip, lv_color_hex(bg), 0);
		lv_obj_set_style_text_color(g_status_chip, lv_color_hex(fg), 0);
		lv_label_set_text(g_status_chip, g_memo.status_text);
	}
}

static void memo_set_default_status(void)
{
	const memo_theme_s *theme = memo_theme();

	if (g_memo.persistence_ready) {
		memo_set_status("Session only", theme->accent_soft, theme->accent);
	} else {
		memo_set_status("Volatile", theme->warm_soft, theme->warm);
	}
}

static uint8_t memo_get_theme_revision(uint64_t *revision_out)
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

static uint8_t memo_read_theme_mode(uint32_t *theme_mode_out)
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

	*theme_mode_out = (uint32_t)response.entry.value_u64 == MEMO_THEME_DARK ?
		MEMO_THEME_DARK : MEMO_THEME_LIGHT;
	return 1U;
}

static void memo_create_background(lv_obj_t *scr)
{
	const memo_theme_s *theme = memo_theme();
	lv_obj_t *shape = NULL;

	lv_obj_set_style_bg_color(scr, lv_color_hex(theme->bg), 0);
	lv_obj_set_style_bg_grad_color(scr, lv_color_hex(theme->bg_alt), 0);
	lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, 0);
	lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
	lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

	shape = lv_obj_create(scr);
	lv_obj_set_pos(shape, -42, -28);
	lv_obj_set_size(shape, 220, 220);
	lv_obj_set_style_bg_color(shape, lv_color_hex(theme->warm), 0);
	lv_obj_set_style_bg_opa(shape, LV_OPA_10, 0);
	lv_obj_set_style_border_width(shape, 0, 0);
	lv_obj_set_style_radius(shape, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_shadow_width(shape, 0, 0);
	clear_static_flags(shape);

	shape = lv_obj_create(scr);
	lv_obj_set_pos(shape, APP_DEFAULT_WIDTH - 196, APP_DEFAULT_HEIGHT - 172);
	lv_obj_set_size(shape, 240, 240);
	lv_obj_set_style_bg_color(shape, lv_color_hex(theme->accent), 0);
	lv_obj_set_style_bg_opa(shape, 18, 0);
	lv_obj_set_style_border_width(shape, 0, 0);
	lv_obj_set_style_radius(shape, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_shadow_width(shape, 0, 0);
	clear_static_flags(shape);
}

static void memo_update_metrics(void)
{
	char text[32] = {0};
	uint32_t open_count = memo_open_task_count();
	uint32_t done_count = memo_done_task_count();
	uint32_t total_count = open_count + done_count;

	if (g_metric_values[0] != NULL) {
		snprintf(text, sizeof(text), "%u", (unsigned int)open_count);
		lv_label_set_text(g_metric_values[0], text);
	}
	if (g_metric_values[1] != NULL) {
		snprintf(text, sizeof(text), "%u", (unsigned int)done_count);
		lv_label_set_text(g_metric_values[1], text);
	}
	if (g_metric_values[2] != NULL) {
		snprintf(text, sizeof(text), "%u", (unsigned int)total_count);
		lv_label_set_text(g_metric_values[2], text);
	}
	if (g_task_summary != NULL) {
		char summary[96] = {0};
		snprintf(summary, sizeof(summary), "%u open, %u done. Keep the rail short and current.",
			 (unsigned int)open_count, (unsigned int)done_count);
		lv_label_set_text(g_task_summary, summary);
	}
}

static void memo_refresh_dynamic_ui(void)
{
	memo_update_metrics();
	memo_render_task_list();
	memo_render_note_list();
	if (g_status_chip != NULL && g_memo.status_text[0] != '\0') {
		lv_obj_set_style_bg_color(g_status_chip, lv_color_hex(g_memo.status_bg), 0);
		lv_obj_set_style_text_color(g_status_chip, lv_color_hex(g_memo.status_fg), 0);
		lv_label_set_text(g_status_chip, g_memo.status_text);
	}
}

static uint8_t memo_add_task(const char *text)
{
	for (uint32_t i = 0; i < MEMO_TASK_MAX; i++) {
		if (g_memo.tasks[i].text[0] != '\0') {
			continue;
		}
		strncpy(g_memo.tasks[i].text, text, sizeof(g_memo.tasks[i].text) - 1U);
		g_memo.tasks[i].text[sizeof(g_memo.tasks[i].text) - 1U] = '\0';
		g_memo.tasks[i].done = 0U;
		return 1U;
	}

	return 0U;
}

static uint8_t memo_add_note(const char *text)
{
	for (uint32_t i = 0; i < MEMO_NOTE_MAX; i++) {
		if (g_memo.notes[i][0] != '\0') {
			continue;
		}
		strncpy(g_memo.notes[i], text, sizeof(g_memo.notes[i]) - 1U);
		g_memo.notes[i][sizeof(g_memo.notes[i]) - 1U] = '\0';
		return 1U;
	}

	return 0U;
}

static void memo_focus_input(void)
{
	if (g_input_ta != NULL) {
		lv_group_focus_obj(g_input_ta);
	}
}

static void memo_finish_edit(const char *status, uint32_t bg, uint32_t fg)
{
	if (!memo_save_state()) {
		memo_set_status("Live only", memo_theme()->warm_soft, memo_theme()->warm);
	} else {
		memo_set_status(status, bg, fg);
	}
	memo_refresh_dynamic_ui();
	memo_focus_input();
}

static void memo_add_from_input(uint8_t as_note)
{
	char text[MEMO_TEXT_MAX] = {0};
	const memo_theme_s *theme = memo_theme();

	if (!memo_copy_trimmed_input(text, sizeof(text))) {
		memo_set_status("Type first", theme->warm_soft, theme->warm);
		return;
	}

	if (as_note) {
		if (!memo_add_note(text)) {
			memo_set_status("Note stack full", theme->warm_soft, theme->warm);
			return;
		}
	} else {
		if (!memo_add_task(text)) {
			memo_set_status("Task rail full", theme->warm_soft, theme->warm);
			return;
		}
	}

	g_memo.draft[0] = '\0';
	if (g_input_ta != NULL) {
		lv_textarea_set_text(g_input_ta, "");
	}
	memo_finish_edit(as_note ? "Note pinned" : "Task added",
			 as_note ? theme->warm_soft : theme->accent_soft,
			 as_note ? theme->warm : theme->accent);
	log_info("memo: %s added\n", as_note ? "note" : "task");
}

static void memo_remove_task(uint32_t index)
{
	if (index >= MEMO_TASK_MAX || g_memo.tasks[index].text[0] == '\0') {
		return;
	}

	memset(&g_memo.tasks[index], 0, sizeof(g_memo.tasks[index]));
	memo_compact_tasks();
	memo_finish_edit("Task cleared", memo_theme()->panel_alt, memo_theme()->dim);
}

static void memo_toggle_task(uint32_t index)
{
	if (index >= MEMO_TASK_MAX || g_memo.tasks[index].text[0] == '\0') {
		return;
	}

	g_memo.tasks[index].done = g_memo.tasks[index].done ? 0U : 1U;
	memo_finish_edit(g_memo.tasks[index].done ? "Marked done" : "Marked open",
			 g_memo.tasks[index].done ? memo_theme()->warm_soft : memo_theme()->accent_soft,
			 g_memo.tasks[index].done ? memo_theme()->warm : memo_theme()->accent);
}

static void memo_remove_note(uint32_t index)
{
	if (index >= MEMO_NOTE_MAX || g_memo.notes[index][0] == '\0') {
		return;
	}

	memset(g_memo.notes[index], 0, sizeof(g_memo.notes[index]));
	memo_compact_notes();
	memo_finish_edit("Note cleared", memo_theme()->panel_alt, memo_theme()->dim);
}

static void memo_action_event_cb(lv_event_t *e)
{
	uintptr_t action = 0;

	if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
		return;
	}

	action = (uintptr_t)lv_event_get_user_data(e);
	if (action == MEMO_ACTION_ADD_TASK) {
		memo_add_from_input(0U);
		return;
	}
	if (action == MEMO_ACTION_ADD_NOTE) {
		memo_add_from_input(1U);
		return;
	}
	if (action == MEMO_ACTION_CLEAR_INPUT) {
		g_memo.draft[0] = '\0';
		if (g_input_ta != NULL) {
			lv_textarea_set_text(g_input_ta, "");
		}
		memo_set_default_status();
		memo_focus_input();
		return;
	}
	if (action >= MEMO_ACTION_TASK_TOGGLE_BASE &&
	    action < MEMO_ACTION_TASK_TOGGLE_BASE + MEMO_TASK_MAX) {
		memo_toggle_task((uint32_t)(action - MEMO_ACTION_TASK_TOGGLE_BASE));
		return;
	}
	if (action >= MEMO_ACTION_TASK_DELETE_BASE &&
	    action < MEMO_ACTION_TASK_DELETE_BASE + MEMO_TASK_MAX) {
		memo_remove_task((uint32_t)(action - MEMO_ACTION_TASK_DELETE_BASE));
		return;
	}
	if (action >= MEMO_ACTION_NOTE_DELETE_BASE &&
	    action < MEMO_ACTION_NOTE_DELETE_BASE + MEMO_NOTE_MAX) {
		memo_remove_note((uint32_t)(action - MEMO_ACTION_NOTE_DELETE_BASE));
	}
}

static void memo_input_ready_event_cb(lv_event_t *e)
{
	if (lv_event_get_code(e) != LV_EVENT_READY) {
		return;
	}

	memo_add_from_input(0U);
}

static void memo_render_task_list(void)
{
	const memo_theme_s *theme = memo_theme();
	lv_coord_t scroll_y = 0;
	lv_coord_t row_w = TASK_EXPANDED_W - (PANEL_INNER_X * 2);

	if (g_task_list == NULL) {
		return;
	}

	scroll_y = lv_obj_get_scroll_y(g_task_list);
	lv_obj_clean(g_task_list);
	if (memo_open_task_count() + memo_done_task_count() == 0U) {
		lv_obj_t *empty = lv_label_create(g_task_list);

		lv_obj_set_width(empty, row_w - 8);
		lv_label_set_long_mode(empty, LV_LABEL_LONG_WRAP);
		lv_obj_set_style_text_font(empty, &lv_font_montserrat_16, 0);
		lv_obj_set_style_text_color(empty, lv_color_hex(theme->dim), 0);
		lv_label_set_text(empty, "No tasks yet. Add one from the inbox to start the rail.");
		if (scroll_y != 0) {
			lv_obj_scroll_to_y(g_task_list, scroll_y, LV_ANIM_OFF);
		}
		return;
	}

	for (uint32_t i = 0; i < MEMO_TASK_MAX; i++) {
		lv_obj_t *row = NULL;
		lv_obj_t *title = NULL;
		lv_obj_t *meta = NULL;
		lv_obj_t *toggle_btn = NULL;
		lv_obj_t *delete_btn = NULL;
		lv_coord_t text_w = row_w - 190;
		lv_coord_t meta_y = 0;
		lv_coord_t row_h = TASK_ROW_H;
		char meta_text[48] = {0};

		if (g_memo.tasks[i].text[0] == '\0') {
			continue;
		}

		row = lv_obj_create(g_task_list);
		lv_obj_set_size(row, row_w, TASK_ROW_H);
		lv_obj_set_style_bg_color(row, lv_color_hex(g_memo.tasks[i].done ?
							 theme->panel_alt : theme->panel), 0);
		lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
		lv_obj_set_style_border_color(row, lv_color_hex(g_memo.tasks[i].done ?
								 theme->warm : theme->line), 0);
		lv_obj_set_style_border_width(row, g_memo.tasks[i].done ? 2 : 1, 0);
		lv_obj_set_style_radius(row, 22, 0);
		lv_obj_set_style_shadow_width(row, 0, 0);
		lv_obj_set_style_pad_all(row, 0, 0);
		clear_static_flags(row);

		(void)create_chip(row, 14, 12,
				  g_memo.tasks[i].done ? "DONE" : "OPEN",
				  g_memo.tasks[i].done ? theme->warm_soft : theme->accent_soft,
				  g_memo.tasks[i].done ? theme->warm : theme->accent);

		title = lv_label_create(row);
		lv_obj_set_pos(title, 14, 44);
		lv_obj_set_width(title, text_w);
		lv_label_set_long_mode(title, LV_LABEL_LONG_WRAP);
		lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
		lv_obj_set_style_text_color(title, lv_color_hex(g_memo.tasks[i].done ?
								 theme->dim : theme->text), 0);
		lv_label_set_text(title, g_memo.tasks[i].text);
		lv_obj_update_layout(title);

		meta = lv_label_create(row);
		meta_y = (lv_coord_t)(lv_obj_get_y(title) + lv_obj_get_height(title) + 8);
		lv_obj_set_pos(meta, 14, meta_y);
		lv_obj_set_width(meta, text_w);
		lv_label_set_long_mode(meta, LV_LABEL_LONG_WRAP);
		lv_obj_set_style_text_font(meta, &lv_font_montserrat_14, 0);
		lv_obj_set_style_text_color(meta, lv_color_hex(theme->dim), 0);
		snprintf(meta_text, sizeof(meta_text), "Task %u  -  %s",
			 (unsigned int)(i + 1U), g_memo.tasks[i].done ? "parked" : "active");
		lv_label_set_text(meta, meta_text);
		lv_obj_update_layout(meta);

		row_h = (lv_coord_t)(meta_y + lv_obj_get_height(meta) + 16);
		if (row_h < TASK_ROW_H) {
			row_h = TASK_ROW_H;
		}
		lv_obj_set_height(row, row_h);

		toggle_btn = create_action_button(row, row_w - 138, 24, 62, 34,
						 g_memo.tasks[i].done ? "Undo" : "Done",
						 g_memo.tasks[i].done ? theme->warm_soft : theme->accent_soft,
						 g_memo.tasks[i].done ? theme->warm : theme->accent,
						 g_memo.tasks[i].done ? theme->warm : theme->accent);
		lv_obj_add_event_cb(toggle_btn, memo_action_event_cb, LV_EVENT_CLICKED,
				    (void *)(uintptr_t)(MEMO_ACTION_TASK_TOGGLE_BASE + i));

		delete_btn = create_action_button(row, row_w - 70, 24, 56, 34, "Drop",
						 theme->panel_alt, theme->line, theme->dim);
		lv_obj_add_event_cb(delete_btn, memo_action_event_cb, LV_EVENT_CLICKED,
				    (void *)(uintptr_t)(MEMO_ACTION_TASK_DELETE_BASE + i));
	}

	if (scroll_y != 0) {
		lv_obj_scroll_to_y(g_task_list, scroll_y, LV_ANIM_OFF);
	}
}

static void memo_render_note_list(void)
{
	const memo_theme_s *theme = memo_theme();
	lv_coord_t scroll_y = 0;
	lv_coord_t card_w = NOTES_W - (PANEL_INNER_X * 2);

	if (g_note_list == NULL) {
		return;
	}

	scroll_y = lv_obj_get_scroll_y(g_note_list);
	lv_obj_clean(g_note_list);
	if (memo_note_count() == 0U) {
		lv_obj_t *empty = lv_label_create(g_note_list);

		lv_obj_set_width(empty, card_w - 8);
		lv_label_set_long_mode(empty, LV_LABEL_LONG_WRAP);
		lv_obj_set_style_text_font(empty, &lv_font_montserrat_16, 0);
		lv_obj_set_style_text_color(empty, lv_color_hex(theme->dim), 0);
		lv_label_set_text(empty, "No pinned notes yet. Save a short memo from the inbox.");
		if (scroll_y != 0) {
			lv_obj_scroll_to_y(g_note_list, scroll_y, LV_ANIM_OFF);
		}
		return;
	}

	for (uint32_t i = 0; i < MEMO_NOTE_MAX; i++) {
		lv_obj_t *card = NULL;
		lv_obj_t *title = NULL;
		lv_obj_t *text = NULL;
		lv_obj_t *drop_btn = NULL;

		if (g_memo.notes[i][0] == '\0') {
			continue;
		}

		card = lv_obj_create(g_note_list);
		lv_obj_set_size(card, card_w, NOTE_CARD_H);
		lv_obj_set_style_bg_color(card, lv_color_hex(i % 2U == 0U ? theme->note : theme->note_alt), 0);
		lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
		lv_obj_set_style_border_color(card, lv_color_hex(theme->line), 0);
		lv_obj_set_style_border_width(card, 1, 0);
		lv_obj_set_style_radius(card, 22, 0);
		lv_obj_set_style_shadow_width(card, 0, 0);
		lv_obj_set_style_pad_all(card, 0, 0);
		clear_static_flags(card);

		title = lv_label_create(card);
		lv_obj_set_pos(title, 14, 14);
		lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
		lv_obj_set_style_text_color(title, lv_color_hex(theme->warm), 0);
		lv_label_set_text(title, "Pinned note");

		text = lv_label_create(card);
		lv_obj_set_pos(text, 14, 40);
		lv_obj_set_width(text, card_w - 28);
		lv_label_set_long_mode(text, LV_LABEL_LONG_WRAP);
		lv_obj_set_style_text_font(text, &lv_font_montserrat_16, 0);
		lv_obj_set_style_text_color(text, lv_color_hex(theme->text), 0);
		lv_label_set_text(text, g_memo.notes[i]);

		drop_btn = create_action_button(card, card_w - 74, NOTE_CARD_H - 44, 60, 30, "Drop",
						theme->panel, theme->line, theme->dim);
		lv_obj_add_event_cb(drop_btn, memo_action_event_cb, LV_EVENT_CLICKED,
				    (void *)(uintptr_t)(MEMO_ACTION_NOTE_DELETE_BASE + i));
	}

	if (scroll_y != 0) {
		lv_obj_scroll_to_y(g_note_list, scroll_y, LV_ANIM_OFF);
	}
}

static void memo_rebuild_ui(void)
{
	memo_capture_draft_from_input();
	g_status_chip = NULL;
	g_input_ta = NULL;
	g_task_summary = NULL;
	g_note_summary = NULL;
	memset(g_metric_values, 0, sizeof(g_metric_values));
	g_task_list = NULL;
	g_note_list = NULL;
	lv_obj_clean(lv_scr_act());
	create_ui();
}

static void memo_refresh_theme(uint64_t mono_ms)
{
	uint64_t revision = 0;
	uint32_t theme_mode = g_theme_mode;

	if (mono_ms < g_last_theme_ms + THEME_REFRESH_INTERVAL_MS) {
		return;
	}
	g_last_theme_ms = mono_ms;

	if (!memo_get_theme_revision(&revision) || !memo_read_theme_mode(&theme_mode)) {
		return;
	}
	if (revision == g_last_theme_revision) {
		return;
	}
	g_last_theme_revision = revision;
	if (theme_mode == g_theme_mode) {
		return;
	}

	g_theme_mode = theme_mode;
	memo_rebuild_ui();
	log_info("memo theme synced: mode=%u rev=%llu\n",
		 (unsigned int)g_theme_mode,
		 (unsigned long long)revision);
}

static void create_ui(void)
{
	const memo_theme_s *theme = memo_theme();
	lv_obj_t *scr = lv_scr_act();
	lv_obj_t *summary = NULL;
	lv_obj_t *composer = NULL;
	lv_obj_t *tasks = NULL;
	lv_obj_t *label = NULL;
	lv_obj_t *input_shell = NULL;
	lv_obj_t *btn = NULL;

	memo_create_background(scr);

	summary = create_panel(scr, CONTENT_X, CONTENT_Y, CONTENT_W, SUMMARY_H,
			      theme->panel, theme->line);
	(void)create_chip(summary, 24, 18, "SESSION BOARD", theme->accent_soft, theme->accent);
	g_status_chip = create_chip(summary, 162, 18,
				    g_memo.status_text[0] == '\0' ? "Session only" : g_memo.status_text,
				    g_memo.status_bg == 0U ? theme->accent_soft : g_memo.status_bg,
				    g_memo.status_fg == 0U ? theme->accent : g_memo.status_fg);

	label = lv_label_create(summary);
	lv_obj_set_pos(label, 24, 54);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_32, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->text), 0);
	lv_label_set_text(label, "Memo");

	label = lv_label_create(summary);
	lv_obj_set_pos(label, 24, 90);
	lv_obj_set_width(label, 488);
	lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->dim), 0);
	lv_label_set_text(label,
			  "Capture tasks without leaving the desktop. State survives app relaunches during this boot session.");

	g_metric_values[0] = create_metric_card(summary, 584, 22, "Open");
	g_metric_values[1] = create_metric_card(summary, 698, 22, "Done");
	g_metric_values[2] = create_metric_card(summary, 812, 22, "Total");

	composer = create_panel(scr, CONTENT_X, BODY_Y, COMPOSER_W, BODY_H, theme->panel, theme->line);
	label = lv_label_create(composer);
	lv_obj_set_pos(label, 18, 18);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->accent), 0);
	lv_label_set_text(label, "INBOX");

	label = lv_label_create(composer);
	lv_obj_set_pos(label, 18, 42);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->text), 0);
	lv_label_set_text(label, "Capture");

	label = lv_label_create(composer);
	lv_obj_set_pos(label, 18, 70);
	lv_obj_set_width(label, COMPOSER_W - 36);
	lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->dim), 0);
	lv_label_set_text(label, "Enter adds a task. Keep the board simple and current.");

	input_shell = create_panel(composer, 18, 110, COMPOSER_W - 36, 84, theme->panel_alt, theme->line);
	g_input_ta = lv_textarea_create(input_shell);
	lv_obj_set_pos(g_input_ta, 12, 12);
	lv_obj_set_size(g_input_ta, COMPOSER_W - 60, 46);
	lv_obj_set_style_bg_color(g_input_ta, lv_color_hex(theme->panel), 0);
	lv_obj_set_style_bg_opa(g_input_ta, LV_OPA_COVER, 0);
	lv_obj_set_style_border_color(g_input_ta, lv_color_hex(theme->line), 0);
	lv_obj_set_style_border_width(g_input_ta, 1, 0);
	lv_obj_set_style_radius(g_input_ta, 16, 0);
	lv_obj_set_style_text_font(g_input_ta, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(g_input_ta, lv_color_hex(theme->text), 0);
	lv_obj_set_style_pad_left(g_input_ta, 14, 0);
	lv_obj_set_style_pad_right(g_input_ta, 14, 0);
	lv_obj_set_style_pad_top(g_input_ta, 14, 0);
	lv_obj_set_style_pad_bottom(g_input_ta, 14, 0);
	lv_obj_clear_flag(g_input_ta, LV_OBJ_FLAG_SCROLLABLE);
	lv_textarea_set_one_line(g_input_ta, true);
	lv_textarea_set_max_length(g_input_ta, MEMO_TEXT_MAX - 1U);
	lv_textarea_set_placeholder_text(g_input_ta, "Type a task");
	lv_textarea_set_text(g_input_ta, g_memo.draft);
	lv_obj_add_event_cb(g_input_ta, memo_input_ready_event_cb, LV_EVENT_READY, NULL);

	btn = create_action_button(composer, 18, 210, COMPOSER_W - 36, 42, "Add Task",
				  theme->accent, theme->accent, theme->action_text);
	lv_obj_add_event_cb(btn, memo_action_event_cb, LV_EVENT_CLICKED,
			    (void *)(uintptr_t)MEMO_ACTION_ADD_TASK);

	btn = create_action_button(composer, 18, 260, COMPOSER_W - 36, 42, "Clear Input",
				  theme->panel_alt, theme->line, theme->dim);
	lv_obj_add_event_cb(btn, memo_action_event_cb, LV_EVENT_CLICKED,
			    (void *)(uintptr_t)MEMO_ACTION_CLEAR_INPUT);

	tasks = create_panel(scr, CONTENT_X + COMPOSER_W + PANEL_GAP, BODY_Y, TASK_EXPANDED_W, BODY_H,
			     theme->panel, theme->line);
	label = lv_label_create(tasks);
	lv_obj_set_pos(label, 18, 18);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->accent), 0);
	lv_label_set_text(label, "TASK RAIL");

	label = lv_label_create(tasks);
	lv_obj_set_pos(label, 18, 42);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->text), 0);
	lv_label_set_text(label, "Todo");

	g_task_summary = lv_label_create(tasks);
	lv_obj_set_pos(g_task_summary, 18, 72);
	lv_obj_set_width(g_task_summary, TASK_EXPANDED_W - 36);
	lv_label_set_long_mode(g_task_summary, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(g_task_summary, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(g_task_summary, lv_color_hex(theme->dim), 0);
	lv_label_set_text(g_task_summary, "");

	g_task_list = lv_obj_create(tasks);
	lv_obj_set_pos(g_task_list, PANEL_INNER_X, PANEL_LIST_Y);
	lv_obj_set_size(g_task_list, TASK_EXPANDED_W - (PANEL_INNER_X * 2), PANEL_LIST_H);
	style_list_container(g_task_list);

	memo_refresh_dynamic_ui();
	memo_focus_input();
}

static void memo_on_create(app_s *app)
{
	(void)app;

	g_statemgr = statemgr_client_get();
	(void)memo_read_theme_mode(&g_theme_mode);
	(void)memo_get_theme_revision(&g_last_theme_revision);
	memo_load_state();
	memo_set_default_status();
	create_ui();
	log_info("memo ready\n");
}

static void memo_on_update(app_s *app, uint64_t mono_ms)
{
	(void)app;
	memo_refresh_theme(mono_ms);
}

int main(int argc, char *argv[])
{
	app_s app = {0};
	app_config_s config = {
		.name = "memo",
		.x = APP_DEFAULT_X,
		.y = APP_DEFAULT_Y,
		.z = 0,
		.width = APP_DEFAULT_WIDTH,
		.height = APP_DEFAULT_HEIGHT,
		.window_flags = WINDOW_FLAG_FOCUSABLE,
		.enable_input = 1,
	};
	app_lifecycle_ops_s lifecycle = {
		.on_create = memo_on_create,
		.on_update = memo_on_update,
	};

	(void)argc;
	(void)argv;

	log_info("memo start!\n");
	app_init(&app, &config, &lifecycle, NULL);
	return app_run(&app);
}
