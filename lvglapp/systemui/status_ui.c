#include "lvgl.h"
#include "app.h"
#include "liblvgl/lv_port_disp.h"
#include "libwindow/window.h"
#include "log.h"
#include "systemui_shared.h"

typedef struct systemui_datetime {
	int year;
	int month;
	int day;
	int weekday;
	int hour;
	int minute;
	int second;
} systemui_datetime_s;

static systemui_runtime_state_s g_state = {0};
static lv_obj_t *g_root = NULL;
static lv_obj_t *g_bar = NULL;
static lv_obj_t *g_line = NULL;
static lv_obj_t *g_brand_chip = NULL;
static lv_obj_t *g_brand_dot = NULL;
static lv_obj_t *g_clock_chip = NULL;
static lv_obj_t *g_brand = NULL;
static lv_obj_t *g_clock = NULL;
static lv_obj_t *g_left_hit = NULL;
static lv_obj_t *g_right_hit = NULL;

static void clear_static_flags(lv_obj_t *obj)
{
	if (obj == NULL) {
		return;
	}

	lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE |
			       LV_OBJ_FLAG_CLICK_FOCUSABLE);
}

static void civil_from_days(int64_t days, int *year, int *month, int *day)
{
	int64_t z = days + 719468;
	int64_t era = (z >= 0 ? z : z - 146096) / 146097;
	uint32_t doe = (uint32_t)(z - era * 146097);
	uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	int y = (int)yoe + (int)era * 400;
	uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	uint32_t mp = (5 * doy + 2) / 153;
	uint32_t d = doy - (153 * mp + 2) / 5 + 1;
	uint32_t m = mp + (mp < 10 ? 3U : (uint32_t)-9);

	y += (m <= 2U);
	*year = y;
	*month = (int)m;
	*day = (int)d;
}

static void timestamp_to_local_datetime(uint64_t ts_sec, systemui_datetime_s *out)
{
	int64_t local_sec = (int64_t)ts_sec + (SYSTEMUI_TIMEZONE_OFFSET_HOURS * 3600);
	int64_t days = local_sec / 86400;
	int64_t sec_of_day = local_sec % 86400;

	if (out == NULL) {
		return;
	}
	if (sec_of_day < 0) {
		sec_of_day += 86400;
		days--;
	}

	out->hour = (int)(sec_of_day / 3600);
	out->minute = (int)((sec_of_day / 60) % 60);
	out->second = (int)(sec_of_day % 60);
	out->weekday = (int)((days + 4) % 7);
	if (out->weekday < 0) {
		out->weekday += 7;
	}
	civil_from_days(days, &out->year, &out->month, &out->day);
}

static uint64_t normalize_timestamp_seconds(uint64_t raw_ts, uint64_t mono_ms)
{
	static uint64_t sample_raw_ts = 0;
	static uint64_t sample_mono_ms = 0;
	static uint8_t ts_in_msec = 0;
	static uint8_t unit_locked = 0;

	if (sample_mono_ms == 0U) {
		sample_raw_ts = raw_ts;
		sample_mono_ms = mono_ms;
	}

	if (mono_ms > sample_mono_ms && raw_ts >= sample_raw_ts) {
		uint64_t mono_delta_ms = mono_ms - sample_mono_ms;
		if (mono_delta_ms >= 200U) {
			uint64_t raw_delta = raw_ts - sample_raw_ts;
			if (raw_delta >= (mono_delta_ms / 2U)) {
				ts_in_msec = 1U;
				unit_locked = 1U;
			} else if (raw_delta <= ((mono_delta_ms / 200U) + 2U)) {
				ts_in_msec = 0U;
				unit_locked = 1U;
			}
			sample_raw_ts = raw_ts;
			sample_mono_ms = mono_ms;
		}
	}

	if (!unit_locked && raw_ts > 4102444800ULL) {
		ts_in_msec = 1U;
	}

	return ts_in_msec ? (raw_ts / 1000ULL) : raw_ts;
}

static void apply_appearance(void)
{
	if (g_root != NULL) {
		lv_obj_set_style_bg_color(g_root, lv_color_hex(g_state.palette.panel), 0);
		lv_obj_set_style_bg_opa(g_root, LV_OPA_COVER, 0);
	}
	if (g_bar != NULL) {
		lv_obj_set_style_bg_color(g_bar, lv_color_hex(g_state.palette.bg), 0);
	}
	if (g_line != NULL) {
		lv_obj_set_style_bg_color(g_line, lv_color_hex(g_state.palette.line), 0);
		lv_obj_set_style_bg_opa(g_line, LV_OPA_40, 0);
	}
	if (g_brand_chip != NULL) {
		lv_obj_set_style_bg_color(g_brand_chip, lv_color_hex(g_state.palette.panel), 0);
		lv_obj_set_style_border_color(g_brand_chip, lv_color_hex(g_state.palette.line), 0);
		lv_obj_set_style_shadow_color(g_brand_chip, lv_color_hex(g_state.palette.accent), 0);
		lv_obj_set_style_shadow_opa(g_brand_chip, 22, 0);
	}
	if (g_brand_dot != NULL) {
		lv_obj_set_style_bg_color(g_brand_dot, lv_color_hex(g_state.palette.accent), 0);
	}
	if (g_clock_chip != NULL) {
		lv_obj_set_style_bg_color(g_clock_chip, lv_color_hex(g_state.palette.accent), 0);
		lv_obj_set_style_bg_opa(g_clock_chip, 34, 0);
		lv_obj_set_style_border_color(g_clock_chip, lv_color_hex(g_state.palette.accent), 0);
	}
	if (g_brand != NULL) {
		lv_obj_set_style_text_color(g_brand, lv_color_hex(g_state.palette.text), 0);
	}
	if (g_clock != NULL) {
		lv_obj_set_style_text_color(g_clock, lv_color_hex(g_state.palette.accent), 0);
	}
}

static void update_clock(uint64_t mono_ms)
{
	static uint64_t last_second = (uint64_t)-1;
	systemui_datetime_s local_time = {0};
	uint64_t ts = normalize_timestamp_seconds(OSSysCtrlGetTimestamp(), mono_ms);
	char text[8];

	if (g_clock == NULL) {
		return;
	}

	timestamp_to_local_datetime(ts, &local_time);
	if ((uint64_t)local_time.hour * 3600ULL + (uint64_t)local_time.minute * 60ULL +
	    (uint64_t)local_time.second == last_second) {
		return;
	}

	text[0] = (char)('0' + (local_time.hour / 10));
	text[1] = (char)('0' + (local_time.hour % 10));
	text[2] = ':';
	text[3] = (char)('0' + (local_time.minute / 10));
	text[4] = (char)('0' + (local_time.minute % 10));
	text[5] = '\0';
	lv_label_set_text(g_clock, text);
	last_second = (uint64_t)local_time.hour * 3600ULL +
		      (uint64_t)local_time.minute * 60ULL +
		      (uint64_t)local_time.second;
}

static void status_left_event_cb(lv_event_t *e)
{
	(void)e;
	(void)systemui_request_overlay_action(SYSTEMUI_OVERLAY_PANEL_NOTIFICATION,
						    SYSTEMUI_OVERLAY_ACTION_TOGGLE);
}

static void status_right_event_cb(lv_event_t *e)
{
	(void)e;
	(void)systemui_request_overlay_action(SYSTEMUI_OVERLAY_PANEL_CONTROL,
						    SYSTEMUI_OVERLAY_ACTION_TOGGLE);
}

static void create_ui(void)
{
	lv_obj_t *scr = lv_scr_act();
	window_s *window = NULL;
	window_rect_s input_region = {
		.x = 0,
		.y = 0,
		.width = SYSTEMUI_WIDTH,
		.height = SYSTEMUI_STATUS_BAR_HEIGHT,
	};

	g_root = scr;
	lv_obj_set_style_bg_color(scr, lv_color_hex(g_state.palette.panel), 0);
	lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
	lv_obj_set_style_border_width(scr, 0, 0);
	lv_obj_set_style_pad_all(scr, 0, 0);
	clear_static_flags(scr);

	g_bar = lv_obj_create(scr);
	lv_obj_set_size(g_bar, SYSTEMUI_WIDTH, SYSTEMUI_STATUS_BAR_HEIGHT);
	lv_obj_set_pos(g_bar, 0, 0);
	lv_obj_set_style_bg_color(g_bar, lv_color_hex(g_state.palette.bg), 0);
	lv_obj_set_style_bg_opa(g_bar, LV_OPA_COVER, 0);
	lv_obj_set_style_border_width(g_bar, 0, 0);
	lv_obj_set_style_radius(g_bar, 0, 0);
	lv_obj_set_style_pad_all(g_bar, 0, 0);
	lv_obj_set_style_shadow_width(g_bar, 0, 0);
	clear_static_flags(g_bar);

	g_brand_chip = lv_obj_create(g_bar);
	lv_obj_set_size(g_brand_chip, 228, 44);
	lv_obj_set_pos(g_brand_chip, 20, 14);
	lv_obj_set_style_bg_color(g_brand_chip, lv_color_hex(g_state.palette.panel), 0);
	lv_obj_set_style_bg_opa(g_brand_chip, LV_OPA_COVER, 0);
	lv_obj_set_style_border_color(g_brand_chip, lv_color_hex(g_state.palette.line), 0);
	lv_obj_set_style_border_width(g_brand_chip, 1, 0);
	lv_obj_set_style_radius(g_brand_chip, 22, 0);
	lv_obj_set_style_pad_all(g_brand_chip, 0, 0);
	lv_obj_set_style_shadow_width(g_brand_chip, 16, 0);
	clear_static_flags(g_brand_chip);

	g_brand_dot = lv_obj_create(g_brand_chip);
	lv_obj_set_size(g_brand_dot, 12, 12);
	lv_obj_align(g_brand_dot, LV_ALIGN_LEFT_MID, 16, 0);
	lv_obj_set_style_radius(g_brand_dot, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_bg_color(g_brand_dot, lv_color_hex(g_state.palette.accent), 0);
	lv_obj_set_style_bg_opa(g_brand_dot, LV_OPA_COVER, 0);
	lv_obj_set_style_border_width(g_brand_dot, 0, 0);
	lv_obj_set_style_shadow_width(g_brand_dot, 0, 0);
	clear_static_flags(g_brand_dot);

	g_brand = lv_label_create(g_brand_chip);
	lv_label_set_text(g_brand, "TranquilOS");
	lv_obj_set_style_text_font(g_brand, &lv_font_montserrat_24, 0);
	lv_obj_set_style_text_color(g_brand, lv_color_hex(g_state.palette.text), 0);
	lv_obj_align(g_brand, LV_ALIGN_LEFT_MID, 38, 0);

	g_clock_chip = lv_obj_create(g_bar);
	lv_obj_set_size(g_clock_chip, 150, 44);
	lv_obj_align(g_clock_chip, LV_ALIGN_RIGHT_MID, -20, 0);
	lv_obj_set_style_bg_color(g_clock_chip, lv_color_hex(g_state.palette.accent), 0);
	lv_obj_set_style_bg_opa(g_clock_chip, 34, 0);
	lv_obj_set_style_border_color(g_clock_chip, lv_color_hex(g_state.palette.accent), 0);
	lv_obj_set_style_border_width(g_clock_chip, 1, 0);
	lv_obj_set_style_radius(g_clock_chip, 22, 0);
	lv_obj_set_style_pad_all(g_clock_chip, 0, 0);
	lv_obj_set_style_shadow_width(g_clock_chip, 0, 0);
	clear_static_flags(g_clock_chip);

	g_clock = lv_label_create(g_clock_chip);
	lv_label_set_text(g_clock, "00:00");
	lv_obj_set_style_text_font(g_clock, &lv_font_montserrat_24, 0);
	lv_obj_set_style_text_color(g_clock, lv_color_hex(g_state.palette.accent), 0);
	lv_obj_center(g_clock);

	g_line = lv_obj_create(g_bar);
	lv_obj_set_size(g_line, SYSTEMUI_WIDTH - 48, 1);
	lv_obj_align(g_line, LV_ALIGN_BOTTOM_MID, 0, -2);
	lv_obj_set_style_bg_color(g_line, lv_color_hex(g_state.palette.line), 0);
	lv_obj_set_style_bg_opa(g_line, LV_OPA_40, 0);
	lv_obj_set_style_border_width(g_line, 0, 0);
	lv_obj_set_style_radius(g_line, LV_RADIUS_CIRCLE, 0);
	clear_static_flags(g_line);

	g_left_hit = lv_obj_create(g_bar);
	lv_obj_set_size(g_left_hit, SYSTEMUI_WIDTH / 2, SYSTEMUI_STATUS_BAR_HEIGHT);
	lv_obj_set_pos(g_left_hit, 0, 0);
	lv_obj_set_style_bg_opa(g_left_hit, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(g_left_hit, 0, 0);
	lv_obj_set_style_radius(g_left_hit, 0, 0);
	lv_obj_set_style_pad_all(g_left_hit, 0, 0);
	lv_obj_set_style_shadow_width(g_left_hit, 0, 0);
	lv_obj_add_flag(g_left_hit, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_clear_flag(g_left_hit, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICK_FOCUSABLE);
	lv_obj_add_event_cb(g_left_hit, status_left_event_cb, LV_EVENT_CLICKED, NULL);

	g_right_hit = lv_obj_create(g_bar);
	lv_obj_set_size(g_right_hit, SYSTEMUI_WIDTH / 2, SYSTEMUI_STATUS_BAR_HEIGHT);
	lv_obj_set_pos(g_right_hit, SYSTEMUI_WIDTH / 2, 0);
	lv_obj_set_style_bg_opa(g_right_hit, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(g_right_hit, 0, 0);
	lv_obj_set_style_radius(g_right_hit, 0, 0);
	lv_obj_set_style_pad_all(g_right_hit, 0, 0);
	lv_obj_set_style_shadow_width(g_right_hit, 0, 0);
	lv_obj_add_flag(g_right_hit, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_clear_flag(g_right_hit, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICK_FOCUSABLE);
	lv_obj_add_event_cb(g_right_hit, status_right_event_cb, LV_EVENT_CLICKED, NULL);

	window = lv_port_disp_get_window();
	if (window != NULL && !window_set_input_region(window, &input_region)) {
		log_warn("statusui: set input region failed\n");
	}
}

static void status_ui_on_create(app_s *app)
{
	(void)app;
	systemui_runtime_init(&g_state);
	(void)systemui_sync_appearance(&g_state, 0U, 1U);
	create_ui();
	apply_appearance();
	lv_obj_invalidate(lv_scr_act());
	lv_refr_now(NULL);
	lv_port_disp_submit();
}

static void status_ui_on_update(app_s *app, uint64_t mono_ms)
{
	(void)app;
	if (systemui_sync_appearance(&g_state, mono_ms, 0U) || g_state.appearance_dirty) {
		g_state.appearance_dirty = 0U;
		apply_appearance();
	}
	update_clock(mono_ms);
}

int main(int argc, char *argv[])
{
	app_s app = {0};
	app_config_s config = {
		.name = "statusui",
		.x = 0,
		.y = 0,
		.z = SYSTEMUI_STATUS_WINDOW_Z,
		.width = SYSTEMUI_WIDTH,
		.height = SYSTEMUI_STATUS_BAR_HEIGHT,
		.window_flags = 0U,
		.enable_input = 1U,
	};
	app_lifecycle_ops_s lifecycle = {
		.on_create = status_ui_on_create,
		.on_update = status_ui_on_update,
	};

	(void)argc;
	(void)argv;
	log_info("statusui start\n");
	app_init(&app, &config, &lifecycle, NULL);
	return app_run(&app);
}
