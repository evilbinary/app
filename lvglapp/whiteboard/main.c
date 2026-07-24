#include "lvgl.h"
#include "widgets/lv_canvas.h"
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
#define HEADER_H 110
#define BODY_Y (CONTENT_Y + HEADER_H + 16)
#define BODY_H (APP_DEFAULT_HEIGHT - BODY_Y - SYSTEM_OVERLAY_GAP)
#define SIDEBAR_W 236
#define PANEL_GAP 16
#define BOARD_W (CONTENT_W - SIDEBAR_W - PANEL_GAP)
#define PANEL_INNER_PAD 24
#define BOARD_CANVAS_X PANEL_INNER_PAD
#define BOARD_CANVAS_Y 24
#define BOARD_CANVAS_W (BOARD_W - (PANEL_INNER_PAD * 2))
#define BOARD_CANVAS_H (BODY_H - BOARD_CANVAS_Y - PANEL_INNER_PAD)
#define METRIC_CARD_W 106
#define METRIC_CARD_H 92
#define THEME_REFRESH_INTERVAL_MS 100ULL
#define WHITEBOARD_STROKE_MAX 96U
#define WHITEBOARD_POINT_MAX 256U
#define WHITEBOARD_COLOR_COUNT 10U
#define WHITEBOARD_WIDTH_COUNT 6U
#define WHITEBOARD_GRID_STEP 32
#define TOOL_BTN_W 92
#define TOOL_BTN_H 40
#define SWATCH_SIZE 32
#define SWATCH_GAP_X 6
#define SWATCH_GAP_Y 8
#define WIDTH_BTN_W 58
#define WIDTH_BTN_H 36
#define WIDTH_BTN_GAP_X 10
#define WIDTH_BTN_GAP_Y 8

#define COLOR_BG_LIGHT          0xfff4ee
#define COLOR_BG_ALT_LIGHT      0xeaf4ff
#define COLOR_PANEL_LIGHT       0xfffffb
#define COLOR_PANEL_ALT_LIGHT   0xfff1eb
#define COLOR_LINE_LIGHT        0xf0ddd2
#define COLOR_TEXT_LIGHT        0x24324a
#define COLOR_DIM_LIGHT         0x7d8198
#define COLOR_ACCENT_LIGHT      0xff7d5c
#define COLOR_ACCENT_SOFT_LIGHT 0xffe6dd
#define COLOR_WARM_LIGHT        0xffc56f
#define COLOR_WARM_SOFT_LIGHT   0xfff0d2
#define COLOR_COOL_LIGHT        0x5d79d3
#define COLOR_COOL_SOFT_LIGHT   0xe8efff
#define COLOR_BOARD_BG_LIGHT    0xfffcf8
#define COLOR_BOARD_GRID_LIGHT  0xf2e8df

typedef enum whiteboard_theme_mode {
	WHITEBOARD_THEME_LIGHT = 0,
	WHITEBOARD_THEME_DARK = 1,
} whiteboard_theme_mode_e;

typedef enum whiteboard_tool {
	WHITEBOARD_TOOL_INK = 0,
	WHITEBOARD_TOOL_ERASER = 1,
} whiteboard_tool_e;

typedef enum whiteboard_action {
	WHITEBOARD_ACTION_TOOL_INK = 1,
	WHITEBOARD_ACTION_TOOL_ERASER = 2,
	WHITEBOARD_ACTION_UNDO = 3,
	WHITEBOARD_ACTION_CLEAR = 4,
} whiteboard_action_e;

typedef struct whiteboard_theme {
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
	uint32_t cool;
	uint32_t cool_soft;
	uint32_t board_bg;
	uint32_t board_grid;
	uint32_t action_text;
} whiteboard_theme_s;

typedef struct whiteboard_stroke {
	lv_point_t points[WHITEBOARD_POINT_MAX];
	uint16_t point_count;
	uint8_t color_slot;
	uint8_t width_slot;
	uint8_t eraser;
} whiteboard_stroke_s;

typedef struct whiteboard_state {
	whiteboard_stroke_s strokes[WHITEBOARD_STROKE_MAX];
	uint32_t stroke_count;
	uint32_t current_color;
	uint32_t current_width;
	uint32_t active_stroke;
	uint8_t drawing;
	uint8_t tool;
	char status_text[32];
	uint32_t status_bg;
	uint32_t status_fg;
} whiteboard_state_s;

static whiteboard_state_s g_whiteboard = {0};
static statemgr_client_s *g_statemgr = NULL;
static uint32_t g_theme_mode = WHITEBOARD_THEME_LIGHT;
static uint64_t g_last_theme_ms = 0;
static uint64_t g_last_theme_revision = 0;

static lv_obj_t *g_status_chip = NULL;
static lv_obj_t *g_metric_values[3] = {0};
static lv_obj_t *g_canvas_shell = NULL;
static lv_obj_t *g_canvas = NULL;
static lv_obj_t *g_color_buttons[WHITEBOARD_COLOR_COUNT] = {0};
static lv_obj_t *g_width_buttons[WHITEBOARD_WIDTH_COUNT] = {0};
static lv_obj_t *g_tool_buttons[2] = {0};

static uint8_t g_canvas_buf[LV_CANVAS_BUF_SIZE_TRUE_COLOR(BOARD_CANVAS_W, BOARD_CANVAS_H)];

static const char *g_color_names[WHITEBOARD_COLOR_COUNT] = {
	"Graphite",
	"Coral",
	"Amber",
	"Gold",
	"Mint",
	"Pine",
	"Blue",
	"Violet",
	"Rose",
	"Cocoa",
};

static const char *g_width_names[WHITEBOARD_WIDTH_COUNT] = {
	"2px",
	"4px",
	"6px",
	"9px",
	"13px",
	"18px",
};

static const lv_coord_t g_width_values[WHITEBOARD_WIDTH_COUNT] = {
	2,
	4,
	6,
	9,
	13,
	18,
};

static const whiteboard_theme_s g_whiteboard_theme_light = {
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
	.cool = COLOR_COOL_LIGHT,
	.cool_soft = COLOR_COOL_SOFT_LIGHT,
	.board_bg = COLOR_BOARD_BG_LIGHT,
	.board_grid = COLOR_BOARD_GRID_LIGHT,
	.action_text = 0x15212c,
};

static const whiteboard_theme_s g_whiteboard_theme_dark = {
	.bg = 0x171828,
	.bg_alt = 0x22253c,
	.panel = 0x20243a,
	.panel_alt = 0x2a3048,
	.line = 0x46506f,
	.text = 0xfff6ef,
	.dim = 0xc7b7b0,
	.accent = 0xffa07c,
	.accent_soft = 0x4b2d2a,
	.warm = 0xf6c86f,
	.warm_soft = 0x4d3927,
	.cool = 0x94b3ff,
	.cool_soft = 0x26344f,
	.board_bg = 0x1b2131,
	.board_grid = 0x2c3750,
	.action_text = 0x18212d,
};

#define WHITEBOARD_ACTION_COLOR_BASE 100U
#define WHITEBOARD_ACTION_WIDTH_BASE 200U

static const whiteboard_theme_s *whiteboard_theme(void)
{
	return g_theme_mode == WHITEBOARD_THEME_DARK ?
		&g_whiteboard_theme_dark : &g_whiteboard_theme_light;
}

static void whiteboard_refresh_ui(void);
static void whiteboard_redraw_canvas(void);
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
	const whiteboard_theme_s *theme = whiteboard_theme();
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
	lv_label_set_text(value_label, "-");
	return value_label;
}

static lv_obj_t *create_action_button(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
				      lv_coord_t w, lv_coord_t h, const char *text)
{
	const whiteboard_theme_s *theme = whiteboard_theme();
	lv_obj_t *btn = lv_btn_create(parent);
	lv_obj_t *label = lv_label_create(btn);

	lv_obj_set_pos(btn, x, y);
	lv_obj_set_size(btn, w, h);
	lv_obj_set_style_radius(btn, 18, 0);
	lv_obj_set_style_bg_color(btn, lv_color_hex(theme->panel_alt), 0);
	lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
	lv_obj_set_style_border_color(btn, lv_color_hex(theme->line), 0);
	lv_obj_set_style_border_width(btn, 1, 0);
	lv_obj_set_style_shadow_width(btn, 0, 0);
	lv_obj_set_style_pad_all(btn, 0, 0);
	lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICK_FOCUSABLE);

	lv_obj_center(label);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->dim), 0);
	lv_label_set_text(label, text);
	return btn;
}

static uint32_t whiteboard_palette_color(uint32_t slot)
{
	const whiteboard_theme_s *theme = whiteboard_theme();

	switch (slot) {
		case 0:
			return theme->text;
		case 1:
			return theme->accent;
		case 2:
			return 0xffa347;
		case 3:
			return theme->warm;
		case 4:
			return 0x53c58c;
		case 5:
			return 0x23956d;
		case 6:
			return theme->cool;
		case 7:
			return 0x8d70f8;
		case 8:
			return 0xd46da3;
		case 9:
		default:
			return 0x8f5f46;
	}
}

static const char *whiteboard_tool_name(void)
{
	return g_whiteboard.tool == WHITEBOARD_TOOL_ERASER ? "Erase" : "Ink";
}

static void whiteboard_set_status(const char *text, uint32_t bg, uint32_t fg)
{
	if (text == NULL) {
		return;
	}

	strncpy(g_whiteboard.status_text, text, sizeof(g_whiteboard.status_text) - 1U);
	g_whiteboard.status_text[sizeof(g_whiteboard.status_text) - 1U] = '\0';
	g_whiteboard.status_bg = bg;
	g_whiteboard.status_fg = fg;

	if (g_status_chip != NULL) {
		lv_obj_set_style_bg_color(g_status_chip, lv_color_hex(bg), 0);
		lv_obj_set_style_text_color(g_status_chip, lv_color_hex(fg), 0);
		lv_label_set_text(g_status_chip, g_whiteboard.status_text);
	}
}

static void whiteboard_set_default_status(void)
{
	const whiteboard_theme_s *theme = whiteboard_theme();

	whiteboard_set_status("Session only", theme->accent_soft, theme->accent);
}

static void whiteboard_update_metrics(void)
{
	char text[32] = {0};

	if (g_metric_values[0] != NULL) {
		snprintf(text, sizeof(text), "%u", (unsigned int)g_whiteboard.stroke_count);
		lv_label_set_text(g_metric_values[0], text);
	}
	if (g_metric_values[1] != NULL) {
		lv_label_set_text(g_metric_values[1], whiteboard_tool_name());
	}
	if (g_metric_values[2] != NULL) {
		lv_label_set_text(g_metric_values[2], g_width_names[g_whiteboard.current_width]);
	}
}

static void whiteboard_style_toggle_button(lv_obj_t *btn, uint8_t active,
					   uint32_t active_bg, uint32_t active_border,
					   uint32_t active_fg)
{
	const whiteboard_theme_s *theme = whiteboard_theme();
	lv_obj_t *label = NULL;

	if (btn == NULL) {
		return;
	}

	label = lv_obj_get_child(btn, 0);
	lv_obj_set_style_bg_color(btn, lv_color_hex(active ? active_bg : theme->panel_alt), 0);
	lv_obj_set_style_border_color(btn, lv_color_hex(active ? active_border : theme->line), 0);
	lv_obj_set_style_border_width(btn, active ? 2 : 1, 0);
	lv_obj_set_style_shadow_width(btn, active ? 12 : 0, 0);
	lv_obj_set_style_shadow_color(btn, lv_color_hex(active_border), 0);
	lv_obj_set_style_shadow_opa(btn, active ? LV_OPA_20 : LV_OPA_TRANSP, 0);
	if (label != NULL) {
		lv_obj_set_style_text_color(label, lv_color_hex(active ? active_fg : theme->dim), 0);
	}
}

static void whiteboard_refresh_control_styles(void)
{
	const whiteboard_theme_s *theme = whiteboard_theme();

	whiteboard_style_toggle_button(g_tool_buttons[WHITEBOARD_TOOL_INK],
				       g_whiteboard.tool == WHITEBOARD_TOOL_INK,
				       theme->accent_soft, theme->accent, theme->accent);
	whiteboard_style_toggle_button(g_tool_buttons[WHITEBOARD_TOOL_ERASER],
				       g_whiteboard.tool == WHITEBOARD_TOOL_ERASER,
				       theme->warm_soft, theme->warm, theme->warm);

	for (uint32_t i = 0; i < WHITEBOARD_COLOR_COUNT; i++) {
		lv_obj_t *btn = g_color_buttons[i];
		uint8_t active = g_whiteboard.current_color == i;
		uint32_t swatch = whiteboard_palette_color(i);

		if (btn == NULL) {
			continue;
		}

		lv_obj_set_style_bg_color(btn, lv_color_hex(swatch), 0);
		lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
		lv_obj_set_style_border_color(btn, lv_color_hex(active ? theme->text : theme->panel), 0);
		lv_obj_set_style_border_width(btn, active ? 3 : 1, 0);
		lv_obj_set_style_shadow_width(btn, active ? 14 : 0, 0);
		lv_obj_set_style_shadow_color(btn, lv_color_hex(swatch), 0);
		lv_obj_set_style_shadow_opa(btn, active ? LV_OPA_30 : LV_OPA_TRANSP, 0);
	}

	for (uint32_t i = 0; i < WHITEBOARD_WIDTH_COUNT; i++) {
		whiteboard_style_toggle_button(g_width_buttons[i],
					       g_whiteboard.current_width == i,
					       theme->cool_soft, theme->cool, theme->cool);
	}

	if (g_canvas_shell != NULL) {
		lv_obj_set_style_bg_color(g_canvas_shell, lv_color_hex(theme->board_bg), 0);
		lv_obj_set_style_border_color(g_canvas_shell, lv_color_hex(theme->line), 0);
		lv_obj_set_style_shadow_color(g_canvas_shell, lv_color_hex(theme->line), 0);
	}
}

static void whiteboard_refresh_ui(void)
{
	whiteboard_update_metrics();
	whiteboard_refresh_control_styles();
	if (g_status_chip != NULL && g_whiteboard.status_text[0] != '\0') {
		lv_obj_set_style_bg_color(g_status_chip, lv_color_hex(g_whiteboard.status_bg), 0);
		lv_obj_set_style_text_color(g_status_chip, lv_color_hex(g_whiteboard.status_fg), 0);
		lv_label_set_text(g_status_chip, g_whiteboard.status_text);
	}
}

static uint8_t whiteboard_get_theme_revision(uint64_t *revision_out)
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

static uint8_t whiteboard_read_theme_mode(uint32_t *theme_mode_out)
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

	*theme_mode_out = (uint32_t)response.entry.value_u64 == WHITEBOARD_THEME_DARK ?
		WHITEBOARD_THEME_DARK : WHITEBOARD_THEME_LIGHT;
	return 1U;
}

static void whiteboard_create_background(lv_obj_t *scr)
{
	const whiteboard_theme_s *theme = whiteboard_theme();
	lv_obj_t *shape = NULL;

	lv_obj_set_style_bg_color(scr, lv_color_hex(theme->bg), 0);
	lv_obj_set_style_bg_grad_color(scr, lv_color_hex(theme->bg_alt), 0);
	lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, 0);
	lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
	lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

	shape = lv_obj_create(scr);
	lv_obj_set_pos(shape, -54, -34);
	lv_obj_set_size(shape, 228, 228);
	lv_obj_set_style_bg_color(shape, lv_color_hex(theme->cool), 0);
	lv_obj_set_style_bg_opa(shape, LV_OPA_10, 0);
	lv_obj_set_style_border_width(shape, 0, 0);
	lv_obj_set_style_radius(shape, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_shadow_width(shape, 0, 0);
	clear_static_flags(shape);

	shape = lv_obj_create(scr);
	lv_obj_set_pos(shape, APP_DEFAULT_WIDTH - 222, APP_DEFAULT_HEIGHT - 214);
	lv_obj_set_size(shape, 256, 256);
	lv_obj_set_style_bg_color(shape, lv_color_hex(theme->accent), 0);
	lv_obj_set_style_bg_opa(shape, 18, 0);
	lv_obj_set_style_border_width(shape, 0, 0);
	lv_obj_set_style_radius(shape, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_shadow_width(shape, 0, 0);
	clear_static_flags(shape);
}

static void whiteboard_init_line_dsc(lv_draw_line_dsc_t *line_dsc,
				     const whiteboard_stroke_s *stroke)
{
	const whiteboard_theme_s *theme = whiteboard_theme();

	if (line_dsc == NULL || stroke == NULL) {
		return;
	}

	lv_draw_line_dsc_init(line_dsc);
	line_dsc->width = g_width_values[stroke->width_slot < WHITEBOARD_WIDTH_COUNT ?
					 stroke->width_slot : 0U];
	line_dsc->color = lv_color_hex(stroke->eraser ?
				       theme->board_bg : whiteboard_palette_color(stroke->color_slot));
	line_dsc->opa = LV_OPA_COVER;
	line_dsc->round_start = 1;
	line_dsc->round_end = 1;
}

static void whiteboard_draw_grid(void)
{
	const whiteboard_theme_s *theme = whiteboard_theme();
	lv_draw_line_dsc_t line_dsc;
	lv_point_t points[2];

	if (g_canvas == NULL) {
		return;
	}

	lv_draw_line_dsc_init(&line_dsc);
	line_dsc.width = 1;
	line_dsc.color = lv_color_hex(theme->board_grid);
	line_dsc.opa = 92;
	line_dsc.round_start = 0;
	line_dsc.round_end = 0;

	for (lv_coord_t x = 0; x < BOARD_CANVAS_W; x += WHITEBOARD_GRID_STEP) {
		points[0].x = x;
		points[0].y = 0;
		points[1].x = x;
		points[1].y = BOARD_CANVAS_H - 1;
		lv_canvas_draw_line(g_canvas, points, 2, &line_dsc);
	}

	for (lv_coord_t y = 0; y < BOARD_CANVAS_H; y += WHITEBOARD_GRID_STEP) {
		points[0].x = 0;
		points[0].y = y;
		points[1].x = BOARD_CANVAS_W - 1;
		points[1].y = y;
		lv_canvas_draw_line(g_canvas, points, 2, &line_dsc);
	}
}

static void whiteboard_draw_stroke(const whiteboard_stroke_s *stroke)
{
	lv_draw_line_dsc_t line_dsc;
	lv_point_t dot[2];

	if (g_canvas == NULL || stroke == NULL || stroke->point_count == 0U) {
		return;
	}

	whiteboard_init_line_dsc(&line_dsc, stroke);
	if (stroke->point_count == 1U) {
		dot[0] = stroke->points[0];
		dot[1] = stroke->points[0];
		lv_canvas_draw_line(g_canvas, dot, 2, &line_dsc);
		return;
	}

	lv_canvas_draw_line(g_canvas, stroke->points, stroke->point_count, &line_dsc);
}

static void whiteboard_redraw_canvas(void)
{
	const whiteboard_theme_s *theme = whiteboard_theme();

	if (g_canvas == NULL) {
		return;
	}

	lv_canvas_fill_bg(g_canvas, lv_color_hex(theme->board_bg), LV_OPA_COVER);
	whiteboard_draw_grid();
	for (uint32_t i = 0; i < g_whiteboard.stroke_count; i++) {
		whiteboard_draw_stroke(&g_whiteboard.strokes[i]);
	}
}

static void whiteboard_draw_latest_segment(whiteboard_stroke_s *stroke)
{
	lv_draw_line_dsc_t line_dsc;
	lv_point_t points[2];

	if (g_canvas == NULL || stroke == NULL || stroke->point_count < 2U) {
		return;
	}

	whiteboard_init_line_dsc(&line_dsc, stroke);
	points[0] = stroke->points[stroke->point_count - 2U];
	points[1] = stroke->points[stroke->point_count - 1U];
	lv_canvas_draw_line(g_canvas, points, 2, &line_dsc);
}

static uint8_t whiteboard_pointer_to_canvas(lv_point_t *out)
{
	lv_indev_t *indev = NULL;
	lv_area_t coords;
	lv_point_t point = {0};

	if (out == NULL || g_canvas_shell == NULL) {
		return 0U;
	}

	indev = lv_indev_get_act();
	if (indev == NULL) {
		return 0U;
	}

	lv_indev_get_point(indev, &point);
	lv_obj_get_coords(g_canvas_shell, &coords);
	if (point.x < coords.x1) {
		point.x = coords.x1;
	}
	if (point.y < coords.y1) {
		point.y = coords.y1;
	}
	if (point.x > coords.x2) {
		point.x = coords.x2;
	}
	if (point.y > coords.y2) {
		point.y = coords.y2;
	}

	out->x = point.x - coords.x1;
	out->y = point.y - coords.y1;
	return 1U;
}

static void whiteboard_begin_stroke(const lv_point_t *point)
{
	whiteboard_stroke_s *stroke = NULL;
	const whiteboard_theme_s *theme = whiteboard_theme();

	if (point == NULL || g_whiteboard.drawing) {
		return;
	}
	if (g_whiteboard.stroke_count >= WHITEBOARD_STROKE_MAX) {
		whiteboard_set_status("Board full", theme->warm_soft, theme->warm);
		return;
	}

	stroke = &g_whiteboard.strokes[g_whiteboard.stroke_count];
	memset(stroke, 0, sizeof(*stroke));
	stroke->color_slot = (uint8_t)g_whiteboard.current_color;
	stroke->width_slot = (uint8_t)g_whiteboard.current_width;
	stroke->eraser = g_whiteboard.tool == WHITEBOARD_TOOL_ERASER ? 1U : 0U;
	stroke->points[0] = *point;
	stroke->point_count = 1U;

	g_whiteboard.active_stroke = g_whiteboard.stroke_count;
	g_whiteboard.stroke_count++;
	g_whiteboard.drawing = 1U;
	whiteboard_update_metrics();
}

static void whiteboard_append_point(const lv_point_t *point)
{
	whiteboard_stroke_s *stroke = NULL;
	const whiteboard_theme_s *theme = whiteboard_theme();

	if (point == NULL || !g_whiteboard.drawing ||
	    g_whiteboard.active_stroke >= g_whiteboard.stroke_count) {
		return;
	}

	stroke = &g_whiteboard.strokes[g_whiteboard.active_stroke];
	if (stroke->point_count == 0U) {
		return;
	}
	if (stroke->points[stroke->point_count - 1U].x == point->x &&
	    stroke->points[stroke->point_count - 1U].y == point->y) {
		return;
	}
	if (stroke->point_count >= WHITEBOARD_POINT_MAX) {
		whiteboard_set_status("Stroke detail capped", theme->warm_soft, theme->warm);
		return;
	}

	stroke->points[stroke->point_count] = *point;
	stroke->point_count++;
	whiteboard_draw_latest_segment(stroke);
}

static void whiteboard_finish_stroke(void)
{
	whiteboard_stroke_s *stroke = NULL;
	const whiteboard_theme_s *theme = whiteboard_theme();

	if (!g_whiteboard.drawing ||
	    g_whiteboard.active_stroke >= g_whiteboard.stroke_count) {
		return;
	}

	stroke = &g_whiteboard.strokes[g_whiteboard.active_stroke];
	if (stroke->point_count == 1U && stroke->point_count < WHITEBOARD_POINT_MAX) {
		stroke->points[1] = stroke->points[0];
		stroke->point_count = 2U;
		whiteboard_draw_latest_segment(stroke);
	}

	g_whiteboard.drawing = 0U;
	g_whiteboard.active_stroke = 0U;
	whiteboard_set_status(stroke->eraser ? "Erased" : "Stroke added",
			      stroke->eraser ? theme->warm_soft : theme->accent_soft,
			      stroke->eraser ? theme->warm : theme->accent);
}

static void whiteboard_canvas_event_cb(lv_event_t *e)
{
	lv_event_code_t code = lv_event_get_code(e);
	lv_point_t point = {0};

	if (code == LV_EVENT_PRESSED) {
		if (whiteboard_pointer_to_canvas(&point)) {
			whiteboard_begin_stroke(&point);
		}
		return;
	}
	if (code == LV_EVENT_PRESSING) {
		if (whiteboard_pointer_to_canvas(&point)) {
			whiteboard_append_point(&point);
		}
		return;
	}
	if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
		whiteboard_finish_stroke();
	}
}

static void whiteboard_action_event_cb(lv_event_t *e)
{
	uintptr_t action = 0U;
	const whiteboard_theme_s *theme = whiteboard_theme();

	if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
		return;
	}

	action = (uintptr_t)lv_event_get_user_data(e);
	if (action == WHITEBOARD_ACTION_TOOL_INK) {
		g_whiteboard.tool = WHITEBOARD_TOOL_INK;
		whiteboard_set_status("Ink mode", theme->accent_soft, theme->accent);
		whiteboard_refresh_ui();
		return;
	}
	if (action == WHITEBOARD_ACTION_TOOL_ERASER) {
		g_whiteboard.tool = WHITEBOARD_TOOL_ERASER;
		whiteboard_set_status("Eraser ready", theme->warm_soft, theme->warm);
		whiteboard_refresh_ui();
		return;
	}
	if (action == WHITEBOARD_ACTION_UNDO) {
		if (g_whiteboard.stroke_count == 0U) {
			whiteboard_set_status("Nothing to undo", theme->panel_alt, theme->dim);
			return;
		}
		g_whiteboard.stroke_count--;
		g_whiteboard.drawing = 0U;
		whiteboard_redraw_canvas();
		whiteboard_set_status("Removed last stroke", theme->cool_soft, theme->cool);
		whiteboard_refresh_ui();
		return;
	}
	if (action == WHITEBOARD_ACTION_CLEAR) {
		if (g_whiteboard.stroke_count == 0U) {
			whiteboard_set_status("Canvas already clear", theme->panel_alt, theme->dim);
			return;
		}
		memset(g_whiteboard.strokes, 0, sizeof(g_whiteboard.strokes));
		g_whiteboard.stroke_count = 0U;
		g_whiteboard.drawing = 0U;
		whiteboard_redraw_canvas();
		whiteboard_set_status("Canvas cleared", theme->warm_soft, theme->warm);
		whiteboard_refresh_ui();
		return;
	}
	if (action >= WHITEBOARD_ACTION_COLOR_BASE &&
	    action < WHITEBOARD_ACTION_COLOR_BASE + WHITEBOARD_COLOR_COUNT) {
		g_whiteboard.current_color = (uint32_t)(action - WHITEBOARD_ACTION_COLOR_BASE);
		g_whiteboard.tool = WHITEBOARD_TOOL_INK;
		whiteboard_set_status(g_color_names[g_whiteboard.current_color],
				      theme->accent_soft, theme->accent);
		whiteboard_refresh_ui();
		return;
	}
	if (action >= WHITEBOARD_ACTION_WIDTH_BASE &&
	    action < WHITEBOARD_ACTION_WIDTH_BASE + WHITEBOARD_WIDTH_COUNT) {
		g_whiteboard.current_width = (uint32_t)(action - WHITEBOARD_ACTION_WIDTH_BASE);
		whiteboard_set_status(g_width_names[g_whiteboard.current_width],
				      theme->cool_soft, theme->cool);
		whiteboard_refresh_ui();
	}
}

static void whiteboard_rebuild_ui(void)
{
	g_status_chip = NULL;
	memset(g_metric_values, 0, sizeof(g_metric_values));
	g_canvas_shell = NULL;
	g_canvas = NULL;
	memset(g_color_buttons, 0, sizeof(g_color_buttons));
	memset(g_width_buttons, 0, sizeof(g_width_buttons));
	memset(g_tool_buttons, 0, sizeof(g_tool_buttons));
	lv_obj_clean(lv_scr_act());
	create_ui();
	whiteboard_redraw_canvas();
	whiteboard_refresh_ui();
}

static void whiteboard_refresh_theme(uint64_t mono_ms)
{
	uint64_t revision = 0;
	uint32_t theme_mode = g_theme_mode;

	if (mono_ms < g_last_theme_ms + THEME_REFRESH_INTERVAL_MS) {
		return;
	}
	g_last_theme_ms = mono_ms;

	if (!whiteboard_get_theme_revision(&revision) ||
	    !whiteboard_read_theme_mode(&theme_mode)) {
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
	whiteboard_rebuild_ui();
	log_info("whiteboard theme synced: mode=%u rev=%llu\n",
		 (unsigned int)g_theme_mode,
		 (unsigned long long)revision);
}

static void create_ui(void)
{
	const whiteboard_theme_s *theme = whiteboard_theme();
	lv_obj_t *scr = lv_scr_act();
	lv_obj_t *header = NULL;
	lv_obj_t *sidebar = NULL;
	lv_obj_t *board = NULL;
	lv_obj_t *label = NULL;
	lv_obj_t *btn = NULL;

	whiteboard_create_background(scr);

	header = create_panel(scr, CONTENT_X, CONTENT_Y, CONTENT_W, HEADER_H,
			      theme->panel, theme->line);
	(void)create_chip(header, 24, 18, "NEW APP", theme->cool_soft, theme->cool);
	g_status_chip = create_chip(header, 118, 18,
				    g_whiteboard.status_text[0] == '\0' ? "Session only" : g_whiteboard.status_text,
				    g_whiteboard.status_bg == 0U ? theme->accent_soft : g_whiteboard.status_bg,
				    g_whiteboard.status_fg == 0U ? theme->accent : g_whiteboard.status_fg);

	label = lv_label_create(header);
	lv_obj_set_pos(label, 24, 54);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_32, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->text), 0);
	lv_label_set_text(label, "Whiteboard");

	g_metric_values[0] = create_metric_card(header, 584, 10, "Strokes");
	g_metric_values[1] = create_metric_card(header, 698, 10, "Tool");
	g_metric_values[2] = create_metric_card(header, 812, 10, "Width");

	sidebar = create_panel(scr, CONTENT_X, BODY_Y, SIDEBAR_W, BODY_H,
			       theme->panel, theme->line);
	label = lv_label_create(sidebar);
	lv_obj_set_pos(label, 18, 18);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->accent), 0);
	lv_label_set_text(label, "TOOLS");

	label = lv_label_create(sidebar);
	lv_obj_set_pos(label, 18, 42);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->text), 0);
	lv_label_set_text(label, "Sketch");

	g_tool_buttons[WHITEBOARD_TOOL_INK] = create_action_button(sidebar, 18, 84, TOOL_BTN_W, TOOL_BTN_H, "Ink");
	lv_obj_add_event_cb(g_tool_buttons[WHITEBOARD_TOOL_INK], whiteboard_action_event_cb,
			    LV_EVENT_CLICKED, (void *)(uintptr_t)WHITEBOARD_ACTION_TOOL_INK);
	g_tool_buttons[WHITEBOARD_TOOL_ERASER] = create_action_button(sidebar, 126, 84, TOOL_BTN_W, TOOL_BTN_H, "Erase");
	lv_obj_add_event_cb(g_tool_buttons[WHITEBOARD_TOOL_ERASER], whiteboard_action_event_cb,
			    LV_EVENT_CLICKED, (void *)(uintptr_t)WHITEBOARD_ACTION_TOOL_ERASER);

	label = lv_label_create(sidebar);
	lv_obj_set_pos(label, 18, 146);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->dim), 0);
	lv_label_set_text(label, "COLORS");

	for (uint32_t i = 0; i < WHITEBOARD_COLOR_COUNT; i++) {
		lv_coord_t x = (lv_coord_t)(18 + (i % 5U) * (SWATCH_SIZE + SWATCH_GAP_X));
		lv_coord_t y = (lv_coord_t)(174 + (i / 5U) * (SWATCH_SIZE + SWATCH_GAP_Y));

		g_color_buttons[i] = lv_btn_create(sidebar);
		lv_obj_set_pos(g_color_buttons[i], x, y);
		lv_obj_set_size(g_color_buttons[i], SWATCH_SIZE, SWATCH_SIZE);
		lv_obj_set_style_radius(g_color_buttons[i], 12, 0);
		lv_obj_set_style_pad_all(g_color_buttons[i], 0, 0);
		lv_obj_set_style_border_width(g_color_buttons[i], 1, 0);
		lv_obj_set_style_shadow_width(g_color_buttons[i], 0, 0);
		lv_obj_clear_flag(g_color_buttons[i], LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICK_FOCUSABLE);
		lv_obj_add_event_cb(g_color_buttons[i], whiteboard_action_event_cb, LV_EVENT_CLICKED,
				    (void *)(uintptr_t)(WHITEBOARD_ACTION_COLOR_BASE + i));
	}

	label = lv_label_create(sidebar);
	lv_obj_set_pos(label, 18, 256);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->dim), 0);
	lv_label_set_text(label, "BRUSH");

	for (uint32_t i = 0; i < WHITEBOARD_WIDTH_COUNT; i++) {
		lv_coord_t x = (lv_coord_t)(18 + (i % 3U) * (WIDTH_BTN_W + WIDTH_BTN_GAP_X));
		lv_coord_t y = (lv_coord_t)(286 + (i / 3U) * (WIDTH_BTN_H + WIDTH_BTN_GAP_Y));

		g_width_buttons[i] = create_action_button(sidebar, x, y,
							 WIDTH_BTN_W, WIDTH_BTN_H, g_width_names[i]);
		lv_obj_add_event_cb(g_width_buttons[i], whiteboard_action_event_cb, LV_EVENT_CLICKED,
				    (void *)(uintptr_t)(WHITEBOARD_ACTION_WIDTH_BASE + i));
	}

	label = lv_label_create(sidebar);
	lv_obj_set_pos(label, 18, 380);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->dim), 0);
	lv_label_set_text(label, "ACTIONS");

	btn = create_action_button(sidebar, 18, 410, TOOL_BTN_W, TOOL_BTN_H, "Undo");
	lv_obj_add_event_cb(btn, whiteboard_action_event_cb, LV_EVENT_CLICKED,
			    (void *)(uintptr_t)WHITEBOARD_ACTION_UNDO);
	btn = create_action_button(sidebar, 126, 410, TOOL_BTN_W, TOOL_BTN_H, "Clear");
	lv_obj_add_event_cb(btn, whiteboard_action_event_cb, LV_EVENT_CLICKED,
			    (void *)(uintptr_t)WHITEBOARD_ACTION_CLEAR);

	board = create_panel(scr, CONTENT_X + SIDEBAR_W + PANEL_GAP, BODY_Y, BOARD_W, BODY_H,
			     theme->panel, theme->line);

	g_canvas_shell = lv_obj_create(board);
	lv_obj_set_pos(g_canvas_shell, BOARD_CANVAS_X, BOARD_CANVAS_Y);
	lv_obj_set_size(g_canvas_shell, BOARD_CANVAS_W, BOARD_CANVAS_H);
	lv_obj_set_style_bg_color(g_canvas_shell, lv_color_hex(theme->board_bg), 0);
	lv_obj_set_style_bg_opa(g_canvas_shell, LV_OPA_COVER, 0);
	lv_obj_set_style_border_color(g_canvas_shell, lv_color_hex(theme->line), 0);
	lv_obj_set_style_border_width(g_canvas_shell, 1, 0);
	lv_obj_set_style_radius(g_canvas_shell, 26, 0);
	lv_obj_set_style_clip_corner(g_canvas_shell, 1, 0);
	lv_obj_set_style_shadow_width(g_canvas_shell, 0, 0);
	lv_obj_set_style_pad_all(g_canvas_shell, 0, 0);
	lv_obj_add_flag(g_canvas_shell, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
	lv_obj_clear_flag(g_canvas_shell, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICK_FOCUSABLE);
	lv_obj_add_event_cb(g_canvas_shell, whiteboard_canvas_event_cb, LV_EVENT_ALL, NULL);

	g_canvas = lv_canvas_create(g_canvas_shell);
	lv_canvas_set_buffer(g_canvas, g_canvas_buf, BOARD_CANVAS_W, BOARD_CANVAS_H,
			     LV_IMG_CF_TRUE_COLOR);
	lv_obj_set_pos(g_canvas, 0, 0);
	lv_obj_set_size(g_canvas, BOARD_CANVAS_W, BOARD_CANVAS_H);
	lv_obj_set_style_border_width(g_canvas, 0, 0);
	lv_obj_set_style_bg_opa(g_canvas, LV_OPA_TRANSP, 0);
	clear_static_flags(g_canvas);

	whiteboard_refresh_ui();
}

static void whiteboard_on_create(app_s *app)
{
	(void)app;

	memset(&g_whiteboard, 0, sizeof(g_whiteboard));
	g_whiteboard.current_color = 0U;
	g_whiteboard.current_width = 2U;
	g_whiteboard.tool = WHITEBOARD_TOOL_INK;

	g_statemgr = statemgr_client_get();
	(void)whiteboard_read_theme_mode(&g_theme_mode);
	(void)whiteboard_get_theme_revision(&g_last_theme_revision);
	whiteboard_set_default_status();
	create_ui();
	whiteboard_redraw_canvas();
	whiteboard_refresh_ui();
	log_info("whiteboard ready\n");
}

static void whiteboard_on_update(app_s *app, uint64_t mono_ms)
{
	(void)app;
	whiteboard_refresh_theme(mono_ms);
}

int main(int argc, char *argv[])
{
	app_s app = {0};
	app_config_s config = {
		.name = "whiteboard",
		.x = APP_DEFAULT_X,
		.y = APP_DEFAULT_Y,
		.z = 0,
		.width = APP_DEFAULT_WIDTH,
		.height = APP_DEFAULT_HEIGHT,
		.window_flags = WINDOW_FLAG_FOCUSABLE,
		.enable_input = 1,
	};
	app_lifecycle_ops_s lifecycle = {
		.on_create = whiteboard_on_create,
		.on_update = whiteboard_on_update,
	};

	(void)argc;
	(void)argv;

	log_info("whiteboard start!\n");
	app_init(&app, &config, &lifecycle, NULL);
	return app_run(&app);
}
