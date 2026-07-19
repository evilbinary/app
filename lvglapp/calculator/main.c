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
#define CONTENT_W 744
#define CONTENT_H (APP_DEFAULT_HEIGHT - CONTENT_Y - SYSTEM_OVERLAY_GAP)
#define DISPLAY_H 216
#define KEYPAD_Y (DISPLAY_H + 24)
#define KEY_W 196
#define KEY_H 80
#define KEY_GAP 14
#define ZERO_KEY_W ((KEY_W * 2) + KEY_GAP)
#define THEME_REFRESH_INTERVAL_MS 100ULL

#define COLOR_BG          0xfff4ee
#define COLOR_BG_ALT      0xeaf4ff
#define COLOR_PANEL       0xfffffb
#define COLOR_PANEL_ALT   0xfff1eb
#define COLOR_LINE        0xf0ddd2
#define COLOR_TEXT        0x24324a
#define COLOR_DIM         0x7d8198
#define COLOR_NUMBER      0xfffbf8
#define COLOR_OPERATOR    0xff8a68
#define COLOR_ACTION      0xffc56f
#define COLOR_DANGER      0xf08d80
#define COLOR_SUCCESS     0x4cbf95

typedef enum calculator_theme_mode {
	CALCULATOR_THEME_LIGHT = 0,
	CALCULATOR_THEME_DARK = 1,
} calculator_theme_mode_e;

typedef struct calculator_theme {
	uint32_t bg;
	uint32_t bg_alt;
	uint32_t panel;
	uint32_t panel_alt;
	uint32_t line;
	uint32_t text;
	uint32_t dim;
	uint32_t number;
	uint32_t operator_bg;
	uint32_t operator_text;
	uint32_t action_bg;
	uint32_t action_text;
	uint32_t danger_bg;
	uint32_t danger_text;
	uint32_t success;
} calculator_theme_s;

typedef struct calculator_state {
	lv_obj_t *trace_label;
	lv_obj_t *value_label;
	char input[32];
	char trace[64];
	int64_t accumulator;
	char pending_op;
	uint8_t has_accumulator;
	uint8_t reset_input;
	uint8_t error;
} calculator_state_s;

static calculator_state_s g_calc = {0};
static statemgr_client_s *g_statemgr = NULL;
static uint32_t g_theme_mode = CALCULATOR_THEME_LIGHT;
static uint64_t g_last_theme_ms = 0;
static uint64_t g_last_theme_revision = 0;

static const calculator_theme_s g_calculator_theme_light = {
	.bg = COLOR_BG,
	.bg_alt = COLOR_BG_ALT,
	.panel = COLOR_PANEL,
	.panel_alt = COLOR_PANEL_ALT,
	.line = COLOR_LINE,
	.text = COLOR_TEXT,
	.dim = COLOR_DIM,
	.number = COLOR_NUMBER,
	.operator_bg = COLOR_OPERATOR,
	.operator_text = 0x16191f,
	.action_bg = COLOR_ACTION,
	.action_text = 0x132028,
	.danger_bg = COLOR_DANGER,
	.danger_text = 0x16191f,
	.success = COLOR_SUCCESS,
};

static const calculator_theme_s g_calculator_theme_dark = {
	.bg = 0x171828,
	.bg_alt = 0x22253c,
	.panel = 0x20243a,
	.panel_alt = 0x2a3048,
	.line = 0x46506f,
	.text = 0xfff6ef,
	.dim = 0xc7b7b0,
	.number = 0x2a3048,
	.operator_bg = 0xffa07c,
	.operator_text = 0x1d2236,
	.action_bg = 0xf6c86f,
	.action_text = 0x1d2236,
	.danger_bg = 0xf3a096,
	.danger_text = 0x26191b,
	.success = 0x68d7ad,
};

static const calculator_theme_s *calculator_theme(void)
{
	return g_theme_mode == CALCULATOR_THEME_DARK ?
		&g_calculator_theme_dark : &g_calculator_theme_light;
}

static void calc_update_display(void);
static void create_ui(void);

static void clear_static_flags(lv_obj_t *obj)
{
	if (obj == NULL) {
		return;
	}
	lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CLICK_FOCUSABLE);
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

static uint8_t calculator_read_theme_mode(uint32_t *theme_mode_out, uint64_t *revision_out)
{
	statemgr_get_response_s response = {0};
	uint64_t revision = 0;

	if (theme_mode_out == NULL) {
		return 0U;
	}
	if (g_statemgr == NULL) {
		g_statemgr = statemgr_client_get();
	}
	if (g_statemgr == NULL || g_statemgr->ops.get == NULL || g_statemgr->ops.get_revision == NULL) {
		return 0U;
	}

	revision = g_statemgr->ops.get_revision(g_statemgr);
	if (revision == 0U) {
		return 0U;
	}
	if (g_statemgr->ops.get(g_statemgr, "ui.theme.mode", &response) == 0U ||
	    !response.found || response.entry.type != STATEMGR_VALUE_TYPE_U32) {
		return 0U;
	}

	*theme_mode_out = (uint32_t)response.entry.value_u64 == CALCULATOR_THEME_DARK ?
		CALCULATOR_THEME_DARK : CALCULATOR_THEME_LIGHT;
	if (revision_out != NULL) {
		*revision_out = revision;
	}
	return 1U;
}

static void calculator_rebuild_ui(void)
{
	lv_obj_t *scr = lv_scr_act();

	g_calc.trace_label = NULL;
	g_calc.value_label = NULL;
	lv_obj_clean(scr);
	create_ui();
	calc_update_display();
}

static void calculator_refresh_theme(uint64_t mono_ms, uint8_t force)
{
	uint32_t theme_mode = g_theme_mode;
	uint64_t revision = 0;

	if (!force && mono_ms < g_last_theme_ms + THEME_REFRESH_INTERVAL_MS) {
		return;
	}
	g_last_theme_ms = mono_ms;

	if (!calculator_read_theme_mode(&theme_mode, &revision)) {
		return;
	}
	if (!force && revision == g_last_theme_revision) {
		return;
	}
	g_last_theme_revision = revision;
	if (theme_mode == g_theme_mode) {
		return;
	}

	g_theme_mode = theme_mode;
	calculator_rebuild_ui();
	log_info("calculator theme synced: mode=%u rev=%llu\n",
		 (unsigned int)g_theme_mode,
		 (unsigned long long)revision);
}

static void calc_update_display(void)
{
	if (g_calc.trace_label != NULL) {
		lv_label_set_text(g_calc.trace_label, g_calc.trace[0] == '\0' ? "Ready for input" : g_calc.trace);
	}
	if (g_calc.value_label != NULL) {
		lv_label_set_text(g_calc.value_label, g_calc.input[0] == '\0' ? "0" : g_calc.input);
	}
}

static void calc_set_value(int64_t value)
{
	snprintf(g_calc.input, sizeof(g_calc.input), "%lld", (long long)value);
}

static int64_t calc_parse_input(void)
{
	uint32_t idx = 0;
	int64_t value = 0;
	int64_t sign = 1;

	if (g_calc.input[0] == '-') {
		sign = -1;
		idx = 1;
	}

	for (; g_calc.input[idx] != '\0'; idx++) {
		if (g_calc.input[idx] < '0' || g_calc.input[idx] > '9') {
			continue;
		}
		value = (value * 10) + (int64_t)(g_calc.input[idx] - '0');
	}

	return value * sign;
}

static void calc_clear(void)
{
	lv_obj_t *trace_label = g_calc.trace_label;
	lv_obj_t *value_label = g_calc.value_label;

	memset(&g_calc, 0, sizeof(g_calc));
	g_calc.trace_label = trace_label;
	g_calc.value_label = value_label;
	strcpy(g_calc.input, "0");
	strcpy(g_calc.trace, "Ready for input");
	calc_update_display();
}

static void calc_set_error(const char *message)
{
	g_calc.error = 1;
	strncpy(g_calc.input, "Error", sizeof(g_calc.input) - 1);
	g_calc.input[sizeof(g_calc.input) - 1] = '\0';
	strncpy(g_calc.trace, message, sizeof(g_calc.trace) - 1);
	g_calc.trace[sizeof(g_calc.trace) - 1] = '\0';
	calc_update_display();
}

static uint8_t calc_apply(int64_t lhs, int64_t rhs, char op, int64_t *out)
{
	if (out == NULL) {
		return 0;
	}

	switch (op) {
		case '+':
			*out = lhs + rhs;
			return 1;
		case '-':
			*out = lhs - rhs;
			return 1;
		case '*':
			*out = lhs * rhs;
			return 1;
		case '/':
			if (rhs == 0) {
				calc_set_error("Division by zero");
				return 0;
			}
			*out = lhs / rhs;
			return 1;
		default:
			*out = rhs;
			return 1;
	}
}

static void calc_append_digit(const char *digit)
{
	uint32_t current_len = 0;

	if (digit == NULL || digit[0] == '\0') {
		return;
	}

	if (g_calc.error) {
		calc_clear();
	}

	if (g_calc.reset_input || strcmp(g_calc.input, "0") == 0) {
		strncpy(g_calc.input, digit, sizeof(g_calc.input) - 1);
		g_calc.input[sizeof(g_calc.input) - 1] = '\0';
		g_calc.reset_input = 0;
		calc_update_display();
		return;
	}

	current_len = strlen(g_calc.input);
	if (current_len + strlen(digit) >= sizeof(g_calc.input)) {
		return;
	}

	strcat(g_calc.input, digit);
	calc_update_display();
}

static void calc_delete_digit(void)
{
	uint32_t len = 0;

	if (g_calc.error) {
		calc_clear();
		return;
	}

	if (g_calc.reset_input) {
		strcpy(g_calc.input, "0");
		g_calc.reset_input = 0;
		calc_update_display();
		return;
	}

	len = strlen(g_calc.input);
	if (len <= 1) {
		strcpy(g_calc.input, "0");
		calc_update_display();
		return;
	}

	g_calc.input[len - 1] = '\0';
	if (strcmp(g_calc.input, "-") == 0) {
		strcpy(g_calc.input, "0");
	}
	calc_update_display();
}

static void calc_operator(char op)
{
	int64_t current = 0;
	int64_t result = 0;

	if (g_calc.error) {
		calc_clear();
	}

	current = calc_parse_input();
	if (!g_calc.has_accumulator) {
		g_calc.accumulator = current;
		g_calc.has_accumulator = 1;
	} else if (g_calc.pending_op != 0 && !g_calc.reset_input) {
		if (!calc_apply(g_calc.accumulator, current, g_calc.pending_op, &result)) {
			return;
		}
		g_calc.accumulator = result;
		calc_set_value(result);
	}

	g_calc.pending_op = op;
	g_calc.reset_input = 1;
	snprintf(g_calc.trace, sizeof(g_calc.trace), "%lld %c",
		 (long long)g_calc.accumulator, g_calc.pending_op);
	calc_update_display();
}

static void calc_equal(void)
{
	int64_t lhs = 0;
	int64_t rhs = 0;
	int64_t result = 0;

	if (g_calc.error) {
		calc_clear();
		return;
	}

	if (!g_calc.has_accumulator || g_calc.pending_op == 0) {
		strcpy(g_calc.trace, "Result");
		calc_update_display();
		return;
	}

	lhs = g_calc.accumulator;
	rhs = calc_parse_input();
	if (!calc_apply(lhs, rhs, g_calc.pending_op, &result)) {
		return;
	}

	snprintf(g_calc.trace, sizeof(g_calc.trace), "%lld %c %lld =",
		 (long long)lhs, g_calc.pending_op, (long long)rhs);
	calc_set_value(result);
	g_calc.accumulator = 0;
	g_calc.pending_op = 0;
	g_calc.has_accumulator = 0;
	g_calc.reset_input = 1;
	calc_update_display();
}

static void calc_key_event_cb(lv_event_t *e)
{
	const char *key = NULL;

	if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
		return;
	}

	key = (const char *)lv_event_get_user_data(e);
	if (key == NULL) {
		return;
	}

	if (strcmp(key, "C") == 0) {
		calc_clear();
		return;
	}
	if (strcmp(key, "DEL") == 0) {
		calc_delete_digit();
		return;
	}
	if (strcmp(key, "=") == 0) {
		calc_equal();
		return;
	}
	if (strcmp(key, "+") == 0 || strcmp(key, "-") == 0 ||
	    strcmp(key, "*") == 0 || strcmp(key, "/") == 0) {
		calc_operator(key[0]);
		return;
	}

	calc_append_digit(key);
}

static lv_obj_t *create_key(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
			    const char *text, uint32_t bg, uint32_t text_color)
{
	lv_obj_t *btn = lv_btn_create(parent);
	lv_obj_t *label = lv_label_create(btn);

	lv_obj_set_pos(btn, x, y);
	lv_obj_set_size(btn, w, h);
	lv_obj_set_style_radius(btn, 22, 0);
	lv_obj_set_style_bg_color(btn, lv_color_hex(bg), 0);
	lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
	lv_obj_set_style_border_width(btn, 0, 0);
	lv_obj_set_style_shadow_width(btn, 0, 0);
	lv_obj_set_style_text_color(btn, lv_color_hex(text_color), 0);
	lv_obj_set_style_text_font(btn, &lv_font_montserrat_22, 0);
	lv_obj_add_event_cb(btn, calc_key_event_cb, LV_EVENT_CLICKED, (void *)text);

	lv_label_set_text(label, text);
	lv_obj_center(label);
	return btn;
}

static void create_ui(void)
{
	const calculator_theme_s *theme = calculator_theme();
	lv_obj_t *scr = lv_scr_act();
	lv_obj_t *shell = NULL;
	lv_obj_t *display = NULL;
	lv_obj_t *readout = NULL;
	lv_obj_t *keypad = NULL;
	lv_obj_t *badge = NULL;
	lv_obj_t *title = NULL;
	lv_obj_t *tip = NULL;
	lv_obj_t *footer = NULL;
	lv_obj_t *glow_left = NULL;
	lv_obj_t *glow_right = NULL;

	lv_obj_set_style_bg_color(scr, lv_color_hex(theme->bg), 0);
	lv_obj_set_style_bg_grad_color(scr, lv_color_hex(theme->bg_alt), 0);
	lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, 0);
	lv_obj_set_style_border_width(scr, 0, 0);
	clear_static_flags(scr);

	glow_left = lv_obj_create(scr);
	lv_obj_set_size(glow_left, 240, 240);
	lv_obj_set_pos(glow_left, -48, 110);
	lv_obj_set_style_radius(glow_left, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_bg_color(glow_left, lv_color_hex(theme->operator_bg), 0);
	lv_obj_set_style_bg_opa(glow_left, LV_OPA_20, 0);
	lv_obj_set_style_border_width(glow_left, 0, 0);
	clear_static_flags(glow_left);

	glow_right = lv_obj_create(scr);
	lv_obj_set_size(glow_right, 220, 220);
	lv_obj_set_pos(glow_right, 812, 188);
	lv_obj_set_style_radius(glow_right, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_bg_color(glow_right, lv_color_hex(theme->action_bg), 0);
	lv_obj_set_style_bg_opa(glow_right, LV_OPA_20, 0);
	lv_obj_set_style_border_width(glow_right, 0, 0);
	clear_static_flags(glow_right);

	shell = create_panel(scr, CONTENT_X, CONTENT_Y, CONTENT_W, CONTENT_H, theme->panel_alt, theme->line);
	display = create_panel(shell, 24, 24, 896, DISPLAY_H, theme->panel, theme->line);
	readout = create_panel(display, 520, 24, 344, 168, theme->panel_alt, theme->line);
	keypad = create_panel(shell, 24, KEYPAD_Y, 896, 540, theme->panel, theme->line);

	badge = lv_label_create(display);
	lv_obj_set_pos(badge, 28, 24);
	lv_obj_set_style_text_font(badge, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(badge, lv_color_hex(theme->operator_bg), 0);
	lv_label_set_text(badge, "DESK MATH");

	title = lv_label_create(display);
	lv_obj_set_pos(title, 28, 54);
	lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
	lv_obj_set_style_text_color(title, lv_color_hex(theme->text), 0);
	lv_label_set_text(title, "Calculator");

	tip = lv_label_create(display);
	lv_obj_set_pos(tip, 28, 118);
	lv_obj_set_width(tip, 360);
	lv_label_set_long_mode(tip, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(tip, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(tip, lv_color_hex(theme->dim), 0);
	lv_label_set_text(tip, "Quick arithmetic for desk work.\nUse the keypad or tap to compute.");

	g_calc.trace_label = lv_label_create(readout);
	lv_obj_set_pos(g_calc.trace_label, 20, 26);
	lv_obj_set_width(g_calc.trace_label, 304);
	lv_obj_set_style_text_align(g_calc.trace_label, LV_TEXT_ALIGN_RIGHT, 0);
	lv_obj_set_style_text_font(g_calc.trace_label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(g_calc.trace_label, lv_color_hex(theme->dim), 0);

	g_calc.value_label = lv_label_create(readout);
	lv_obj_set_pos(g_calc.value_label, 20, 78);
	lv_obj_set_width(g_calc.value_label, 304);
	lv_obj_set_style_text_align(g_calc.value_label, LV_TEXT_ALIGN_RIGHT, 0);
	lv_obj_set_style_text_font(g_calc.value_label, &lv_font_montserrat_32, 0);
	lv_obj_set_style_text_color(g_calc.value_label, lv_color_hex(theme->text), 0);

	tip = lv_label_create(keypad);
	lv_obj_set_pos(tip, 28, 22);
	lv_obj_set_style_text_font(tip, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(tip, lv_color_hex(theme->dim), 0);
	lv_label_set_text(tip, "Integer mode is on. Division keeps the whole-number result.");

	create_key(keypad, 28, 56, KEY_W, KEY_H, "C", theme->danger_bg, theme->danger_text);
	create_key(keypad, 28 + (KEY_W + KEY_GAP), 56, KEY_W, KEY_H, "DEL", theme->panel_alt, theme->text);
	create_key(keypad, 28 + ((KEY_W + KEY_GAP) * 2), 56, KEY_W, KEY_H, "/", theme->operator_bg, theme->operator_text);
	create_key(keypad, 28 + ((KEY_W + KEY_GAP) * 3), 56, KEY_W, KEY_H, "*", theme->operator_bg, theme->operator_text);

	create_key(keypad, 28, 56 + (KEY_H + KEY_GAP), KEY_W, KEY_H, "7", theme->number, theme->text);
	create_key(keypad, 28 + (KEY_W + KEY_GAP), 56 + (KEY_H + KEY_GAP), KEY_W, KEY_H, "8", theme->number, theme->text);
	create_key(keypad, 28 + ((KEY_W + KEY_GAP) * 2), 56 + (KEY_H + KEY_GAP), KEY_W, KEY_H, "9", theme->number, theme->text);
	create_key(keypad, 28 + ((KEY_W + KEY_GAP) * 3), 56 + (KEY_H + KEY_GAP), KEY_W, KEY_H, "-", theme->operator_bg, theme->operator_text);

	create_key(keypad, 28, 56 + ((KEY_H + KEY_GAP) * 2), KEY_W, KEY_H, "4", theme->number, theme->text);
	create_key(keypad, 28 + (KEY_W + KEY_GAP), 56 + ((KEY_H + KEY_GAP) * 2), KEY_W, KEY_H, "5", theme->number, theme->text);
	create_key(keypad, 28 + ((KEY_W + KEY_GAP) * 2), 56 + ((KEY_H + KEY_GAP) * 2), KEY_W, KEY_H, "6", theme->number, theme->text);
	create_key(keypad, 28 + ((KEY_W + KEY_GAP) * 3), 56 + ((KEY_H + KEY_GAP) * 2), KEY_W, KEY_H, "+", theme->operator_bg, theme->operator_text);

	create_key(keypad, 28, 56 + ((KEY_H + KEY_GAP) * 3), KEY_W, KEY_H, "1", theme->number, theme->text);
	create_key(keypad, 28 + (KEY_W + KEY_GAP), 56 + ((KEY_H + KEY_GAP) * 3), KEY_W, KEY_H, "2", theme->number, theme->text);
	create_key(keypad, 28 + ((KEY_W + KEY_GAP) * 2), 56 + ((KEY_H + KEY_GAP) * 3), KEY_W, KEY_H, "3", theme->number, theme->text);
	create_key(keypad, 28 + ((KEY_W + KEY_GAP) * 3), 56 + ((KEY_H + KEY_GAP) * 3), KEY_W, KEY_H, "=", theme->action_bg, theme->action_text);

	create_key(keypad, 28, 56 + ((KEY_H + KEY_GAP) * 4), ZERO_KEY_W, KEY_H, "0", theme->number, theme->text);

	footer = create_panel(keypad, 28 + ZERO_KEY_W + KEY_GAP, 56 + ((KEY_H + KEY_GAP) * 4),
			       416, KEY_H, theme->panel_alt, theme->line);
	tip = lv_label_create(footer);
	lv_obj_set_pos(tip, 22, 14);
	lv_obj_set_width(tip, 368);
	lv_label_set_long_mode(tip, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(tip, &lv_font_montserrat_32, 0);
	lv_obj_set_style_text_color(tip, lv_color_hex(theme->success), 0);
	lv_label_set_text(tip, "Use C to clear the stack. DEL removes the last digit.");
}

static void calculator_on_create(app_s *app)
{
	(void)app;

	(void)calculator_read_theme_mode(&g_theme_mode, &g_last_theme_revision);
	create_ui();
	calc_clear();
	log_info("calculator ready\n");
}

static void calculator_on_update(app_s *app, uint64_t mono_ms)
{
	(void)app;
	calculator_refresh_theme(mono_ms, 0U);
}

int main(int argc, char *argv[])
{
	app_s app = {0};
	app_config_s config = {
		.name = "calculator",
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

	log_info("calculator start!\n");
	lifecycle.on_create = calculator_on_create;
	lifecycle.on_update = calculator_on_update;
	app_init(&app, &config, &lifecycle, NULL);
	return app_run(&app);
}
