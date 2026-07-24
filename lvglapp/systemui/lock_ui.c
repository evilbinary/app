#include "lvgl.h"
#include "app_time.h"
#include "app.h"
#include "stdint.h"
#include "stdio.h"
#include "string.h"
#include "liblvgl/lv_port_disp.h"
#include "liblvgl/lv_port_indev.h"
#include "libsystem/ipc.h"
#include "libwindow/window.h"
#include "log.h"
#include "systemui_shared.h"

#define LOCKSCREEN_WIDTH 1024
#define LOCKSCREEN_HEIGHT 1024
#define LOCKSCREEN_UNLOCK_THRESHOLD 48
#define LOCKSCREEN_UNLOCK_TRAVEL 320

#define LOCKSCREEN_PANEL_WIDTH 456
#define LOCKSCREEN_PANEL_HEIGHT 136
#define LOCKSCREEN_PANEL_X ((LOCKSCREEN_WIDTH - LOCKSCREEN_PANEL_WIDTH) / 2)
#define LOCKSCREEN_PANEL_Y (LOCKSCREEN_HEIGHT - LOCKSCREEN_PANEL_HEIGHT - 64)

typedef struct lockscreen_theme {
	uint32_t text;
	uint32_t text_secondary;
	uint32_t dim;
	uint32_t chip_bg;
	uint32_t chip_fg;
	uint32_t chip_border;
	uint32_t panel;
	uint32_t panel_border;
	uint32_t accent;
	uint32_t accent_soft;
	uint32_t hero_fill;
	uint32_t hero_ring;
} lockscreen_theme_s;

static systemui_runtime_state_s g_state = {0};
static lv_obj_t *g_root = NULL;
static lv_obj_t *g_time_label = NULL;
static lv_obj_t *g_weekday_label = NULL;
static lv_obj_t *g_calendar_label = NULL;
static lv_obj_t *g_swipe_panel = NULL;
static lv_obj_t *g_swipe_title = NULL;
static lv_obj_t *g_swipe_subtitle = NULL;
static uint64_t g_last_clock_minute = (uint64_t)-1;
static uint8_t g_drag_active = 0U;
static uint8_t g_lockscreen_visible = 0U;
static int32_t g_drag_start_y = 0;
static int32_t g_panel_offset = 0;
static volatile uint32_t g_pending_action = 0xFFFFFFFFU;

static const char *g_weekday_names[7] = {
	"Sunday",
	"Monday",
	"Tuesday",
	"Wednesday",
	"Thursday",
	"Friday",
	"Saturday",
};

static const lockscreen_theme_s g_theme_light = {
	.text = 0x25324aU,
	.text_secondary = 0x4b6288U,
	.dim = 0x7b8aa3U,
	.chip_bg = 0xffffffU,
	.chip_fg = 0x25324aU,
	.chip_border = 0xe6edf7U,
	.panel = 0xfefcf8U,
	.panel_border = 0xd9e2f1U,
	.accent = 0xff7d5cU,
	.accent_soft = 0xffd8cfU,
	.hero_fill = 0xffffffU,
	.hero_ring = 0xdde7f7U,
};

static const lockscreen_theme_s g_theme_dark = {
	.text = 0xf3f7ffU,
	.text_secondary = 0xd8e2f4U,
	.dim = 0x93a3bfU,
	.chip_bg = 0x1b2434U,
	.chip_fg = 0xf3f7ffU,
	.chip_border = 0x33415bU,
	.panel = 0x182132U,
	.panel_border = 0x33425dU,
	.accent = 0xffa07cU,
	.accent_soft = 0x523026U,
	.hero_fill = 0x111927U,
	.hero_ring = 0x2f405dU,
};

static const lockscreen_theme_s *lock_ui_theme(void)
{
	return g_state.appearance.theme_mode == SYSTEMUI_THEME_MODE_DARK ?
	       &g_theme_dark : &g_theme_light;
}

static uint32_t lock_ui_mix_color(uint32_t from, uint32_t to,
				  uint32_t numerator, uint32_t denominator)
{
	uint32_t from_r = (from >> 16) & 0xFFU;
	uint32_t from_g = (from >> 8) & 0xFFU;
	uint32_t from_b = from & 0xFFU;
	uint32_t to_r = (to >> 16) & 0xFFU;
	uint32_t to_g = (to >> 8) & 0xFFU;
	uint32_t to_b = to & 0xFFU;
	uint32_t out_r = 0U;
	uint32_t out_g = 0U;
	uint32_t out_b = 0U;

	if (denominator == 0U) {
		return from;
	}

	out_r = (from_r * (denominator - numerator) + to_r * numerator) / denominator;
	out_g = (from_g * (denominator - numerator) + to_g * numerator) / denominator;
	out_b = (from_b * (denominator - numerator) + to_b * numerator) / denominator;
	return (out_r << 16) | (out_g << 8) | out_b;
}

static void clear_static_flags(lv_obj_t *obj)
{
	if (obj == NULL) {
		return;
	}

	lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE |
			       LV_OBJ_FLAG_CLICK_FOCUSABLE);
}

static void lock_ui_set_window_visible(uint8_t visible)
{
	window_s *window = lv_port_disp_get_window();

	if (window == NULL) {
		return;
	}

	(void)window_set_visible(window, visible);
}

static void lock_ui_present_now(uint32_t passes)
{
	if (passes == 0U) {
		return;
	}

	for (uint32_t i = 0; i < passes; i++) {
		lv_obj_invalidate(lv_scr_act());
		lv_refr_now(NULL);
		lv_port_disp_submit();
	}
}

static void lock_ui_set_panel_offset(int32_t offset)
{
	uint32_t distance = 0U;
	uint32_t opacity = 0U;

	if (g_swipe_panel == NULL) {
		return;
	}

	if (offset > 24) {
		offset = 24;
	}
	if (offset < -LOCKSCREEN_UNLOCK_TRAVEL) {
		offset = -LOCKSCREEN_UNLOCK_TRAVEL;
	}

	g_panel_offset = offset;
	lv_obj_set_y(g_swipe_panel, LOCKSCREEN_PANEL_Y + (lv_coord_t)offset);
	if (g_time_label != NULL) {
		lv_obj_set_style_translate_y(g_time_label, (lv_coord_t)(offset / 7), 0);
	}
	if (g_weekday_label != NULL) {
		lv_obj_set_style_translate_y(g_weekday_label, (lv_coord_t)(offset / 9), 0);
	}
	if (g_calendar_label != NULL) {
		lv_obj_set_style_translate_y(g_calendar_label, (lv_coord_t)(offset / 10), 0);
	}

	distance = (uint32_t)(-offset > 0 ? -offset : 0);
	if (distance > LOCKSCREEN_UNLOCK_TRAVEL) {
		distance = LOCKSCREEN_UNLOCK_TRAVEL;
	}
	opacity = (distance * 130U) / LOCKSCREEN_UNLOCK_TRAVEL;
	if (g_swipe_subtitle != NULL) {
		lv_obj_set_style_opa(g_swipe_subtitle, (lv_opa_t)(LV_OPA_COVER - opacity), 0);
	}
}

static void lock_ui_anim_panel_offset(void *obj, int32_t value)
{
	(void)obj;
	lock_ui_set_panel_offset(value);
}

static void lock_ui_snap_panel(int32_t target, uint32_t duration_ms)
{
	lv_anim_t anim = {0};

	if (g_swipe_panel == NULL) {
		return;
	}

	lv_anim_del(g_swipe_panel, lock_ui_anim_panel_offset);
	lv_anim_init(&anim);
	lv_anim_set_var(&anim, g_swipe_panel);
	lv_anim_set_exec_cb(&anim, lock_ui_anim_panel_offset);
	lv_anim_set_values(&anim, g_panel_offset, target);
	lv_anim_set_time(&anim, duration_ms);
	lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
	lv_anim_start(&anim);
}

static void lock_ui_reset_interaction(void)
{
	g_drag_active = 0U;
	g_drag_start_y = 0;
	lock_ui_set_panel_offset(0);
}

static uint8_t lock_ui_point_in_swipe_panel(const lv_point_t *point)
{
	int32_t panel_top = LOCKSCREEN_PANEL_Y + g_panel_offset;
	int32_t panel_bottom = panel_top + LOCKSCREEN_PANEL_HEIGHT;
	int32_t panel_right = LOCKSCREEN_PANEL_X + LOCKSCREEN_PANEL_WIDTH;

	if (point == NULL) {
		return 0U;
	}

	return point->x >= LOCKSCREEN_PANEL_X && point->x < panel_right &&
	       point->y >= panel_top && point->y < panel_bottom;
}

static void lock_ui_hide(void)
{
	g_lockscreen_visible = 0U;
	lock_ui_reset_interaction();
	lock_ui_set_window_visible(0U);
}

static void lock_ui_refresh_clock(uint64_t mono_ms)
{
	app_datetime_s local_time = {0};
	uint64_t ts = 0U;
	char time_buf[16] = {0};
	char weekday_buf[24] = {0};
	char calendar_buf[24] = {0};
	char year_text[5] = {0};
	char month_text[3] = {0};
	char day_text[3] = {0};

	if (g_time_label == NULL || g_weekday_label == NULL || g_calendar_label == NULL) {
		return;
	}

	ts = app_time_normalize_timestamp_seconds(OSSysCtrlGetTimestamp(), mono_ms);
	if ((ts / 60ULL) == g_last_clock_minute) {
		return;
	}
	g_last_clock_minute = ts / 60ULL;

	app_time_timestamp_to_local_datetime(ts, SYSTEMUI_TIMEZONE_OFFSET_HOURS, &local_time);
	app_time_format_fixed_u32(time_buf, (uint32_t)local_time.hour, 2U);
	time_buf[2] = ':';
	app_time_format_fixed_u32(time_buf + 3, (uint32_t)local_time.minute, 2U);
	time_buf[5] = '\0';
	app_time_format_fixed_u32(year_text, (uint32_t)local_time.year, 4U);
	app_time_format_fixed_u32(month_text, (uint32_t)local_time.month, 2U);
	app_time_format_fixed_u32(day_text, (uint32_t)local_time.day, 2U);
	snprintf(weekday_buf, sizeof(weekday_buf), "%s", g_weekday_names[local_time.weekday]);
	snprintf(calendar_buf, sizeof(calendar_buf), "%s-%s-%s", year_text, month_text, day_text);

	lv_label_set_text(g_time_label, time_buf);
	lv_obj_update_layout(g_time_label);
	lv_obj_set_style_transform_zoom(g_time_label, 512, 0);
	lv_obj_set_style_transform_pivot_x(g_time_label, lv_obj_get_width(g_time_label) / 2, 0);
	lv_obj_set_style_transform_pivot_y(g_time_label, lv_obj_get_height(g_time_label) / 2, 0);
	lv_label_set_text(g_weekday_label, weekday_buf);
	lv_label_set_text(g_calendar_label, calendar_buf);
}

static void lock_ui_poll_gesture(void)
{
	lv_point_t touch_point = lv_port_indev_get_touch_point();
	lv_indev_state_t touch_state = lv_port_indev_get_touch_state();
	int32_t delta = 0;

	if (!g_lockscreen_visible) {
		return;
	}

	if (touch_state == LV_INDEV_STATE_PRESSED) {
		if (!g_drag_active) {
			if (!lock_ui_point_in_swipe_panel(&touch_point)) {
				return;
			}
			g_drag_active = 1U;
			g_drag_start_y = touch_point.y;
		}

		delta = touch_point.y - g_drag_start_y;
		if (delta > 0) {
			delta /= 3;
		}
		lock_ui_set_panel_offset(delta);
		return;
	}

	if (!g_drag_active) {
		return;
	}

	g_drag_active = 0U;
	if (g_panel_offset <= -LOCKSCREEN_UNLOCK_THRESHOLD) {
		lock_ui_hide();
		return;
	}

	lock_ui_snap_panel(0, 220U);
}

static void lock_ui_create_blob(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
				lv_coord_t w, lv_coord_t h, uint32_t color, lv_opa_t opa)
{
	lv_obj_t *blob = lv_obj_create(parent);

	lv_obj_set_pos(blob, x, y);
	lv_obj_set_size(blob, w, h);
	lv_obj_set_style_radius(blob, 999, 0);
	lv_obj_set_style_bg_color(blob, lv_color_hex(color), 0);
	lv_obj_set_style_bg_opa(blob, opa, 0);
	lv_obj_set_style_border_width(blob, 0, 0);
	lv_obj_set_style_shadow_width(blob, 0, 0);
	lv_obj_set_style_pad_all(blob, 0, 0);
	clear_static_flags(blob);
}

static lv_obj_t *lock_ui_create_disc(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
				     lv_coord_t w, lv_coord_t h, uint32_t fill,
				     lv_opa_t fill_opa, uint32_t border,
				     lv_opa_t border_opa, lv_coord_t border_width)
{
	lv_obj_t *disc = lv_obj_create(parent);

	lv_obj_set_pos(disc, x, y);
	lv_obj_set_size(disc, w, h);
	lv_obj_set_style_radius(disc, 999, 0);
	lv_obj_set_style_bg_color(disc, lv_color_hex(fill), 0);
	lv_obj_set_style_bg_opa(disc, fill_opa, 0);
	lv_obj_set_style_border_color(disc, lv_color_hex(border), 0);
	lv_obj_set_style_border_opa(disc, border_opa, 0);
	lv_obj_set_style_border_width(disc, border_width, 0);
	lv_obj_set_style_shadow_width(disc, 0, 0);
	lv_obj_set_style_pad_all(disc, 0, 0);
	clear_static_flags(disc);
	return disc;
}

static void lock_ui_create_ui(void)
{
	const lockscreen_theme_s *theme = lock_ui_theme();
	lv_obj_t *layer = NULL;
	lv_obj_t *chip = NULL;
	lv_obj_t *hero_orb = NULL;
	lv_obj_t *hero_ring = NULL;
	uint32_t desktop_color = g_state.appearance.desktop_color;
	uint32_t bg_top = 0U;
	uint32_t bg_bottom = 0U;
	uint32_t panel_color = 0U;
	uint32_t hero_fill = 0U;
	uint32_t hero_ring_color = 0U;

	g_root = lv_scr_act();
	clear_static_flags(g_root);
	lv_obj_set_scrollbar_mode(g_root, LV_SCROLLBAR_MODE_OFF);
	bg_top = g_state.appearance.theme_mode == SYSTEMUI_THEME_MODE_DARK ?
		 lock_ui_mix_color(desktop_color, g_state.palette.bg, 5U, 6U) :
		 lock_ui_mix_color(desktop_color, g_state.palette.panel, 1U, 3U);
	bg_bottom = g_state.appearance.theme_mode == SYSTEMUI_THEME_MODE_DARK ?
		    lock_ui_mix_color(desktop_color, g_state.palette.panel_alt, 7U, 8U) :
		    lock_ui_mix_color(desktop_color, 0xf3f7ffU, 1U, 2U);
	panel_color = lock_ui_mix_color(desktop_color, theme->panel,
					g_state.appearance.theme_mode == SYSTEMUI_THEME_MODE_DARK ?
					4U : 5U, 6U);
	hero_fill = lock_ui_mix_color(desktop_color, theme->hero_fill,
				      g_state.appearance.theme_mode == SYSTEMUI_THEME_MODE_DARK ?
				      5U : 4U, 6U);
	hero_ring_color = lock_ui_mix_color(theme->hero_ring, desktop_color, 1U, 5U);

	lv_obj_set_style_bg_color(g_root, lv_color_hex(bg_top), 0);
	lv_obj_set_style_bg_grad_color(g_root, lv_color_hex(bg_bottom), 0);
	lv_obj_set_style_bg_grad_dir(g_root, LV_GRAD_DIR_VER, 0);
	lv_obj_set_style_bg_opa(g_root, LV_OPA_COVER, 0);
	lv_obj_set_style_border_width(g_root, 0, 0);
	lv_obj_set_style_pad_all(g_root, 0, 0);

	lock_ui_create_blob(g_root, -120, 96, 420, 420,
			    lock_ui_mix_color(theme->accent, 0xffffffU, 1U, 3U),
			    g_state.appearance.theme_mode == SYSTEMUI_THEME_MODE_DARK ?
			    LV_OPA_20 : LV_OPA_30);
	lock_ui_create_blob(g_root, 712, 118, 260, 260,
			    lock_ui_mix_color(desktop_color, theme->panel_border, 1U, 2U),
			    g_state.appearance.theme_mode == SYSTEMUI_THEME_MODE_DARK ?
			    LV_OPA_20 : 25);
	lock_ui_create_blob(g_root, 694, 702, 360, 360,
			    lock_ui_mix_color(theme->accent, desktop_color, 1U, 2U),
			    g_state.appearance.theme_mode == SYSTEMUI_THEME_MODE_DARK ?
			    18 : 24);

	layer = lv_obj_create(g_root);
	lv_obj_set_size(layer, LOCKSCREEN_WIDTH, LOCKSCREEN_HEIGHT);
	lv_obj_set_pos(layer, 0, 0);
	lv_obj_set_style_bg_opa(layer, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(layer, 0, 0);
	lv_obj_set_style_shadow_width(layer, 0, 0);
	lv_obj_set_style_pad_all(layer, 0, 0);
	clear_static_flags(layer);
	lv_obj_set_scrollbar_mode(layer, LV_SCROLLBAR_MODE_OFF);

	hero_ring = lock_ui_create_disc(layer, 316, 116, 392, 392,
					hero_fill, g_state.appearance.theme_mode == SYSTEMUI_THEME_MODE_DARK ?
					20 : 28,
					hero_ring_color, g_state.appearance.theme_mode == SYSTEMUI_THEME_MODE_DARK ?
					32 : 42, 1);
	hero_orb = lock_ui_create_disc(layer, 356, 156, 312, 312,
				       hero_fill, g_state.appearance.theme_mode == SYSTEMUI_THEME_MODE_DARK ?
				       48 : 72,
				       theme->hero_ring, g_state.appearance.theme_mode == SYSTEMUI_THEME_MODE_DARK ?
				       48 : 58, 1);
	lv_obj_move_background(hero_ring);
	lv_obj_move_background(hero_orb);

	chip = lv_label_create(layer);
	lv_label_set_text(chip, "LOCKED");
	lv_obj_align(chip, LV_ALIGN_TOP_MID, 0, 66);
	lv_obj_set_style_bg_color(chip, lv_color_hex(theme->chip_bg), 0);
	lv_obj_set_style_bg_opa(chip,
				g_state.appearance.theme_mode == SYSTEMUI_THEME_MODE_DARK ?
				72 : LV_OPA_90, 0);
	lv_obj_set_style_border_color(chip, lv_color_hex(theme->chip_border), 0);
	lv_obj_set_style_border_width(chip, 1, 0);
	lv_obj_set_style_radius(chip, 999, 0);
	lv_obj_set_style_pad_left(chip, 18, 0);
	lv_obj_set_style_pad_right(chip, 18, 0);
	lv_obj_set_style_pad_top(chip, 8, 0);
	lv_obj_set_style_pad_bottom(chip, 8, 0);
	lv_obj_set_style_text_color(chip, lv_color_hex(theme->chip_fg), 0);
	lv_obj_set_style_text_font(chip, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_letter_space(chip, 3, 0);
	clear_static_flags(chip);

	g_time_label = lv_label_create(layer);
	lv_label_set_text(g_time_label, "--:--");
	lv_obj_set_style_text_font(g_time_label, &lv_font_montserrat_48, 0);
	lv_obj_set_style_text_color(g_time_label, lv_color_hex(theme->text), 0);
	lv_obj_align(g_time_label, LV_ALIGN_TOP_MID, 0, 212);
	clear_static_flags(g_time_label);
	lv_obj_set_style_transform_zoom(g_time_label, 512, 0);

	g_weekday_label = lv_label_create(layer);
	lv_label_set_text(g_weekday_label, "Monday");
	lv_obj_set_style_text_font(g_weekday_label, &lv_font_montserrat_32, 0);
	lv_obj_set_style_text_color(g_weekday_label, lv_color_hex(theme->text_secondary), 0);
	lv_obj_set_style_text_letter_space(g_weekday_label, 1, 0);
	lv_obj_align(g_weekday_label, LV_ALIGN_TOP_MID, 0, 386);
	clear_static_flags(g_weekday_label);

	g_calendar_label = lv_label_create(layer);
	lv_label_set_text(g_calendar_label, "2026-01-01");
	lv_obj_set_style_text_font(g_calendar_label, &lv_font_montserrat_24, 0);
	lv_obj_set_style_text_color(g_calendar_label, lv_color_hex(theme->dim), 0);
	lv_obj_set_style_text_letter_space(g_calendar_label, 1, 0);
	lv_obj_align(g_calendar_label, LV_ALIGN_TOP_MID, 0, 430);
	clear_static_flags(g_calendar_label);

	g_swipe_panel = lv_obj_create(layer);
	lv_obj_set_size(g_swipe_panel, LOCKSCREEN_PANEL_WIDTH, LOCKSCREEN_PANEL_HEIGHT);
	lv_obj_set_pos(g_swipe_panel, LOCKSCREEN_PANEL_X, LOCKSCREEN_PANEL_Y);
	lv_obj_set_style_radius(g_swipe_panel, 30, 0);
	lv_obj_set_style_bg_color(g_swipe_panel, lv_color_hex(panel_color), 0);
	lv_obj_set_style_bg_opa(g_swipe_panel,
				g_state.appearance.theme_mode == SYSTEMUI_THEME_MODE_DARK ?
				78 : 92, 0);
	lv_obj_set_style_border_color(g_swipe_panel, lv_color_hex(theme->panel_border), 0);
	lv_obj_set_style_border_width(g_swipe_panel, 1, 0);
	lv_obj_set_style_shadow_width(g_swipe_panel, 0, 0);
	lv_obj_set_style_pad_all(g_swipe_panel, 0, 0);
	clear_static_flags(g_swipe_panel);
	lv_obj_set_scrollbar_mode(g_swipe_panel, LV_SCROLLBAR_MODE_OFF);

	g_swipe_title = lv_label_create(g_swipe_panel);
	lv_label_set_text(g_swipe_title, LV_SYMBOL_UP);
	lv_obj_align(g_swipe_title, LV_ALIGN_TOP_MID, 0, 22);
	lv_obj_set_style_text_font(g_swipe_title, &lv_font_montserrat_48, 0);
	lv_obj_set_style_text_color(g_swipe_title, lv_color_hex(theme->accent), 0);
	clear_static_flags(g_swipe_title);

	g_swipe_subtitle = lv_label_create(g_swipe_panel);
	lv_label_set_text(g_swipe_subtitle, "Swipe upward to unlock");
	lv_obj_set_width(g_swipe_subtitle, LOCKSCREEN_PANEL_WIDTH - 88);
	lv_label_set_long_mode(g_swipe_subtitle, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(g_swipe_subtitle, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_align(g_swipe_subtitle, LV_ALIGN_TOP_MID, 0, 74);
	lv_obj_set_style_text_font(g_swipe_subtitle, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(g_swipe_subtitle, lv_color_hex(theme->dim), 0);
	lv_obj_set_style_text_letter_space(g_swipe_subtitle, 1, 0);
	clear_static_flags(g_swipe_subtitle);

	g_last_clock_minute = (uint64_t)-1;
	lock_ui_reset_interaction();
}

static void lock_ui_rebuild_ui(uint64_t mono_ms)
{
	lv_obj_clean(lv_scr_act());
	g_root = NULL;
	g_time_label = NULL;
	g_weekday_label = NULL;
	g_calendar_label = NULL;
	g_swipe_panel = NULL;
	g_swipe_title = NULL;
	g_swipe_subtitle = NULL;
	lock_ui_create_ui();
	lock_ui_refresh_clock(mono_ms);
}

static void lock_ui_apply_action(uint32_t action, uint64_t mono_ms)
{
	uint8_t open = action == SYSTEMUI_OVERLAY_ACTION_OPEN ? 1U : 0U;
	uint64_t lock_pid = 0;

	if (action == 0xFFFFFFFFU) {
		return;
	}
	if (action == SYSTEMUI_OVERLAY_ACTION_TOGGLE) {
		open = g_lockscreen_visible ? 0U : 1U;
	}

	if (open) {
		g_lockscreen_visible = 1U;
		lock_ui_reset_interaction();
		lock_ui_set_window_visible(1U);
		lock_pid = systemui_lookup_process_pid(SYSTEMUI_LOCKSCREEN_PROCESS_PATH);
		if (lock_pid != 0U) {
			(void)window_activate_owner(lock_pid);
		}
		if (systemui_sync_appearance(&g_state, mono_ms, 0U) || g_state.appearance_dirty) {
			g_state.appearance_dirty = 0U;
			lock_ui_rebuild_ui(mono_ms);
		} else {
			lock_ui_refresh_clock(mono_ms);
		}
		lock_ui_present_now(2U);
		return;
	}

	lock_ui_hide();
}

static void lock_ui_process_pending_action(uint64_t mono_ms)
{
	uint32_t action = g_pending_action;

	if (action == 0xFFFFFFFFU) {
		return;
	}

	g_pending_action = 0xFFFFFFFFU;
	lock_ui_apply_action(action, mono_ms);
}

IPC_ENDPOINT void lockscreen_service_entry(uint64_t cref, uint64_t method,
					   uint64_t arg1, uint64_t arg2, uint64_t arg3)
{
	(void)cref;
	(void)arg2;
	(void)arg3;

	if (method == IPC_SYSTEMUI_LOCKSCREEN_SERVICE_FUNCTION_REQUEST) {
		uint32_t action = (uint32_t)arg1;
		uint64_t lock_pid = 0;

		g_pending_action = action;
		if (action == SYSTEMUI_OVERLAY_ACTION_OPEN ||
		    (action == SYSTEMUI_OVERLAY_ACTION_TOGGLE && !g_lockscreen_visible)) {
			lock_ui_set_window_visible(1U);
			lock_pid = systemui_lookup_process_pid(SYSTEMUI_LOCKSCREEN_PROCESS_PATH);
			if (lock_pid != 0U) {
				(void)window_activate_owner(lock_pid);
			}
		}
		OSIpcEndPointPoolReply(1);
	} else {
		OSIpcEndPointPoolReply(0);
	}

	while (1) {}
}

static void lock_ui_publish_service(void)
{
	if (!sys_register_service_pool(IPC_SYSTEMUI_LOCKSCREEN_SERVICE_ID, &lockscreen_service_entry)) {
		log_warn("lockui: register service failed\n");
		return;
	}

	log_info("lockui: service published\n");
}

static void lock_ui_on_create(app_s *app)
{
	(void)app;
	systemui_runtime_init(&g_state);
	(void)systemui_sync_appearance(&g_state, 0U, 1U);
	lock_ui_rebuild_ui(0U);
	lock_ui_publish_service();
	lock_ui_present_now(2U);
	lock_ui_set_window_visible(0U);
	g_lockscreen_visible = 0U;
}

static void lock_ui_on_foreground(app_s *app)
{
	uint64_t mono_ms = OSSysCtrlGetMonoTime() / NSEC_PER_MSEC;

	(void)app;
	lock_ui_process_pending_action(mono_ms);
	if (!g_lockscreen_visible) {
		return;
	}
	if (systemui_sync_appearance(&g_state, mono_ms, 0U) || g_state.appearance_dirty) {
		g_state.appearance_dirty = 0U;
		lock_ui_rebuild_ui(mono_ms);
	}
	lock_ui_poll_gesture();
	if (!g_lockscreen_visible) {
		return;
	}
	lock_ui_refresh_clock(mono_ms);
}

static void lock_ui_on_update(app_s *app, uint64_t mono_ms)
{
	(void)app;
	lock_ui_process_pending_action(mono_ms);
	if (!g_lockscreen_visible) {
		return;
	}
	if (systemui_sync_appearance(&g_state, mono_ms, 0U) || g_state.appearance_dirty) {
		g_state.appearance_dirty = 0U;
		lock_ui_rebuild_ui(mono_ms);
	}
	lock_ui_poll_gesture();
	if (!g_lockscreen_visible) {
		return;
	}
	lock_ui_refresh_clock(mono_ms);
}

static void lock_ui_on_background(app_s *app)
{
	(void)app;
	lock_ui_reset_interaction();
}

int main(int argc, char *argv[])
{
	app_s app = {0};
	app_config_s config = {
		.name = "lockui",
		.x = 0,
		.y = 0,
		.z = SYSTEMUI_LOCK_WINDOW_Z,
		.width = LOCKSCREEN_WIDTH,
		.height = LOCKSCREEN_HEIGHT,
		.window_flags = WINDOW_FLAG_FOCUSABLE,
		.enable_input = 1U,
	};
	app_lifecycle_ops_s lifecycle = {
		.on_create = lock_ui_on_create,
		.on_foreground = lock_ui_on_foreground,
		.on_background = lock_ui_on_background,
		.on_update = lock_ui_on_update,
	};

	(void)argc;
	(void)argv;
	log_info("lockui start\n");
	app_init(&app, &config, &lifecycle, NULL);
	return app_run(&app);
}
