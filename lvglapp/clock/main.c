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
#define DIAL_PANEL_W 436
#define DIAL_PANEL_H CONTENT_H
#define DETAIL_X (CONTENT_X + DIAL_PANEL_W + 16)
#define DETAIL_W (CONTENT_W - DIAL_PANEL_W - 16)
#define HERO_H 336
#define INFO_Y (CONTENT_Y + HERO_H + 16)
#define INFO_H (CONTENT_H - HERO_H - 16)
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
#define COLOR_ALERT       0xf08d80

typedef enum clock_theme_mode {
	CLOCK_THEME_LIGHT = 0,
	CLOCK_THEME_DARK = 1,
} clock_theme_mode_e;

typedef struct clock_theme {
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
	uint32_t alert;
	uint32_t chip_text;
} clock_theme_s;

typedef struct clock_state {
	lv_obj_t *meter;
	lv_meter_indicator_t *second_arc;
	lv_meter_indicator_t *minute_arc;
	lv_meter_indicator_t *hour_hand;
	lv_meter_indicator_t *minute_hand;
	lv_meter_indicator_t *second_hand;
	lv_obj_t *time_label;
	lv_obj_t *seconds_label;
	lv_obj_t *date_label;
	lv_obj_t *weekday_chip;
	lv_obj_t *zone_chip;
	lv_obj_t *status_chip;
	lv_obj_t *hero_note;
	lv_obj_t *card_values[4];
	uint64_t last_second_key;
	uint8_t rtc_online;
} clock_state_s;

static const char *g_weekday_names[7] = {
	"Sunday", "Monday", "Tuesday", "Wednesday",
	"Thursday", "Friday", "Saturday",
};

static clock_state_s g_clock = {0};
static statemgr_client_s *g_statemgr = NULL;
static uint32_t g_theme_mode = CLOCK_THEME_LIGHT;
static uint64_t g_last_theme_ms = 0;
static uint64_t g_last_theme_revision = 0;

static const clock_theme_s g_clock_theme_light = {
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
	.alert = COLOR_ALERT,
	.chip_text = 0x132028,
};

static const clock_theme_s g_clock_theme_dark = {
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
	.alert = 0xf3a096,
	.chip_text = 0xfff6ef,
};

static const clock_theme_s *clock_theme(void)
{
	return g_theme_mode == CLOCK_THEME_DARK ?
		&g_clock_theme_dark : &g_clock_theme_light;
}

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
				    lv_coord_t w, lv_coord_t h, const char *title)
{
	const clock_theme_s *theme = clock_theme();
	lv_obj_t *card = create_panel(parent, x, y, w, h, theme->panel_alt, theme->line);
	lv_obj_t *title_label = lv_label_create(card);
	lv_obj_t *value_label = lv_label_create(card);

	lv_obj_set_pos(title_label, 18, 16);
	lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(title_label, lv_color_hex(theme->dim), 0);
	lv_label_set_text(title_label, title);

	lv_obj_set_pos(value_label, 18, 50);
	lv_obj_set_width(value_label, w - 36);
	lv_obj_set_style_text_font(value_label, &lv_font_montserrat_24, 0);
	lv_obj_set_style_text_color(value_label, lv_color_hex(theme->text), 0);
	lv_label_set_text(value_label, "--");

	return value_label;
}

static uint8_t clock_get_theme_revision(uint64_t *revision_out)
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

static uint8_t clock_read_theme_mode(uint32_t *theme_mode_out)
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

	*theme_mode_out = (uint32_t)response.entry.value_u64 == CLOCK_THEME_DARK ?
		CLOCK_THEME_DARK : CLOCK_THEME_LIGHT;
	return 1U;
}

static void clock_update_time(uint64_t mono_ms)
{
	const clock_theme_s *theme = clock_theme();
	app_datetime_s local_time = {0};
	uint64_t raw_ts = OSSysCtrlGetTimestamp();
	uint64_t ts = app_time_normalize_timestamp_seconds(raw_ts, mono_ms);
	uint64_t second_key = 0;
	char time_text[6];
	char seconds_text[4];
	char date_text[48];
	char chip_text[16];
	char note_text[128];
	char value_text[32];
	char year_text[5];
	char month_text[3];
	char day_text[3];
	char year_day_text[4];
	int day_progress = 0;
	int year_day = 0;
	int year_total = 0;
	int hour_value = 0;

	g_clock.rtc_online = raw_ts != 0U;
	if (!g_clock.rtc_online) {
		if (g_clock.last_second_key == UINT64_MAX) {
			return;
		}
		g_clock.last_second_key = UINT64_MAX;
		lv_label_set_text(g_clock.time_label, "--:--");
		lv_label_set_text(g_clock.seconds_label, "--");
		lv_label_set_text(g_clock.date_label, "RTC unavailable");
		lv_label_set_text(g_clock.weekday_chip, "Offline");
		lv_label_set_text(g_clock.zone_chip, "UTC+08");
		lv_label_set_text(g_clock.status_chip, "RTC offline");
		lv_obj_set_style_bg_color(g_clock.status_chip, lv_color_hex(theme->warm_soft), 0);
		lv_obj_set_style_text_color(g_clock.status_chip, lv_color_hex(theme->warm), 0);
		lv_label_set_text(g_clock.hero_note, "No wall-clock timestamp is exposed yet. The UI is ready and will switch live as soon as RTC starts responding.");
		lv_label_set_text(g_clock.card_values[0], "No source");
		lv_label_set_text(g_clock.card_values[1], "No data");
		lv_label_set_text(g_clock.card_values[2], "-- / --");
		lv_label_set_text(g_clock.card_values[3], "--");
		if (g_clock.meter != NULL) {
			lv_meter_set_indicator_end_value(g_clock.meter, g_clock.second_arc, 0);
			lv_meter_set_indicator_end_value(g_clock.meter, g_clock.minute_arc, 0);
			lv_meter_set_indicator_value(g_clock.meter, g_clock.hour_hand, 0);
			lv_meter_set_indicator_value(g_clock.meter, g_clock.minute_hand, 0);
			lv_meter_set_indicator_value(g_clock.meter, g_clock.second_hand, 0);
		}
		return;
	}

	app_time_timestamp_to_local_datetime(ts, APP_TIMEZONE_OFFSET_HOURS, &local_time);
	second_key = ((uint64_t)local_time.year * 10000000000ULL) +
		     ((uint64_t)local_time.month * 100000000ULL) +
		     ((uint64_t)local_time.day * 1000000ULL) +
		     ((uint64_t)local_time.hour * 10000ULL) +
		     ((uint64_t)local_time.minute * 100ULL) +
		     (uint64_t)local_time.second;
	if (second_key == g_clock.last_second_key) {
		return;
	}
	g_clock.last_second_key = second_key;

	app_time_format_fixed_u32(time_text, (uint32_t)local_time.hour, 2U);
	time_text[2] = ':';
	app_time_format_fixed_u32(time_text + 3, (uint32_t)local_time.minute, 2U);
	time_text[5] = '\0';
	app_time_format_fixed_u32(seconds_text, (uint32_t)local_time.second, 2U);
	app_time_format_fixed_u32(year_text, (uint32_t)local_time.year, 4U);
	app_time_format_fixed_u32(month_text, (uint32_t)local_time.month, 2U);
	app_time_format_fixed_u32(day_text, (uint32_t)local_time.day, 2U);
	snprintf(date_text, sizeof(date_text), "%s-%s-%s",
		 year_text, month_text, day_text);
	snprintf(chip_text, sizeof(chip_text), "%s",
		 g_weekday_names[local_time.weekday]);
	lv_label_set_text(g_clock.time_label, time_text);
	lv_label_set_text(g_clock.seconds_label, seconds_text);
	lv_label_set_text(g_clock.date_label, date_text);
	lv_label_set_text(g_clock.weekday_chip, chip_text);
	lv_label_set_text(g_clock.zone_chip, "UTC+08");
	lv_label_set_text(g_clock.status_chip, "RTC live");
	lv_obj_set_style_bg_color(g_clock.status_chip, lv_color_hex(theme->accent_soft), 0);
	lv_obj_set_style_text_color(g_clock.status_chip, lv_color_hex(theme->accent), 0);

	day_progress = (int)(((uint64_t)local_time.hour * 3600ULL +
			      (uint64_t)local_time.minute * 60ULL +
			      (uint64_t)local_time.second) * 100ULL / 86400ULL);
	year_day = app_time_day_of_year(local_time.year, local_time.month, local_time.day);
	year_total = app_time_days_in_year(local_time.year);
	snprintf(note_text, sizeof(note_text),
		 "Wall clock is sourced from RTC seconds, normalized if the firmware switches units, and rendered with the desktop timezone offset.");
	lv_label_set_text(g_clock.hero_note, note_text);

	snprintf(value_text, sizeof(value_text), "%s", g_weekday_names[local_time.weekday]);
	lv_label_set_text(g_clock.card_values[0], value_text);
	snprintf(value_text, sizeof(value_text), "%d%% complete", day_progress);
	lv_label_set_text(g_clock.card_values[1], value_text);
	app_time_format_fixed_u32(year_day_text, (uint32_t)year_day, 3U);
	snprintf(value_text, sizeof(value_text), "%s / %d", year_day_text, year_total);
	lv_label_set_text(g_clock.card_values[2], value_text);
	snprintf(value_text, sizeof(value_text), "%llu", (unsigned long long)ts);
	lv_label_set_text(g_clock.card_values[3], value_text);

	if (g_clock.meter != NULL) {
		hour_value = ((local_time.hour % 12) * 5) + (local_time.minute / 12);
		lv_meter_set_indicator_end_value(g_clock.meter, g_clock.second_arc, local_time.second);
		lv_meter_set_indicator_end_value(g_clock.meter, g_clock.minute_arc, local_time.minute);
		lv_meter_set_indicator_value(g_clock.meter, g_clock.hour_hand, hour_value);
		lv_meter_set_indicator_value(g_clock.meter, g_clock.minute_hand, local_time.minute);
		lv_meter_set_indicator_value(g_clock.meter, g_clock.second_hand, local_time.second);
	}
}

static void create_ui(void)
{
	const clock_theme_s *theme = clock_theme();
	lv_obj_t *scr = lv_scr_act();
	lv_obj_t *dial_panel = NULL;
	lv_obj_t *hero_panel = NULL;
	lv_obj_t *info_panel = NULL;
	lv_obj_t *label = NULL;
	lv_obj_t *center_dot = NULL;
	lv_meter_scale_t *scale_min = NULL;
	lv_meter_scale_t *scale_hour = NULL;

	lv_obj_set_style_bg_color(scr, lv_color_hex(theme->bg), 0);
	lv_obj_set_style_bg_grad_color(scr, lv_color_hex(theme->bg_alt), 0);
	lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, 0);
	lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
	lv_obj_set_style_border_width(scr, 0, 0);
	lv_obj_set_style_pad_all(scr, 0, 0);
	clear_static_flags(scr);

	dial_panel = create_panel(scr, CONTENT_X, CONTENT_Y, DIAL_PANEL_W, DIAL_PANEL_H,
				 theme->panel, theme->line);
	hero_panel = create_panel(scr, DETAIL_X, CONTENT_Y, DETAIL_W, HERO_H,
				 theme->panel, theme->line);
	info_panel = create_panel(scr, DETAIL_X, INFO_Y, DETAIL_W, INFO_H,
				 theme->panel, theme->line);

	label = lv_label_create(dial_panel);
	lv_obj_set_pos(label, 28, 28);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_32, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->text), 0);
	lv_label_set_text(label, "Clock");

	label = lv_label_create(dial_panel);
	lv_obj_set_pos(label, 28, 72);
	lv_obj_set_width(label, DIAL_PANEL_W - 56);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->dim), 0);
	lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
	lv_label_set_text(label, "A live desk clock with analog hands, digital readout, and RTC-backed date details.");

	g_clock.status_chip = create_chip(dial_panel, 28, 126, "RTC live",
					 theme->accent_soft, theme->accent);

	g_clock.meter = lv_meter_create(dial_panel);
	lv_obj_set_size(g_clock.meter, 320, 320);
	lv_obj_set_pos(g_clock.meter, 58, 208);
	lv_obj_set_style_bg_opa(g_clock.meter, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(g_clock.meter, 0, 0);
	lv_obj_set_style_pad_all(g_clock.meter, 18, 0);
	lv_obj_set_style_text_font(g_clock.meter, &lv_font_montserrat_16, LV_PART_TICKS);
	lv_obj_set_style_text_color(g_clock.meter, lv_color_hex(theme->text), LV_PART_TICKS);

	scale_min = lv_meter_add_scale(g_clock.meter);
	lv_meter_set_scale_ticks(g_clock.meter, scale_min, 61, 1, 10, lv_color_hex(theme->line));
	lv_meter_set_scale_range(g_clock.meter, scale_min, 0, 60, 360, 270);

	scale_hour = lv_meter_add_scale(g_clock.meter);
	lv_meter_set_scale_ticks(g_clock.meter, scale_hour, 12, 0, 0, lv_color_hex(theme->line));
	lv_meter_set_scale_major_ticks(g_clock.meter, scale_hour, 1, 2, 20,
					      lv_color_hex(theme->text), 10);
	lv_meter_set_scale_range(g_clock.meter, scale_hour, 1, 12, 330, 300);

	g_clock.second_arc = lv_meter_add_arc(g_clock.meter, scale_min, 6, lv_color_hex(theme->warm_soft), 8);
	g_clock.minute_arc = lv_meter_add_arc(g_clock.meter, scale_min, 10, lv_color_hex(theme->accent_soft), -6);
	g_clock.hour_hand = lv_meter_add_needle_line(g_clock.meter, scale_min, 6,
						      lv_color_hex(theme->text), -76);
	g_clock.minute_hand = lv_meter_add_needle_line(g_clock.meter, scale_min, 4,
							lv_color_hex(theme->accent), -42);
	g_clock.second_hand = lv_meter_add_needle_line(g_clock.meter, scale_min, 2,
							lv_color_hex(theme->warm), -18);
	center_dot = lv_obj_create(g_clock.meter);
	lv_obj_set_size(center_dot, 18, 18);
	lv_obj_align(center_dot, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_style_bg_color(center_dot, lv_color_hex(theme->text), 0);
	lv_obj_set_style_bg_opa(center_dot, LV_OPA_COVER, 0);
	lv_obj_set_style_border_width(center_dot, 0, 0);
	lv_obj_set_style_radius(center_dot, 999, 0);
	lv_obj_set_style_shadow_width(center_dot, 0, 0);
	clear_static_flags(center_dot);

	g_clock.date_label = lv_label_create(dial_panel);
	lv_obj_set_pos(g_clock.date_label, 28, 556);
	lv_obj_set_style_text_font(g_clock.date_label, &lv_font_montserrat_24, 0);
	lv_obj_set_style_text_color(g_clock.date_label, lv_color_hex(theme->text), 0);
	lv_label_set_text(g_clock.date_label, "0000-00-00");

	g_clock.weekday_chip = create_chip(dial_panel, 28, 604, "Monday",
					 theme->panel_alt, theme->text);
	g_clock.zone_chip = create_chip(dial_panel, 154, 604, "UTC+08",
				      theme->warm_soft, theme->warm);

	label = lv_label_create(hero_panel);
	lv_obj_set_pos(label, 28, 28);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->dim), 0);
	lv_label_set_text(label, "LOCAL TIME");

	g_clock.time_label = lv_label_create(hero_panel);
	lv_obj_set_pos(g_clock.time_label, 28, 74);
	lv_obj_set_style_text_font(g_clock.time_label, &lv_font_montserrat_48, 0);
	lv_obj_set_style_text_color(g_clock.time_label, lv_color_hex(theme->text), 0);
	lv_label_set_text(g_clock.time_label, "00:00");

	g_clock.seconds_label = lv_label_create(hero_panel);
	lv_obj_set_pos(g_clock.seconds_label, 318, 96);
	lv_obj_set_style_text_font(g_clock.seconds_label, &lv_font_montserrat_24, 0);
	lv_obj_set_style_text_color(g_clock.seconds_label, lv_color_hex(theme->accent), 0);
	lv_label_set_text(g_clock.seconds_label, "00");

	label = lv_label_create(hero_panel);
	lv_obj_set_pos(label, 28, 150);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->dim), 0);
	lv_label_set_text(label, "The digital readout follows the same timezone logic as the system bar.");

	g_clock.hero_note = lv_label_create(hero_panel);
	lv_obj_set_pos(g_clock.hero_note, 28, 206);
	lv_obj_set_width(g_clock.hero_note, DETAIL_W - 56);
	lv_label_set_long_mode(g_clock.hero_note, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(g_clock.hero_note, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(g_clock.hero_note, lv_color_hex(theme->text), 0);
	lv_label_set_text(g_clock.hero_note, "");

	g_clock.card_values[0] = create_metric_card(info_panel, 24, 24, 216, 144, "Weekday");
	g_clock.card_values[1] = create_metric_card(info_panel, 252, 24, 216, 144, "Day Progress");
	g_clock.card_values[2] = create_metric_card(info_panel, 24, 184, 216, 144, "Year Day");
	g_clock.card_values[3] = create_metric_card(info_panel, 252, 184, 216, 144, "Unix Time");
}

static void clock_rebuild_ui(void)
{
	memset(&g_clock, 0, sizeof(g_clock));
	lv_obj_clean(lv_scr_act());
	create_ui();
}

static void clock_refresh_theme(uint64_t mono_ms)
{
	uint32_t theme_mode = g_theme_mode;
	uint64_t revision = 0;

	if (mono_ms < g_last_theme_ms + THEME_REFRESH_INTERVAL_MS) {
		return;
	}
	g_last_theme_ms = mono_ms;

	if (!clock_get_theme_revision(&revision) || !clock_read_theme_mode(&theme_mode)) {
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
	clock_rebuild_ui();
	clock_update_time(mono_ms);
	log_info("clock theme synced: mode=%u rev=%llu\n",
		 (unsigned int)g_theme_mode,
		 (unsigned long long)revision);
}

static void clock_on_create(app_s *app)
{
	(void)app;

	g_statemgr = statemgr_client_get();
	(void)clock_read_theme_mode(&g_theme_mode);
	(void)clock_get_theme_revision(&g_last_theme_revision);
	create_ui();
	clock_update_time(OSSysCtrlGetMonoTime() / NSEC_PER_MSEC);
	log_info("clock ready\n");
}

static void clock_on_update(app_s *app, uint64_t mono_ms)
{
	(void)app;
	clock_refresh_theme(mono_ms);
	clock_update_time(mono_ms);
}

int main(int argc, char *argv[])
{
	app_s app = {0};
	app_config_s config = {
		.name = "clock",
		.x = APP_DEFAULT_X,
		.y = APP_DEFAULT_Y,
		.z = 0,
		.width = APP_DEFAULT_WIDTH,
		.height = APP_DEFAULT_HEIGHT,
		.window_flags = WINDOW_FLAG_FOCUSABLE,
		.enable_input = 1,
	};
	app_lifecycle_ops_s lifecycle = {
		.on_create = clock_on_create,
		.on_update = clock_on_update,
	};

	(void)argc;
	(void)argv;

	log_info("clock start!\n");
	app_init(&app, &config, &lifecycle, NULL);
	return app_run(&app);
}
