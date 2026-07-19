#include "lvgl.h"
#include "app_time.h"
#include "app.h"
#include "libwindow/window.h"
#include "log.h"
#include "libkernel/capcall.h"
#include "libsystem/statemgr_client.h"
#include "stdio.h"
#include "string.h"
#include "stdint.h"

#define SYSTEM_OVERLAY_GAP 24
#define CONTENT_X 40
#define CONTENT_Y SYSTEM_OVERLAY_GAP
#define CONTENT_W 944
#define CONTENT_H (APP_DEFAULT_HEIGHT - CONTENT_Y - SYSTEM_OVERLAY_GAP)
#define HEADER_H 156
#define BODY_Y (CONTENT_Y + HEADER_H + 16)
#define BODY_H (CONTENT_H - HEADER_H - 16)
#define GRID_W 620
#define DETAIL_X (CONTENT_X + GRID_W + 16)
#define DETAIL_W (CONTENT_W - GRID_W - 16)
#define GRID_CELL_W 76
#define GRID_CELL_H 68
#define GRID_COL_GAP 8
#define GRID_ROW_GAP 10
#define GRID_START_X 20
#define GRID_START_Y 92
#define THEME_REFRESH_INTERVAL_MS 100ULL

#define COLOR_BG          0xfff4ee
#define COLOR_BG_ALT      0xeaf4ff
#define COLOR_PANEL       0xfffffb
#define COLOR_PANEL_ALT   0xfff1eb
#define COLOR_LINE        0xf0ddd2
#define COLOR_TEXT        0x24324a
#define COLOR_DIM         0x7d8198
#define COLOR_ACCENT      0xff7d5c
#define COLOR_ACCENT_SOFT 0xffe6dd
#define COLOR_WARM        0xffb85e
#define COLOR_WARM_SOFT   0xfff0d2
#define COLOR_SUCCESS     0x4cbf95

typedef enum calendar_theme_mode {
	CALENDAR_THEME_LIGHT = 0,
	CALENDAR_THEME_DARK = 1,
} calendar_theme_mode_e;

typedef enum calendar_nav_action {
	CALENDAR_NAV_PREV = 1,
	CALENDAR_NAV_NEXT = 2,
	CALENDAR_NAV_TODAY = 3,
} calendar_nav_action_e;

typedef struct calendar_theme {
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
	uint32_t success;
	uint32_t chip_text;
} calendar_theme_s;

typedef struct calendar_state {
	lv_obj_t *month_title;
	lv_obj_t *summary_label;
	lv_obj_t *status_chip;
	lv_obj_t *detail_title;
	lv_obj_t *detail_subtitle;
	lv_obj_t *detail_chip;
	lv_obj_t *detail_note;
	lv_obj_t *metric_values[4];
	lv_obj_t *grid_buttons[42];
	lv_obj_t *grid_labels[42];
	int cell_year[42];
	int cell_month[42];
	int cell_day[42];
	uint8_t cell_in_month[42];
	int today_year;
	int today_month;
	int today_day;
	int today_weekday;
	int view_year;
	int view_month;
	int selected_year;
	int selected_month;
	int selected_day;
	uint64_t last_day_key;
	uint8_t rtc_online;
} calendar_state_s;

static const char *g_weekday_names[7] = {
	"Sunday", "Monday", "Tuesday", "Wednesday",
	"Thursday", "Friday", "Saturday",
};

static const char *g_weekday_short[7] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat",
};

static const char *g_month_names[12] = {
	"January", "February", "March", "April",
	"May", "June", "July", "August",
	"September", "October", "November", "December",
};

static calendar_state_s g_calendar = {0};
static statemgr_client_s *g_statemgr = NULL;
static uint32_t g_theme_mode = CALENDAR_THEME_LIGHT;
static uint64_t g_last_theme_ms = 0;
static uint64_t g_last_theme_revision = 0;

static const calendar_theme_s g_calendar_theme_light = {
	.bg = COLOR_BG,
	.bg_alt = COLOR_BG_ALT,
	.panel = COLOR_PANEL,
	.panel_alt = COLOR_PANEL_ALT,
	.line = COLOR_LINE,
	.text = COLOR_TEXT,
	.dim = COLOR_DIM,
	.accent = COLOR_ACCENT,
	.accent_soft = COLOR_ACCENT_SOFT,
	.warm = COLOR_WARM,
	.warm_soft = COLOR_WARM_SOFT,
	.success = COLOR_SUCCESS,
	.chip_text = 0x132028,
};

static const calendar_theme_s g_calendar_theme_dark = {
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
	.success = 0x68d7ad,
	.chip_text = 0xfff6ef,
};

static const calendar_theme_s *calendar_theme(void)
{
	return g_theme_mode == CALENDAR_THEME_DARK ?
		&g_calendar_theme_dark : &g_calendar_theme_light;
}

static void calendar_refresh_ui(void);
static void calendar_nav_event_cb(lv_event_t *e);
static void calendar_day_event_cb(lv_event_t *e);

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

static lv_obj_t *create_nav_button(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
				   lv_coord_t w, const char *text,
				   calendar_nav_action_e action)
{
	const calendar_theme_s *theme = calendar_theme();
	lv_obj_t *btn = lv_btn_create(parent);
	lv_obj_t *label = lv_label_create(btn);

	lv_obj_set_pos(btn, x, y);
	lv_obj_set_size(btn, w, 34);
	lv_obj_set_style_radius(btn, 14, 0);
	lv_obj_set_style_bg_color(btn, lv_color_hex(theme->panel_alt), 0);
	lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
	lv_obj_set_style_border_color(btn, lv_color_hex(theme->line), 0);
	lv_obj_set_style_border_width(btn, 1, 0);
	lv_obj_set_style_shadow_width(btn, 0, 0);
	lv_obj_add_event_cb(btn, calendar_nav_event_cb, LV_EVENT_CLICKED,
			    (void *)(uintptr_t)action);

	lv_obj_center(label);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->text), 0);
	lv_label_set_text(label, text);
	return btn;
}

static lv_obj_t *create_metric_card(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
				    lv_coord_t w, lv_coord_t h, const char *title)
{
	const calendar_theme_s *theme = calendar_theme();
	lv_obj_t *card = create_panel(parent, x, y, w, h, theme->panel_alt, theme->line);
	lv_obj_t *title_label = lv_label_create(card);
	lv_obj_t *value_label = lv_label_create(card);

	lv_obj_set_pos(title_label, 14, 14);
	lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(title_label, lv_color_hex(theme->dim), 0);
	lv_label_set_text(title_label, title);

	lv_obj_set_pos(value_label, 14, 48);
	lv_obj_set_width(value_label, w - 28);
	lv_obj_set_style_text_font(value_label, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(value_label, lv_color_hex(theme->text), 0);
	lv_label_set_long_mode(value_label, LV_LABEL_LONG_WRAP);
	lv_label_set_text(value_label, "--");

	return value_label;
}

static uint8_t calendar_get_theme_revision(uint64_t *revision_out)
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

static uint8_t calendar_read_theme_mode(uint32_t *theme_mode_out)
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

	*theme_mode_out = (uint32_t)response.entry.value_u64 == CALENDAR_THEME_DARK ?
		CALENDAR_THEME_DARK : CALENDAR_THEME_LIGHT;
	return 1U;
}

static void calendar_normalize_year_month(int *year, int *month)
{
	if (year == NULL || month == NULL) {
		return;
	}

	while (*month < 1) {
		*month += 12;
		(*year)--;
	}
	while (*month > 12) {
		*month -= 12;
		(*year)++;
	}
}

static void calendar_select_date(int year, int month, int day)
{
	int month_days = app_time_days_in_month(year, month);

	if (day < 1) {
		day = 1;
	}
	if (day > month_days) {
		day = month_days;
	}
	g_calendar.selected_year = year;
	g_calendar.selected_month = month;
	g_calendar.selected_day = day;
}

static void calendar_set_view_month(int year, int month, uint8_t preserve_day)
{
	int selected_day = g_calendar.selected_day == 0 ? 1 : g_calendar.selected_day;

	calendar_normalize_year_month(&year, &month);
	g_calendar.view_year = year;
	g_calendar.view_month = month;
	if (!preserve_day) {
		selected_day = 1;
	}
	calendar_select_date(year, month, selected_day);
}

static void calendar_shift_month(int delta)
{
	int year = g_calendar.view_year;
	int month = g_calendar.view_month + delta;

	calendar_normalize_year_month(&year, &month);
	calendar_set_view_month(year, month, 1U);
	calendar_refresh_ui();
}

static void calendar_jump_today(void)
{
	calendar_set_view_month(g_calendar.today_year, g_calendar.today_month, 0U);
	calendar_select_date(g_calendar.today_year, g_calendar.today_month, g_calendar.today_day);
	calendar_refresh_ui();
}

static void calendar_refresh_header(void)
{
	const calendar_theme_s *theme = calendar_theme();
	char title_text[48];
	char summary_text[160];
	int first_weekday = app_time_weekday(g_calendar.view_year, g_calendar.view_month, 1);
	int month_days = app_time_days_in_month(g_calendar.view_year, g_calendar.view_month);

	snprintf(title_text, sizeof(title_text), "%s %d",
		 g_month_names[g_calendar.view_month - 1], g_calendar.view_year);
	lv_label_set_text(g_calendar.month_title, title_text);
	if (g_calendar.rtc_online) {
		snprintf(summary_text, sizeof(summary_text),
			 "%d days, starting on %s. Tap any date to focus the detail card and use Today to snap back.",
			 month_days, g_weekday_names[first_weekday]);
		lv_label_set_text(g_calendar.status_chip, "RTC live");
		lv_obj_set_style_bg_color(g_calendar.status_chip, lv_color_hex(theme->accent_soft), 0);
		lv_obj_set_style_text_color(g_calendar.status_chip, lv_color_hex(theme->accent), 0);
	} else {
		snprintf(summary_text, sizeof(summary_text),
			 "%d days, starting on %s. RTC is offline, so the view falls back to the epoch calendar until timestamps arrive.",
			 month_days, g_weekday_names[first_weekday]);
		lv_label_set_text(g_calendar.status_chip, "RTC offline");
		lv_obj_set_style_bg_color(g_calendar.status_chip, lv_color_hex(theme->warm_soft), 0);
		lv_obj_set_style_text_color(g_calendar.status_chip, lv_color_hex(theme->warm), 0);
	}
	lv_label_set_text(g_calendar.summary_label, summary_text);
}

static void calendar_refresh_details(void)
{
	char title_text[64];
	char subtitle_text[96];
	char note_text[160];
	char value_text[32];
	char year_text[5];
	char year_day_text[4];
	int weekday = app_time_weekday(g_calendar.selected_year, g_calendar.selected_month,
				      g_calendar.selected_day);
	int first_weekday = app_time_weekday(g_calendar.selected_year, g_calendar.selected_month, 1);
	int week_of_month = ((first_weekday + g_calendar.selected_day - 1) / 7) + 1;
	int year_day = app_time_day_of_year(g_calendar.selected_year, g_calendar.selected_month,
					    g_calendar.selected_day);
	int year_total = app_time_days_in_year(g_calendar.selected_year);
	int month_days = app_time_days_in_month(g_calendar.selected_year, g_calendar.selected_month);
	int days_left = month_days - g_calendar.selected_day;
	uint8_t is_today = g_calendar.selected_year == g_calendar.today_year &&
			  g_calendar.selected_month == g_calendar.today_month &&
			  g_calendar.selected_day == g_calendar.today_day;
	const calendar_theme_s *theme = calendar_theme();

	snprintf(title_text, sizeof(title_text), "%s %d",
		 g_month_names[g_calendar.selected_month - 1], g_calendar.selected_day);
	app_time_format_fixed_u32(year_text, (uint32_t)g_calendar.selected_year, 4U);
	snprintf(subtitle_text, sizeof(subtitle_text), "%s, %s",
		 g_weekday_names[weekday], year_text);
	lv_label_set_text(g_calendar.detail_title, title_text);
	lv_label_set_text(g_calendar.detail_subtitle, subtitle_text);
	lv_label_set_text(g_calendar.detail_chip, is_today ? "Today" : "Focused");
	lv_obj_set_style_bg_color(g_calendar.detail_chip,
				 lv_color_hex(is_today ? theme->accent_soft : theme->panel_alt), 0);
	lv_obj_set_style_text_color(g_calendar.detail_chip,
				   lv_color_hex(is_today ? theme->accent : theme->text), 0);

	if (is_today) {
		snprintf(note_text, sizeof(note_text),
			 "This is the live desk date. Midnight rollover will update the highlight and keep this detail card anchored to the new day.");
	} else if (g_calendar.selected_month != g_calendar.view_month ||
		   g_calendar.selected_year != g_calendar.view_year) {
		snprintf(note_text, sizeof(note_text),
			 "This date lives just outside the current month grid. Selecting it also pivots the whole view into its month.");
	} else {
		snprintf(note_text, sizeof(note_text),
			 "Focused date in the current month. Use the left and right arrows to move one month at a time without losing the selected day when possible.");
	}
	lv_label_set_text(g_calendar.detail_note, note_text);

	snprintf(value_text, sizeof(value_text), "%s", g_weekday_names[weekday]);
	lv_label_set_text(g_calendar.metric_values[0], value_text);
	snprintf(value_text, sizeof(value_text), "Week %d", week_of_month);
	lv_label_set_text(g_calendar.metric_values[1], value_text);
	app_time_format_fixed_u32(year_day_text, (uint32_t)year_day, 3U);
	snprintf(value_text, sizeof(value_text), "%s / %d", year_day_text, year_total);
	lv_label_set_text(g_calendar.metric_values[2], value_text);
	snprintf(value_text, sizeof(value_text), "%d left", days_left);
	lv_label_set_text(g_calendar.metric_values[3], value_text);
}

static void calendar_refresh_grid(void)
{
	const calendar_theme_s *theme = calendar_theme();
	int first_weekday = app_time_weekday(g_calendar.view_year, g_calendar.view_month, 1);
	int month_days = app_time_days_in_month(g_calendar.view_year, g_calendar.view_month);
	int prev_year = g_calendar.view_year;
	int prev_month = g_calendar.view_month - 1;
	int next_year = g_calendar.view_year;
	int next_month = g_calendar.view_month + 1;
	int prev_month_days = 0;
	char label_text[4];

	calendar_normalize_year_month(&prev_year, &prev_month);
	calendar_normalize_year_month(&next_year, &next_month);
	prev_month_days = app_time_days_in_month(prev_year, prev_month);

	for (int i = 0; i < 42; i++) {
		int row = i / 7;
		int col = i % 7;
		int day = 0;
		int year = g_calendar.view_year;
		int month = g_calendar.view_month;
		uint8_t in_month = 1U;
		uint8_t is_today = 0U;
		uint8_t is_selected = 0U;
		uint8_t is_weekend = col == 0 || col == 6;
		uint32_t bg = theme->panel_alt;
		uint32_t border = theme->line;
		uint32_t text = theme->text;

		(void)row;

		if (i < first_weekday) {
			day = prev_month_days - first_weekday + i + 1;
			year = prev_year;
			month = prev_month;
			in_month = 0U;
		} else if (i >= first_weekday + month_days) {
			day = i - first_weekday - month_days + 1;
			year = next_year;
			month = next_month;
			in_month = 0U;
		} else {
			day = i - first_weekday + 1;
		}

		g_calendar.cell_year[i] = year;
		g_calendar.cell_month[i] = month;
		g_calendar.cell_day[i] = day;
		g_calendar.cell_in_month[i] = in_month;
		is_today = year == g_calendar.today_year &&
			   month == g_calendar.today_month &&
			   day == g_calendar.today_day;
		is_selected = year == g_calendar.selected_year &&
			     month == g_calendar.selected_month &&
			     day == g_calendar.selected_day;

		if (!in_month) {
			bg = theme->bg_alt;
			text = theme->dim;
		}
		if (is_weekend && in_month) {
			text = theme->warm;
		}
		if (is_today) {
			bg = theme->accent_soft;
			border = theme->accent;
			text = theme->text;
		}
		if (is_selected) {
			bg = theme->accent;
			border = theme->accent;
			text = theme->chip_text;
		}

		snprintf(label_text, sizeof(label_text), "%d", day);
		lv_label_set_text(g_calendar.grid_labels[i], label_text);
		lv_obj_set_style_bg_color(g_calendar.grid_buttons[i], lv_color_hex(bg), 0);
		lv_obj_set_style_bg_opa(g_calendar.grid_buttons[i], LV_OPA_COVER, 0);
		lv_obj_set_style_border_color(g_calendar.grid_buttons[i], lv_color_hex(border), 0);
		lv_obj_set_style_border_width(g_calendar.grid_buttons[i], is_selected || is_today ? 2 : 1, 0);
		lv_obj_set_style_text_color(g_calendar.grid_labels[i], lv_color_hex(text), 0);
	}
}

static void calendar_refresh_ui(void)
{
	calendar_refresh_header();
	calendar_refresh_grid();
	calendar_refresh_details();
}

static void create_ui(void)
{
	const calendar_theme_s *theme = calendar_theme();
	lv_obj_t *scr = lv_scr_act();
	lv_obj_t *header_panel = NULL;
	lv_obj_t *grid_panel = NULL;
	lv_obj_t *detail_panel = NULL;
	lv_obj_t *label = NULL;

	lv_obj_set_style_bg_color(scr, lv_color_hex(theme->bg), 0);
	lv_obj_set_style_bg_grad_color(scr, lv_color_hex(theme->bg_alt), 0);
	lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, 0);
	lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
	lv_obj_set_style_border_width(scr, 0, 0);
	lv_obj_set_style_pad_all(scr, 0, 0);
	clear_static_flags(scr);

	header_panel = create_panel(scr, CONTENT_X, CONTENT_Y, CONTENT_W, HEADER_H,
				    theme->panel, theme->line);
	grid_panel = create_panel(scr, CONTENT_X, BODY_Y, GRID_W, BODY_H,
				  theme->panel, theme->line);
	detail_panel = create_panel(scr, DETAIL_X, BODY_Y, DETAIL_W, BODY_H,
				    theme->panel, theme->line);

	label = lv_label_create(header_panel);
	lv_obj_set_pos(label, 28, 26);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_32, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->text), 0);
	lv_label_set_text(label, "Calendar");

	label = lv_label_create(header_panel);
	lv_obj_set_pos(label, 28, 74);
	lv_obj_set_width(label, 464);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->dim), 0);
	lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
	lv_label_set_text(label, "Month view with live today highlight, flip-month controls, and a focused-day detail rail.");

	g_calendar.status_chip = create_chip(header_panel, 28, 118, "RTC live",
					      theme->accent_soft, theme->accent);
	g_calendar.month_title = lv_label_create(header_panel);
	lv_obj_set_pos(g_calendar.month_title, 554, 28);
	lv_obj_set_style_text_font(g_calendar.month_title, &lv_font_montserrat_32, 0);
	lv_obj_set_style_text_color(g_calendar.month_title, lv_color_hex(theme->text), 0);
	lv_label_set_text(g_calendar.month_title, "Month 0000");

	g_calendar.summary_label = lv_label_create(header_panel);
	lv_obj_set_pos(g_calendar.summary_label, 554, 76);
	lv_obj_set_width(g_calendar.summary_label, 360);
	lv_obj_set_style_text_font(g_calendar.summary_label, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(g_calendar.summary_label, lv_color_hex(theme->dim), 0);
	lv_label_set_long_mode(g_calendar.summary_label, LV_LABEL_LONG_WRAP);
	lv_label_set_text(g_calendar.summary_label, "");

	(void)create_nav_button(header_panel, 554, 116, 40, "<", CALENDAR_NAV_PREV);
	(void)create_nav_button(header_panel, 604, 116, 40, ">", CALENDAR_NAV_NEXT);
	(void)create_nav_button(header_panel, 654, 116, 78, "Today", CALENDAR_NAV_TODAY);

	label = lv_label_create(grid_panel);
	lv_obj_set_pos(label, 20, 22);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->dim), 0);
	lv_label_set_text(label, "MONTH VIEW");

	for (int i = 0; i < 7; i++) {
		lv_obj_t *weekday = lv_label_create(grid_panel);

		lv_obj_set_pos(weekday, GRID_START_X + i * (GRID_CELL_W + GRID_COL_GAP) + 18, 58);
		lv_obj_set_style_text_font(weekday, &lv_font_montserrat_14, 0);
		lv_obj_set_style_text_color(weekday, lv_color_hex(theme->dim), 0);
		lv_label_set_text(weekday, g_weekday_short[i]);
	}

	for (int i = 0; i < 42; i++) {
		int row = i / 7;
		int col = i % 7;
		lv_obj_t *btn = lv_btn_create(grid_panel);
		lv_obj_t *day_label = lv_label_create(btn);

		lv_obj_set_pos(btn,
			       GRID_START_X + col * (GRID_CELL_W + GRID_COL_GAP),
			       GRID_START_Y + row * (GRID_CELL_H + GRID_ROW_GAP));
		lv_obj_set_size(btn, GRID_CELL_W, GRID_CELL_H);
		lv_obj_set_style_radius(btn, 18, 0);
		lv_obj_set_style_shadow_width(btn, 0, 0);
		lv_obj_set_style_border_width(btn, 1, 0);
		lv_obj_set_style_pad_all(btn, 0, 0);
		lv_obj_add_event_cb(btn, calendar_day_event_cb, LV_EVENT_CLICKED,
				    (void *)(uintptr_t)i);

		lv_obj_center(day_label);
		lv_obj_set_style_text_font(day_label, &lv_font_montserrat_24, 0);
		lv_label_set_text(day_label, "1");

		g_calendar.grid_buttons[i] = btn;
		g_calendar.grid_labels[i] = day_label;
	}

	g_calendar.detail_title = lv_label_create(detail_panel);
	lv_obj_set_pos(g_calendar.detail_title, 20, 24);
	lv_obj_set_width(g_calendar.detail_title, DETAIL_W - 40);
	lv_obj_set_style_text_font(g_calendar.detail_title, &lv_font_montserrat_32, 0);
	lv_obj_set_style_text_color(g_calendar.detail_title, lv_color_hex(theme->text), 0);
	lv_label_set_text(g_calendar.detail_title, "Month 0");

	g_calendar.detail_subtitle = lv_label_create(detail_panel);
	lv_obj_set_pos(g_calendar.detail_subtitle, 20, 72);
	lv_obj_set_width(g_calendar.detail_subtitle, DETAIL_W - 40);
	lv_obj_set_style_text_font(g_calendar.detail_subtitle, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(g_calendar.detail_subtitle, lv_color_hex(theme->dim), 0);
	lv_label_set_text(g_calendar.detail_subtitle, "");

	g_calendar.detail_chip = create_chip(detail_panel, 20, 106, "Focused",
					      theme->panel_alt, theme->text);

	g_calendar.detail_note = lv_label_create(detail_panel);
	lv_obj_set_pos(g_calendar.detail_note, 20, 156);
	lv_obj_set_width(g_calendar.detail_note, DETAIL_W - 40);
	lv_label_set_long_mode(g_calendar.detail_note, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(g_calendar.detail_note, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(g_calendar.detail_note, lv_color_hex(theme->text), 0);
	lv_label_set_text(g_calendar.detail_note, "");

	g_calendar.metric_values[0] = create_metric_card(detail_panel, 20, 274, 128, 108, "Weekday");
	g_calendar.metric_values[1] = create_metric_card(detail_panel, 160, 274, 128, 108, "Week Row");
	g_calendar.metric_values[2] = create_metric_card(detail_panel, 20, 394, 128, 108, "Year Day");
	g_calendar.metric_values[3] = create_metric_card(detail_panel, 160, 394, 128, 108, "Days Left");
}

static void calendar_sync_time(uint64_t mono_ms)
{
	app_datetime_s local_time = {0};
	uint64_t raw_ts = OSSysCtrlGetTimestamp();
	uint64_t ts = app_time_normalize_timestamp_seconds(raw_ts, mono_ms);
	uint64_t day_key = 0;
	int previous_today_year = g_calendar.today_year;
	int previous_today_month = g_calendar.today_month;
	int previous_today_day = g_calendar.today_day;
	uint8_t selected_was_today = g_calendar.selected_year == previous_today_year &&
				    g_calendar.selected_month == previous_today_month &&
				    g_calendar.selected_day == previous_today_day;

	g_calendar.rtc_online = raw_ts != 0U;
	app_time_timestamp_to_local_datetime(ts, APP_TIMEZONE_OFFSET_HOURS, &local_time);
	g_calendar.today_year = local_time.year;
	g_calendar.today_month = local_time.month;
	g_calendar.today_day = local_time.day;
	g_calendar.today_weekday = local_time.weekday;
	day_key = ((uint64_t)local_time.year * 10000ULL) +
		  ((uint64_t)local_time.month * 100ULL) +
		  (uint64_t)local_time.day;

	if (g_calendar.view_year == 0 || g_calendar.view_month == 0) {
		g_calendar.view_year = local_time.year;
		g_calendar.view_month = local_time.month;
		calendar_select_date(local_time.year, local_time.month, local_time.day);
		g_calendar.last_day_key = day_key;
		return;
	}

	if (g_calendar.last_day_key == day_key && previous_today_year != 0) {
		return;
	}

	if (g_calendar.view_year == previous_today_year &&
	    g_calendar.view_month == previous_today_month) {
		g_calendar.view_year = local_time.year;
		g_calendar.view_month = local_time.month;
	}
	if (selected_was_today || g_calendar.selected_year == 0) {
		calendar_select_date(local_time.year, local_time.month, local_time.day);
	}
	g_calendar.last_day_key = day_key;
	calendar_refresh_ui();
}

static void calendar_rebuild_ui(void)
{
	memset(&g_calendar.grid_buttons, 0, sizeof(g_calendar.grid_buttons));
	memset(&g_calendar.grid_labels, 0, sizeof(g_calendar.grid_labels));
	g_calendar.month_title = NULL;
	g_calendar.summary_label = NULL;
	g_calendar.status_chip = NULL;
	g_calendar.detail_title = NULL;
	g_calendar.detail_subtitle = NULL;
	g_calendar.detail_chip = NULL;
	g_calendar.detail_note = NULL;
	memset(&g_calendar.metric_values, 0, sizeof(g_calendar.metric_values));
	lv_obj_clean(lv_scr_act());
	create_ui();
	calendar_refresh_ui();
}

static void calendar_refresh_theme(uint64_t mono_ms)
{
	uint32_t theme_mode = g_theme_mode;
	uint64_t revision = 0;

	if (mono_ms < g_last_theme_ms + THEME_REFRESH_INTERVAL_MS) {
		return;
	}
	g_last_theme_ms = mono_ms;

	if (!calendar_get_theme_revision(&revision) || !calendar_read_theme_mode(&theme_mode)) {
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
	calendar_rebuild_ui();
	log_info("calendar theme synced: mode=%u rev=%llu\n",
		 (unsigned int)g_theme_mode,
		 (unsigned long long)revision);
}

static void calendar_nav_event_cb(lv_event_t *e)
{
	uintptr_t action = (uintptr_t)lv_event_get_user_data(e);

	if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
		return;
	}

	switch ((calendar_nav_action_e)action) {
	case CALENDAR_NAV_PREV:
		calendar_shift_month(-1);
		break;
	case CALENDAR_NAV_NEXT:
		calendar_shift_month(1);
		break;
	case CALENDAR_NAV_TODAY:
		calendar_jump_today();
		break;
	default:
		break;
	}
}

static void calendar_day_event_cb(lv_event_t *e)
{
	uintptr_t index = (uintptr_t)lv_event_get_user_data(e);

	if (lv_event_get_code(e) != LV_EVENT_CLICKED || index >= 42U) {
		return;
	}

	calendar_set_view_month(g_calendar.cell_year[index], g_calendar.cell_month[index], 0U);
	calendar_select_date(g_calendar.cell_year[index],
			    g_calendar.cell_month[index],
			    g_calendar.cell_day[index]);
	calendar_refresh_ui();
}

static void calendar_on_create(app_s *app)
{
	(void)app;

	g_statemgr = statemgr_client_get();
	(void)calendar_read_theme_mode(&g_theme_mode);
	(void)calendar_get_theme_revision(&g_last_theme_revision);
	calendar_sync_time(OSSysCtrlGetMonoTime() / NSEC_PER_MSEC);
	create_ui();
	calendar_refresh_ui();
	log_info("calendar ready\n");
}

static void calendar_on_update(app_s *app, uint64_t mono_ms)
{
	(void)app;
	calendar_refresh_theme(mono_ms);
	calendar_sync_time(mono_ms);
}

int main(int argc, char *argv[])
{
	app_s app = {0};
	app_config_s config = {
		.name = "calendar",
		.x = APP_DEFAULT_X,
		.y = APP_DEFAULT_Y,
		.z = 0,
		.width = APP_DEFAULT_WIDTH,
		.height = APP_DEFAULT_HEIGHT,
		.window_flags = WINDOW_FLAG_FOCUSABLE,
		.enable_input = 1,
	};
	app_lifecycle_ops_s lifecycle = {
		.on_create = calendar_on_create,
		.on_update = calendar_on_update,
	};

	(void)argc;
	(void)argv;

	log_info("calendar start!\n");
	app_init(&app, &config, &lifecycle, NULL);
	return app_run(&app);
}
