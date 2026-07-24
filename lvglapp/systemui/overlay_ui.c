#include "lvgl.h"
#include "app.h"
#include "liblvgl/lv_port_disp.h"
#include "libsystem/appmgr_client.h"
#include "libwindow/window.h"
#include "log.h"
#include "systemui_shared.h"

#define RECENT_PANEL_WIDTH 744
#define RECENT_PANEL_HEIGHT 584
#define RECENT_PANEL_Y ((APP_DEFAULT_HEIGHT - RECENT_PANEL_HEIGHT) / 2)
#define RECENT_PANEL_LIST_X 28
#define RECENT_PANEL_LIST_Y 108
#define RECENT_PANEL_LIST_WIDTH (RECENT_PANEL_WIDTH - (RECENT_PANEL_LIST_X * 2))
#define RECENT_PANEL_LIST_HEIGHT (RECENT_PANEL_HEIGHT - RECENT_PANEL_LIST_Y - 28)
#define RECENT_ITEM_HEIGHT 84
#define RECENT_ITEM_GAP 14

#define TOP_SHEET_MARGIN 18
#define TOP_SHEET_Y 86
#define NOTIFICATION_PANEL_WIDTH (APP_DEFAULT_WIDTH - (TOP_SHEET_MARGIN * 2))
#define NOTIFICATION_PANEL_HEIGHT 442
#define CONTROL_PANEL_WIDTH (APP_DEFAULT_WIDTH - (TOP_SHEET_MARGIN * 2))
#define CONTROL_PANEL_HEIGHT 344
#define TOP_PANEL_CARD_GAP 14
#define TOP_PANEL_CARD_HEIGHT 92
#define CONTROL_PANEL_INSET 24
#define CONTROL_CARD_GAP 16
#define CONTROL_CARD_TOP 132
#define CONTROL_CARD_HALF_WIDTH ((CONTROL_PANEL_WIDTH - (CONTROL_PANEL_INSET * 2) - CONTROL_CARD_GAP) / 2)
#define CONTROL_CARD_SMALL_HEIGHT 184
#define CONTROL_SEGMENT_WIDTH 204
#define RECENT_REFRESH_INTERVAL_MS 1000U

typedef struct systemui_recent_app {
	char title[32];
	char subtitle[64];
	char process_path[APPMGR_PROCESS_PATH_MAX];
	uint64_t pid;
} systemui_recent_app_s;

typedef struct systemui_datetime {
	int year;
	int month;
	int day;
	int weekday;
	int hour;
	int minute;
	int second;
} systemui_datetime_s;

typedef struct systemui_info_card {
	lv_obj_t *card;
	lv_obj_t *title;
	lv_obj_t *body;
	lv_obj_t *chip;
} systemui_info_card_s;

static systemui_runtime_state_s g_state = {0};
static appmgr_client_s *g_appmgr = NULL;
static lv_obj_t *g_root_scr = NULL;
static lv_obj_t *g_recent_overlay = NULL;
static lv_obj_t *g_recent_panel = NULL;
static lv_obj_t *g_recent_list = NULL;
static lv_obj_t *g_recent_title_lbl = NULL;
static lv_obj_t *g_recent_subtitle_lbl = NULL;
static lv_obj_t *g_notification_overlay = NULL;
static lv_obj_t *g_notification_panel = NULL;
static lv_obj_t *g_notification_title_lbl = NULL;
static lv_obj_t *g_notification_subtitle_lbl = NULL;
static lv_obj_t *g_control_overlay = NULL;
static lv_obj_t *g_control_panel = NULL;
static lv_obj_t *g_control_title_lbl = NULL;
static lv_obj_t *g_control_subtitle_lbl = NULL;
static lv_obj_t *g_control_theme_card = NULL;
static lv_obj_t *g_control_network_card = NULL;
static lv_obj_t *g_control_theme_section_lbl = NULL;
static lv_obj_t *g_control_network_section_lbl = NULL;
static lv_obj_t *g_control_theme_hint_lbl = NULL;
static lv_obj_t *g_control_network_hint_lbl = NULL;
static lv_obj_t *g_control_theme_chip = NULL;
static lv_obj_t *g_control_network_chip = NULL;
static lv_obj_t *g_control_theme_light_btn = NULL;
static lv_obj_t *g_control_theme_dark_btn = NULL;
static lv_obj_t *g_control_network_btn = NULL;
static lv_obj_t *g_control_network_btn_label = NULL;
static systemui_recent_app_s g_recent_apps[APPMGR_APP_LIST_MAX + 1U] = {0};
static systemui_info_card_s g_notification_cards[3] = {0};
static uint32_t g_recent_app_count = 0;
static uint64_t g_last_recent_refresh_ms = 0;
static uint64_t g_last_drawer_clock_min = (uint64_t)-1;
static uint8_t g_recent_visible = 0;
static uint8_t g_notification_visible = 0;
static uint8_t g_control_visible = 0;
static volatile uint32_t g_pending_panel = 0;
static volatile uint32_t g_pending_action = 0xFFFFFFFFU;

static void clear_static_flags(lv_obj_t *obj)
{
	if (obj == NULL) {
		return;
	}

	lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE |
			       LV_OBJ_FLAG_CLICK_FOCUSABLE);
}

static void overlay_setup_root(lv_obj_t *overlay)
{
	if (overlay == NULL) {
		return;
	}

	lv_obj_set_size(overlay, APP_DEFAULT_WIDTH, APP_DEFAULT_HEIGHT);
	lv_obj_set_pos(overlay, 0, 0);
	lv_obj_set_style_bg_color(overlay, lv_color_hex(g_state.palette.overlay), 0);
	lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
	lv_obj_set_style_border_width(overlay, 0, 0);
	lv_obj_set_style_radius(overlay, 0, 0);
	lv_obj_set_style_pad_all(overlay, 0, 0);
	lv_obj_set_style_shadow_width(overlay, 0, 0);
	lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
}

static void overlay_create_handle(lv_obj_t *parent)
{
	lv_obj_t *handle = lv_obj_create(parent);

	lv_obj_set_size(handle, 92, 6);
	lv_obj_align(handle, LV_ALIGN_TOP_MID, 0, 16);
	lv_obj_set_style_radius(handle, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_bg_color(handle, lv_color_hex(g_state.palette.line), 0);
	lv_obj_set_style_bg_opa(handle, LV_OPA_70, 0);
	lv_obj_set_style_border_width(handle, 0, 0);
	lv_obj_set_style_shadow_width(handle, 0, 0);
	clear_static_flags(handle);
}

static uint8_t overlay_any_visible(void)
{
	return g_recent_visible || g_notification_visible || g_control_visible;
}

static void overlay_set_window_visible(uint8_t visible)
{
	window_s *window = lv_port_disp_get_window();

	if (window == NULL) {
		return;
	}

	(void)window_set_visible(window, visible);
}

static void overlay_present_now(uint32_t passes)
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

static void format_fixed_u32(char *out, uint32_t value, uint32_t width)
{
	if (out == NULL || width == 0U) {
		return;
	}

	out[width] = '\0';
	for (uint32_t i = 0; i < width; i++) {
		out[width - 1U - i] = (char)('0' + (value % 10U));
		value /= 10U;
	}
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

static char overlay_upper_char(char ch)
{
	if (ch >= 'a' && ch <= 'z') {
		return (char)(ch - 'a' + 'A');
	}
	return ch;
}

static const char *overlay_path_basename(const char *path)
{
	const char *base = path;

	if (path == NULL) {
		return "";
	}
	for (const char *cursor = path; *cursor != '\0'; cursor++) {
		if (*cursor == '/') {
			base = cursor + 1;
		}
	}
	return base;
}

static void overlay_make_display_name(const char *source, char *out, uint32_t cap)
{
	const char *base = overlay_path_basename(source);
	uint32_t j = 0;
	uint8_t upper_next = 1U;

	if (out == NULL || cap == 0U) {
		return;
	}

	memset(out, 0, cap);
	for (uint32_t i = 0; base[i] != '\0' && j + 1U < cap; i++) {
		char ch = base[i];

		if (ch == '.') {
			break;
		}
		if (ch == '_' || ch == '-' || ch == ' ') {
			if (j + 1U < cap) {
				out[j++] = ' ';
			}
			upper_next = 1U;
			continue;
		}
		out[j++] = upper_next ? overlay_upper_char(ch) : ch;
		upper_next = 0U;
	}
}

static void overlay_fill_recent_title(const char *app_id, const char *process_path,
				      char *out, uint32_t cap)
{
	if (out == NULL || cap == 0U) {
		return;
	}
	if (process_path != NULL && strcmp(process_path, SYSTEMUI_LAUNCHER_PROCESS_PATH) == 0) {
		strncpy(out, "Launcher", cap - 1U);
		return;
	}
	if (app_id != NULL && app_id[0] != '\0') {
		overlay_make_display_name(app_id, out, cap);
		return;
	}
	overlay_make_display_name(process_path, out, cap);
}

static void overlay_fill_recent_subtitle(const char *app_id, const char *process_path,
					 char *out, uint32_t cap)
{
	(void)app_id;
	if (out == NULL || cap == 0U) {
		return;
	}
	if (process_path != NULL && strcmp(process_path, SYSTEMUI_LAUNCHER_PROCESS_PATH) == 0) {
		strncpy(out, "Home screen", cap - 1U);
		return;
	}
	strncpy(out, "System tool", cap - 1U);
}

static void overlay_apply_input_region(void)
{
	window_s *window = lv_port_disp_get_window();
	window_rect_s fullscreen = {0};

	if (window == NULL) {
		return;
	}

	fullscreen.width = window->width;
	fullscreen.height = window->height;

	if (overlay_any_visible()) {
		(void)window_set_input_region(window, &fullscreen);
	} else {
		(void)window_set_input_region(window, NULL);
	}
}

static void overlay_sync_window_state(void)
{
	overlay_apply_input_region();
	overlay_set_window_visible(overlay_any_visible());
}

static void overlay_push_recent_app(const char *app_id, const char *process_path, uint64_t pid)
{
	systemui_recent_app_s *entry = NULL;

	if (process_path == NULL || process_path[0] == '\0' || pid == 0U ||
	    g_recent_app_count >= (APPMGR_APP_LIST_MAX + 1U)) {
		return;
	}

	for (uint32_t i = 0; i < g_recent_app_count; i++) {
		if (strcmp(g_recent_apps[i].process_path, process_path) == 0) {
			g_recent_apps[i].pid = pid;
			return;
		}
	}

	entry = &g_recent_apps[g_recent_app_count++];
	memset(entry, 0, sizeof(*entry));
	strncpy(entry->process_path, process_path, APPMGR_PROCESS_PATH_MAX - 1U);
	overlay_fill_recent_title(app_id, process_path, entry->title, sizeof(entry->title));
	overlay_fill_recent_subtitle(app_id, process_path, entry->subtitle, sizeof(entry->subtitle));
	entry->pid = pid;
}

static void overlay_recent_item_event_cb(lv_event_t *e)
{
	systemui_recent_app_s *entry = (systemui_recent_app_s *)lv_event_get_user_data(e);

	if (entry == NULL || entry->pid == 0U) {
		return;
	}

	if (!window_activate_owner(entry->pid)) {
		log_warn("overlayui: activate recent app %s pid=%d failed\n",
			 entry->process_path, entry->pid);
		return;
	}
	g_recent_visible = 0U;
	if (g_recent_overlay != NULL) {
		lv_obj_add_flag(g_recent_overlay, LV_OBJ_FLAG_HIDDEN);
	}
	overlay_sync_window_state();
}

static void overlay_refresh_recent_apps(uint64_t mono_ms)
{
	appmgr_app_list_response_s response = {0};

	memset(g_recent_apps, 0, sizeof(g_recent_apps));
	g_recent_app_count = 0U;
	overlay_push_recent_app("launcher", SYSTEMUI_LAUNCHER_PROCESS_PATH,
				systemui_lookup_process_pid(SYSTEMUI_LAUNCHER_PROCESS_PATH));

	if (g_appmgr == NULL) {
		g_appmgr = appmgr_client_get();
	}
	if (g_appmgr != NULL && g_appmgr->ops.get_apps != NULL &&
	    g_appmgr->ops.get_apps(g_appmgr, &response) != 0U) {
		for (uint32_t i = 0; i < response.count; i++) {
			overlay_push_recent_app(response.apps[i].app_id,
						response.apps[i].process_path,
						response.apps[i].pid);
		}
	}

	g_last_recent_refresh_ms = mono_ms;
	if (g_recent_list == NULL) {
		return;
	}

	lv_obj_clean(g_recent_list);
	if (g_recent_app_count == 0U) {
		lv_obj_t *empty = lv_label_create(g_recent_list);
		lv_label_set_text(empty, "Nothing is open yet");
		lv_obj_set_style_text_font(empty, &lv_font_montserrat_24, 0);
		lv_obj_set_style_text_color(empty, lv_color_hex(g_state.palette.dim), 0);
		return;
	}

	for (uint32_t i = 0; i < g_recent_app_count; i++) {
		lv_obj_t *item = lv_btn_create(g_recent_list);
		lv_obj_t *title = lv_label_create(item);
		lv_obj_t *subtitle = lv_label_create(item);

		lv_obj_set_size(item, RECENT_PANEL_LIST_WIDTH - 8, RECENT_ITEM_HEIGHT);
		lv_obj_set_style_bg_color(item, lv_color_hex(g_state.palette.panel_alt), 0);
		lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
		lv_obj_set_style_border_color(item, lv_color_hex(g_state.palette.line), 0);
		lv_obj_set_style_border_width(item, 1, 0);
		lv_obj_set_style_radius(item, 26, 0);
		lv_obj_set_style_pad_all(item, 0, 0);
		lv_obj_set_style_shadow_width(item, 10, 0);
		lv_obj_set_style_shadow_color(item, lv_color_hex(g_state.palette.accent), 0);
		lv_obj_set_style_shadow_opa(item, 14, 0);
		lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
		lv_obj_add_event_cb(item, overlay_recent_item_event_cb, LV_EVENT_CLICKED, &g_recent_apps[i]);

		lv_label_set_text(title, g_recent_apps[i].title);
		lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
		lv_obj_set_style_text_color(title, lv_color_hex(g_state.palette.text), 0);
		lv_obj_set_pos(title, 24, 18);

		lv_label_set_text(subtitle, g_recent_apps[i].subtitle);
		lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_16, 0);
		lv_obj_set_style_text_color(subtitle, lv_color_hex(g_state.palette.dim), 0);
		lv_obj_set_pos(subtitle, 24, 48);
	}
}

static lv_obj_t *overlay_create_info_card(lv_obj_t *parent, lv_coord_t y, systemui_info_card_s *card)
{
	lv_obj_t *obj = lv_obj_create(parent);
	lv_coord_t panel_width = 0;

	if (card == NULL) {
		return obj;
	}

	memset(card, 0, sizeof(*card));
	card->card = obj;
	panel_width = lv_obj_get_width(parent);

	lv_obj_set_size(obj, panel_width - 48, TOP_PANEL_CARD_HEIGHT);
	lv_obj_set_pos(obj, 24, y);
	lv_obj_set_style_bg_color(obj, lv_color_hex(g_state.palette.panel_alt), 0);
	lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
	lv_obj_set_style_border_color(obj, lv_color_hex(g_state.palette.line), 0);
	lv_obj_set_style_border_width(obj, 1, 0);
	lv_obj_set_style_radius(obj, 28, 0);
	lv_obj_set_style_pad_all(obj, 0, 0);
	lv_obj_set_style_shadow_width(obj, 12, 0);
	lv_obj_set_style_shadow_color(obj, lv_color_hex(g_state.palette.accent), 0);
	lv_obj_set_style_shadow_opa(obj, 16, 0);
	clear_static_flags(obj);

	card->title = lv_label_create(obj);
	lv_obj_set_pos(card->title, 20, 16);
	lv_obj_set_style_text_font(card->title, &lv_font_montserrat_24, 0);

	card->body = lv_label_create(obj);
	lv_obj_set_pos(card->body, 20, 46);
	lv_obj_set_width(card->body, panel_width - 116);
	lv_label_set_long_mode(card->body, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(card->body, &lv_font_montserrat_16, 0);

	card->chip = lv_label_create(obj);
	lv_obj_align(card->chip, LV_ALIGN_TOP_RIGHT, -18, 14);
	lv_obj_set_style_radius(card->chip, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_pad_hor(card->chip, 12, 0);
	lv_obj_set_style_pad_ver(card->chip, 6, 0);
	lv_obj_set_style_text_font(card->chip, &lv_font_montserrat_16, 0);
	return obj;
}

static lv_obj_t *overlay_create_control_card(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
					     lv_coord_t width, lv_coord_t height)
{
	lv_obj_t *card = lv_obj_create(parent);

	lv_obj_set_size(card, width, height);
	lv_obj_set_pos(card, x, y);
	lv_obj_set_style_bg_color(card, lv_color_hex(g_state.palette.panel_alt), 0);
	lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
	lv_obj_set_style_border_color(card, lv_color_hex(g_state.palette.line), 0);
	lv_obj_set_style_border_width(card, 1, 0);
	lv_obj_set_style_radius(card, 30, 0);
	lv_obj_set_style_pad_all(card, 0, 0);
	lv_obj_set_style_shadow_width(card, 14, 0);
	lv_obj_set_style_shadow_color(card, lv_color_hex(g_state.palette.accent), 0);
	lv_obj_set_style_shadow_opa(card, 16, 0);
	lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
	return card;
}

static lv_obj_t *overlay_create_segment_button(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
					       const char *label_text, lv_event_cb_t event_cb,
					       void *user_data)
{
	lv_obj_t *btn = lv_btn_create(parent);
	lv_obj_t *label = lv_label_create(btn);

	lv_obj_set_size(btn, CONTROL_SEGMENT_WIDTH, 44);
	lv_obj_set_pos(btn, x, y);
	lv_obj_set_style_radius(btn, 22, 0);
	lv_obj_set_style_pad_all(btn, 0, 0);
	lv_obj_set_style_shadow_width(btn, 0, 0);
	lv_obj_set_style_outline_width(btn, 0, 0);
	lv_obj_set_style_border_width(btn, 1, 0);
	lv_obj_add_event_cb(btn, event_cb, LV_EVENT_CLICKED, user_data);
	lv_obj_center(label);
	lv_label_set_text(label, label_text);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
	return btn;
}

static lv_obj_t *overlay_create_card_chip(lv_obj_t *parent)
{
	lv_obj_t *chip = lv_label_create(parent);

	lv_obj_set_style_radius(chip, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_pad_hor(chip, 12, 0);
	lv_obj_set_style_pad_ver(chip, 6, 0);
	lv_obj_set_style_text_font(chip, &lv_font_montserrat_16, 0);
	lv_obj_align(chip, LV_ALIGN_TOP_RIGHT, -18, 16);
	return chip;
}

static void overlay_update_segment_button(lv_obj_t *btn, uint8_t selected)
{
	lv_obj_t *label = NULL;

	if (btn == NULL) {
		return;
	}

	label = lv_obj_get_child(btn, 0);
	lv_obj_set_style_bg_color(btn, lv_color_hex(selected ? g_state.palette.accent :
							 g_state.palette.panel_alt), 0);
	lv_obj_set_style_bg_opa(btn, selected ? LV_OPA_20 : LV_OPA_COVER, 0);
	lv_obj_set_style_border_color(btn, lv_color_hex(selected ? g_state.palette.accent :
							    g_state.palette.line), 0);
	if (label != NULL) {
		lv_obj_set_style_text_color(label, lv_color_hex(selected ? g_state.palette.accent :
								 g_state.palette.text), 0);
	}
}

static void overlay_update_network_button(void)
{
	if (g_control_network_btn == NULL || g_control_network_btn_label == NULL) {
		return;
	}

	if (g_state.net_enabled) {
		lv_label_set_text(g_control_network_btn_label, "Pause network");
		lv_obj_set_style_bg_color(g_control_network_btn, lv_color_hex(g_state.palette.panel_alt), 0);
		lv_obj_set_style_bg_opa(g_control_network_btn, LV_OPA_COVER, 0);
		lv_obj_set_style_border_color(g_control_network_btn, lv_color_hex(g_state.palette.line), 0);
		lv_obj_set_style_text_color(g_control_network_btn_label, lv_color_hex(g_state.palette.text), 0);
		return;
	}

	lv_label_set_text(g_control_network_btn_label, "Resume network");
	lv_obj_set_style_bg_color(g_control_network_btn, lv_color_hex(g_state.palette.accent), 0);
	lv_obj_set_style_bg_opa(g_control_network_btn, LV_OPA_20, 0);
	lv_obj_set_style_border_color(g_control_network_btn, lv_color_hex(g_state.palette.accent), 0);
	lv_obj_set_style_text_color(g_control_network_btn_label, lv_color_hex(g_state.palette.accent), 0);
}

static void overlay_refresh_drawers(void)
{
	systemui_datetime_s local_time = {0};
	uint64_t mono_ms = OSSysCtrlGetMonoTime() / NSEC_PER_MSEC;
	uint64_t ts = normalize_timestamp_seconds(OSSysCtrlGetTimestamp(), mono_ms);
	char body_buf[3][128];
	char time_buf[32];
	char date_buf[32];
	char year_buf[5];
	char month_buf[3];
	char day_buf[3];

	timestamp_to_local_datetime(ts, &local_time);
	g_last_drawer_clock_min = ts / 60ULL;
	format_fixed_u32(time_buf, (uint32_t)local_time.hour, 2U);
	time_buf[2] = ':';
	format_fixed_u32(time_buf + 3, (uint32_t)local_time.minute, 2U);
	time_buf[5] = '\0';
	format_fixed_u32(year_buf, (uint32_t)local_time.year, 4U);
	format_fixed_u32(month_buf, (uint32_t)local_time.month, 2U);
	format_fixed_u32(day_buf, (uint32_t)local_time.day, 2U);
	snprintf(date_buf, sizeof(date_buf), "%s-%s-%s", year_buf, month_buf, day_buf);

	if (g_recent_overlay != NULL) {
		lv_obj_set_style_bg_color(g_recent_overlay, lv_color_hex(g_state.palette.overlay), 0);
	}
	if (g_notification_overlay != NULL) {
		lv_obj_set_style_bg_color(g_notification_overlay, lv_color_hex(g_state.palette.overlay), 0);
	}
	if (g_control_overlay != NULL) {
		lv_obj_set_style_bg_color(g_control_overlay, lv_color_hex(g_state.palette.overlay), 0);
	}
	if (g_recent_panel != NULL) {
		lv_obj_set_style_bg_color(g_recent_panel, lv_color_hex(g_state.palette.panel), 0);
		lv_obj_set_style_border_color(g_recent_panel, lv_color_hex(g_state.palette.line), 0);
	}
	if (g_recent_title_lbl != NULL) {
		lv_obj_set_style_text_color(g_recent_title_lbl, lv_color_hex(g_state.palette.text), 0);
	}
	if (g_recent_subtitle_lbl != NULL) {
		lv_obj_set_style_text_color(g_recent_subtitle_lbl, lv_color_hex(g_state.palette.dim), 0);
	}
	if (g_notification_panel != NULL) {
		lv_obj_set_style_bg_color(g_notification_panel, lv_color_hex(g_state.palette.panel), 0);
		lv_obj_set_style_border_color(g_notification_panel, lv_color_hex(g_state.palette.line), 0);
	}
	if (g_control_panel != NULL) {
		lv_obj_set_style_bg_color(g_control_panel, lv_color_hex(g_state.palette.panel), 0);
		lv_obj_set_style_border_color(g_control_panel, lv_color_hex(g_state.palette.line), 0);
	}

	if (g_notification_title_lbl != NULL) {
		lv_obj_set_style_text_color(g_notification_title_lbl, lv_color_hex(g_state.palette.text), 0);
	}
	if (g_notification_subtitle_lbl != NULL) {
		lv_obj_set_style_text_color(g_notification_subtitle_lbl, lv_color_hex(g_state.palette.dim), 0);
	}

	if (g_notification_cards[0].title != NULL) {
		snprintf(body_buf[0], sizeof(body_buf[0]),
			 "%s on %s. %s mode with the %s backdrop.",
			 time_buf, date_buf, systemui_theme_name(g_state.appearance.theme_mode),
			 systemui_desktop_color_name(g_state.appearance.desktop_color));
		lv_label_set_text(g_notification_cards[0].title, "Right now");
		lv_label_set_text(g_notification_cards[0].body, body_buf[0]);
		lv_label_set_text(g_notification_cards[0].chip,
				  systemui_theme_name(g_state.appearance.theme_mode));
	}
	if (g_notification_cards[1].title != NULL) {
		snprintf(body_buf[1], sizeof(body_buf[1]),
			 "%s. Packet and HTTP bridge follow Statemgr.",
			 g_state.net_enabled ? "Traffic is flowing" : "Traffic is paused");
		lv_label_set_text(g_notification_cards[1].title, "Network pulse");
		lv_label_set_text(g_notification_cards[1].body, body_buf[1]);
		lv_label_set_text(g_notification_cards[1].chip, g_state.net_enabled ? "Online" : "Paused");
	}
	if (g_notification_cards[2].title != NULL) {
		snprintf(body_buf[2], sizeof(body_buf[2]),
			 "Tap the left chip for updates. Tap the right chip for instant controls.");
		lv_label_set_text(g_notification_cards[2].title, "Quick tip");
		lv_label_set_text(g_notification_cards[2].body, body_buf[2]);
		lv_label_set_text(g_notification_cards[2].chip, "Tip");
	}

	for (uint32_t i = 0; i < 3U; i++) {
		uint32_t chip_bg = g_state.palette.accent;
		uint32_t chip_fg = g_state.palette.accent;

		if (g_notification_cards[i].card == NULL) {
			continue;
		}

		lv_obj_set_style_bg_color(g_notification_cards[i].card,
					  lv_color_hex(g_state.palette.panel_alt), 0);
		lv_obj_set_style_border_color(g_notification_cards[i].card,
					      lv_color_hex(g_state.palette.line), 0);
		lv_obj_set_style_text_color(g_notification_cards[i].title,
					    lv_color_hex(g_state.palette.text), 0);
		lv_obj_set_style_text_color(g_notification_cards[i].body,
					    lv_color_hex(g_state.palette.dim), 0);

		if (i == 1U) {
			chip_bg = g_state.net_enabled ? g_state.palette.success : g_state.palette.warning;
			chip_fg = chip_bg;
		}
		lv_obj_set_style_bg_color(g_notification_cards[i].chip, lv_color_hex(chip_bg), 0);
		lv_obj_set_style_bg_opa(g_notification_cards[i].chip, LV_OPA_20, 0);
		lv_obj_set_style_text_color(g_notification_cards[i].chip, lv_color_hex(chip_fg), 0);
	}

	if (g_control_title_lbl != NULL) {
		lv_obj_set_style_text_color(g_control_title_lbl, lv_color_hex(g_state.palette.text), 0);
	}
	if (g_control_subtitle_lbl != NULL) {
		lv_obj_set_style_text_color(g_control_subtitle_lbl, lv_color_hex(g_state.palette.dim), 0);
	}
	if (g_control_theme_card != NULL) {
		lv_obj_set_style_bg_color(g_control_theme_card, lv_color_hex(g_state.palette.panel_alt), 0);
		lv_obj_set_style_border_color(g_control_theme_card, lv_color_hex(g_state.palette.line), 0);
	}
	if (g_control_network_card != NULL) {
		lv_obj_set_style_bg_color(g_control_network_card, lv_color_hex(g_state.palette.panel_alt), 0);
		lv_obj_set_style_border_color(g_control_network_card, lv_color_hex(g_state.palette.line), 0);
	}
	if (g_control_theme_section_lbl != NULL) {
		lv_obj_set_style_text_color(g_control_theme_section_lbl, lv_color_hex(g_state.palette.dim), 0);
	}
	if (g_control_network_section_lbl != NULL) {
		lv_obj_set_style_text_color(g_control_network_section_lbl, lv_color_hex(g_state.palette.dim), 0);
	}
	if (g_control_theme_hint_lbl != NULL) {
		lv_obj_set_style_text_color(g_control_theme_hint_lbl, lv_color_hex(g_state.palette.dim), 0);
	}
	if (g_control_network_hint_lbl != NULL) {
		lv_obj_set_style_text_color(g_control_network_hint_lbl, lv_color_hex(g_state.palette.dim), 0);
	}
	if (g_control_theme_chip != NULL) {
		lv_label_set_text(g_control_theme_chip, systemui_theme_name(g_state.appearance.theme_mode));
		lv_obj_set_style_bg_color(g_control_theme_chip, lv_color_hex(g_state.palette.accent), 0);
		lv_obj_set_style_bg_opa(g_control_theme_chip, LV_OPA_20, 0);
		lv_obj_set_style_text_color(g_control_theme_chip, lv_color_hex(g_state.palette.accent), 0);
	}
	if (g_control_network_chip != NULL) {
		lv_label_set_text(g_control_network_chip, g_state.net_enabled ? "Online" : "Paused");
		lv_obj_set_style_bg_color(g_control_network_chip,
					  lv_color_hex(g_state.net_enabled ? g_state.palette.success :
							 g_state.palette.warning), 0);
		lv_obj_set_style_bg_opa(g_control_network_chip, LV_OPA_20, 0);
		lv_obj_set_style_text_color(g_control_network_chip,
					    lv_color_hex(g_state.net_enabled ? g_state.palette.success :
							 g_state.palette.warning), 0);
	}

	overlay_update_segment_button(g_control_theme_light_btn,
				      g_state.appearance.theme_mode == SYSTEMUI_THEME_MODE_LIGHT);
	overlay_update_segment_button(g_control_theme_dark_btn,
				      g_state.appearance.theme_mode == SYSTEMUI_THEME_MODE_DARK);
	overlay_update_network_button();
}

static void overlay_hide_all(void)
{
	g_recent_visible = 0U;
	g_notification_visible = 0U;
	g_control_visible = 0U;
	if (g_recent_overlay != NULL) {
		lv_obj_add_flag(g_recent_overlay, LV_OBJ_FLAG_HIDDEN);
	}
	if (g_notification_overlay != NULL) {
		lv_obj_add_flag(g_notification_overlay, LV_OBJ_FLAG_HIDDEN);
	}
	if (g_control_overlay != NULL) {
		lv_obj_add_flag(g_control_overlay, LV_OBJ_FLAG_HIDDEN);
	}
	overlay_sync_window_state();
}

static void overlay_set_recent_visible(uint8_t visible, uint64_t mono_ms)
{
	if (visible) {
		g_notification_visible = 0U;
		g_control_visible = 0U;
		if (g_notification_overlay != NULL) {
			lv_obj_add_flag(g_notification_overlay, LV_OBJ_FLAG_HIDDEN);
		}
		if (g_control_overlay != NULL) {
			lv_obj_add_flag(g_control_overlay, LV_OBJ_FLAG_HIDDEN);
		}
	}

	g_recent_visible = visible ? 1U : 0U;
	if (g_recent_overlay != NULL) {
		if (g_recent_visible) {
			lv_obj_clear_flag(g_recent_overlay, LV_OBJ_FLAG_HIDDEN);
			overlay_refresh_recent_apps(mono_ms);
			overlay_present_now(2U);
		} else {
			lv_obj_add_flag(g_recent_overlay, LV_OBJ_FLAG_HIDDEN);
		}
	}
	overlay_sync_window_state();
}

static void overlay_set_notification_visible(uint8_t visible, uint64_t mono_ms)
{
	(void)mono_ms;
	if (visible) {
		g_recent_visible = 0U;
		g_control_visible = 0U;
		if (g_recent_overlay != NULL) {
			lv_obj_add_flag(g_recent_overlay, LV_OBJ_FLAG_HIDDEN);
		}
		if (g_control_overlay != NULL) {
			lv_obj_add_flag(g_control_overlay, LV_OBJ_FLAG_HIDDEN);
		}
	}

	g_notification_visible = visible ? 1U : 0U;
	if (g_notification_overlay != NULL) {
		if (g_notification_visible) {
			overlay_refresh_drawers();
			lv_obj_clear_flag(g_notification_overlay, LV_OBJ_FLAG_HIDDEN);
			overlay_present_now(2U);
		} else {
			lv_obj_add_flag(g_notification_overlay, LV_OBJ_FLAG_HIDDEN);
		}
	}
	overlay_sync_window_state();
}

static void overlay_set_control_visible(uint8_t visible, uint64_t mono_ms)
{
	(void)mono_ms;
	if (visible) {
		g_recent_visible = 0U;
		g_notification_visible = 0U;
		if (g_recent_overlay != NULL) {
			lv_obj_add_flag(g_recent_overlay, LV_OBJ_FLAG_HIDDEN);
		}
		if (g_notification_overlay != NULL) {
			lv_obj_add_flag(g_notification_overlay, LV_OBJ_FLAG_HIDDEN);
		}
	}

	g_control_visible = visible ? 1U : 0U;
	if (g_control_overlay != NULL) {
		if (g_control_visible) {
			overlay_refresh_drawers();
			lv_obj_clear_flag(g_control_overlay, LV_OBJ_FLAG_HIDDEN);
			overlay_present_now(2U);
		} else {
			lv_obj_add_flag(g_control_overlay, LV_OBJ_FLAG_HIDDEN);
		}
	}
	overlay_sync_window_state();
}

static void overlay_request_apply(uint32_t panel, uint32_t action, uint64_t mono_ms)
{
	uint8_t open = action == SYSTEMUI_OVERLAY_ACTION_OPEN ? 1U : 0U;

	if (panel == SYSTEMUI_OVERLAY_PANEL_ALL) {
		overlay_hide_all();
		return;
	}

	if (panel == SYSTEMUI_OVERLAY_PANEL_NOTIFICATION) {
		if (action == SYSTEMUI_OVERLAY_ACTION_TOGGLE) {
			open = g_notification_visible ? 0U : 1U;
		}
		overlay_set_notification_visible(open, mono_ms);
		return;
	}
	if (panel == SYSTEMUI_OVERLAY_PANEL_CONTROL) {
		if (action == SYSTEMUI_OVERLAY_ACTION_TOGGLE) {
			open = g_control_visible ? 0U : 1U;
		}
		overlay_set_control_visible(open, mono_ms);
		return;
	}
	if (panel == SYSTEMUI_OVERLAY_PANEL_RECENT) {
		if (action == SYSTEMUI_OVERLAY_ACTION_TOGGLE) {
			open = g_recent_visible ? 0U : 1U;
		}
		overlay_set_recent_visible(open, mono_ms);
	}
}

static void overlay_process_pending_requests(uint64_t mono_ms)
{
	uint32_t panel = g_pending_panel;
	uint32_t action = g_pending_action;

	if (action == 0xFFFFFFFFU) {
		return;
	}

	g_pending_action = 0xFFFFFFFFU;
	g_pending_panel = 0U;
	overlay_request_apply(panel, action, mono_ms);
}

static void overlay_recent_overlay_event_cb(lv_event_t *e)
{
	(void)e;
	overlay_set_recent_visible(0U, OSSysCtrlGetMonoTime() / NSEC_PER_MSEC);
}

static void overlay_notification_overlay_event_cb(lv_event_t *e)
{
	(void)e;
	overlay_set_notification_visible(0U, OSSysCtrlGetMonoTime() / NSEC_PER_MSEC);
}

static void overlay_control_overlay_event_cb(lv_event_t *e)
{
	(void)e;
	overlay_set_control_visible(0U, OSSysCtrlGetMonoTime() / NSEC_PER_MSEC);
}

static void overlay_control_theme_event_cb(lv_event_t *e)
{
	uint32_t mode = (uint32_t)(uintptr_t)lv_event_get_user_data(e);

	if (!systemui_write_state_u64("ui.theme.mode", STATEMGR_VALUE_TYPE_U32, mode)) {
		log_warn("overlayui: write theme mode failed\n");
		return;
	}

	g_state.appearance.theme_mode = mode == SYSTEMUI_THEME_MODE_DARK ?
					SYSTEMUI_THEME_MODE_DARK : SYSTEMUI_THEME_MODE_LIGHT;
	g_state.appearance_dirty = 1U;
	systemui_update_palette(&g_state);
	overlay_refresh_drawers();
}

static void overlay_control_network_event_cb(lv_event_t *e)
{
	uint8_t enabled = g_state.net_enabled ? 0U : 1U;

	(void)e;
	if (!systemui_write_state_u64("net.enabled", STATEMGR_VALUE_TYPE_BOOL, enabled)) {
		log_warn("overlayui: write network state failed\n");
		return;
	}

	g_state.net_enabled = enabled;
	overlay_refresh_drawers();
}

static void overlay_create_recent_overlay(lv_obj_t *scr)
{
	lv_obj_t *title = NULL;
	lv_obj_t *subtitle = NULL;

	g_recent_overlay = lv_obj_create(scr);
	overlay_setup_root(g_recent_overlay);
	lv_obj_add_event_cb(g_recent_overlay, overlay_recent_overlay_event_cb, LV_EVENT_CLICKED, NULL);
	lv_obj_add_flag(g_recent_overlay, LV_OBJ_FLAG_HIDDEN);

	g_recent_panel = lv_obj_create(g_recent_overlay);
	lv_obj_set_size(g_recent_panel, RECENT_PANEL_WIDTH, RECENT_PANEL_HEIGHT);
	lv_obj_set_pos(g_recent_panel, (APP_DEFAULT_WIDTH - RECENT_PANEL_WIDTH) / 2, RECENT_PANEL_Y);
	lv_obj_set_style_bg_color(g_recent_panel, lv_color_hex(g_state.palette.panel), 0);
	lv_obj_set_style_bg_opa(g_recent_panel, LV_OPA_COVER, 0);
	lv_obj_set_style_border_color(g_recent_panel, lv_color_hex(g_state.palette.line), 0);
	lv_obj_set_style_border_width(g_recent_panel, 1, 0);
	lv_obj_set_style_radius(g_recent_panel, 36, 0);
	lv_obj_set_style_pad_all(g_recent_panel, 0, 0);
	lv_obj_set_style_shadow_width(g_recent_panel, 20, 0);
	lv_obj_set_style_shadow_color(g_recent_panel, lv_color_hex(g_state.palette.accent), 0);
	lv_obj_set_style_shadow_opa(g_recent_panel, 22, 0);
	clear_static_flags(g_recent_panel);
	overlay_create_handle(g_recent_panel);

	title = lv_label_create(g_recent_panel);
	g_recent_title_lbl = title;
	lv_label_set_text(title, "Quick switch");
	lv_obj_set_pos(title, 28, 34);
	lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
	lv_obj_set_style_text_color(title, lv_color_hex(g_state.palette.text), 0);

	subtitle = lv_label_create(g_recent_panel);
	g_recent_subtitle_lbl = subtitle;
	lv_label_set_text(subtitle, "Jump back into what you were just using.");
	lv_obj_set_pos(subtitle, 28, 70);
	lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(subtitle, lv_color_hex(g_state.palette.dim), 0);

	g_recent_list = lv_obj_create(g_recent_panel);
	lv_obj_set_pos(g_recent_list, RECENT_PANEL_LIST_X, RECENT_PANEL_LIST_Y);
	lv_obj_set_size(g_recent_list, RECENT_PANEL_LIST_WIDTH, RECENT_PANEL_LIST_HEIGHT);
	lv_obj_set_style_bg_opa(g_recent_list, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(g_recent_list, 0, 0);
	lv_obj_set_style_pad_all(g_recent_list, 0, 0);
	lv_obj_set_style_pad_row(g_recent_list, RECENT_ITEM_GAP, 0);
	lv_obj_set_style_radius(g_recent_list, 0, 0);
	lv_obj_set_layout(g_recent_list, LV_LAYOUT_FLEX);
	lv_obj_set_flex_flow(g_recent_list, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_flex_align(g_recent_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
			      LV_FLEX_ALIGN_START);
}

static void overlay_create_notification_overlay(lv_obj_t *scr)
{
	lv_obj_t *panel = NULL;

	g_notification_overlay = lv_obj_create(scr);
	overlay_setup_root(g_notification_overlay);
	lv_obj_add_flag(g_notification_overlay, LV_OBJ_FLAG_HIDDEN);

	panel = lv_obj_create(g_notification_overlay);
	g_notification_panel = panel;
	lv_obj_set_size(panel, NOTIFICATION_PANEL_WIDTH, NOTIFICATION_PANEL_HEIGHT);
	lv_obj_set_pos(panel, TOP_SHEET_MARGIN, TOP_SHEET_Y);
	lv_obj_set_style_bg_color(panel, lv_color_hex(g_state.palette.panel), 0);
	lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
	lv_obj_set_style_border_color(panel, lv_color_hex(g_state.palette.line), 0);
	lv_obj_set_style_border_width(panel, 1, 0);
	lv_obj_set_style_radius(panel, 36, 0);
	lv_obj_set_style_pad_all(panel, 0, 0);
	lv_obj_set_style_shadow_width(panel, 20, 0);
	lv_obj_set_style_shadow_color(panel, lv_color_hex(g_state.palette.accent), 0);
	lv_obj_set_style_shadow_opa(panel, 22, 0);
	lv_obj_add_flag(panel, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
	overlay_create_handle(panel);

	g_notification_title_lbl = lv_label_create(panel);
	lv_label_set_text(g_notification_title_lbl, "Notifications");
	lv_obj_set_pos(g_notification_title_lbl, 24, 36);
	lv_obj_set_style_text_font(g_notification_title_lbl, &lv_font_montserrat_32, 0);

	g_notification_subtitle_lbl = lv_label_create(panel);
	lv_label_set_text(g_notification_subtitle_lbl, "Time, network, and the little things moving across your desk.");
	lv_obj_set_pos(g_notification_subtitle_lbl, 24, 72);
	lv_obj_set_style_text_font(g_notification_subtitle_lbl, &lv_font_montserrat_16, 0);

	overlay_create_info_card(panel, 114, &g_notification_cards[0]);
	overlay_create_info_card(panel, 114 + TOP_PANEL_CARD_HEIGHT + TOP_PANEL_CARD_GAP,
				 &g_notification_cards[1]);
	overlay_create_info_card(panel, 114 + (TOP_PANEL_CARD_HEIGHT + TOP_PANEL_CARD_GAP) * 2,
				 &g_notification_cards[2]);
}

static void overlay_create_control_overlay(lv_obj_t *scr)
{
	lv_obj_t *panel = NULL;

	g_control_overlay = lv_obj_create(scr);
	overlay_setup_root(g_control_overlay);
	lv_obj_add_event_cb(g_control_overlay, overlay_control_overlay_event_cb, LV_EVENT_CLICKED, NULL);
	lv_obj_add_flag(g_control_overlay, LV_OBJ_FLAG_HIDDEN);

	panel = lv_obj_create(g_control_overlay);
	g_control_panel = panel;
	lv_obj_set_size(panel, CONTROL_PANEL_WIDTH, CONTROL_PANEL_HEIGHT);
	lv_obj_set_pos(panel, TOP_SHEET_MARGIN, TOP_SHEET_Y);
	lv_obj_set_style_bg_color(panel, lv_color_hex(g_state.palette.panel), 0);
	lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
	lv_obj_set_style_border_color(panel, lv_color_hex(g_state.palette.line), 0);
	lv_obj_set_style_border_width(panel, 1, 0);
	lv_obj_set_style_radius(panel, 36, 0);
	lv_obj_set_style_pad_all(panel, 0, 0);
	lv_obj_set_style_shadow_width(panel, 20, 0);
	lv_obj_set_style_shadow_color(panel, lv_color_hex(g_state.palette.accent), 0);
	lv_obj_set_style_shadow_opa(panel, 22, 0);
	lv_obj_add_flag(panel, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
	overlay_create_handle(panel);

	g_control_title_lbl = lv_label_create(panel);
	lv_label_set_text(g_control_title_lbl, "Quick controls");
	lv_obj_set_pos(g_control_title_lbl, 24, 36);
	lv_obj_set_style_text_font(g_control_title_lbl, &lv_font_montserrat_32, 0);

	g_control_subtitle_lbl = lv_label_create(panel);
	lv_label_set_text(g_control_subtitle_lbl, "Theme and network, one tap away.");
	lv_obj_set_pos(g_control_subtitle_lbl, 24, 72);
	lv_obj_set_width(g_control_subtitle_lbl, CONTROL_PANEL_WIDTH - 48);
	lv_label_set_long_mode(g_control_subtitle_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(g_control_subtitle_lbl, &lv_font_montserrat_16, 0);

	g_control_theme_card = overlay_create_control_card(panel, CONTROL_PANEL_INSET, CONTROL_CARD_TOP,
							   CONTROL_CARD_HALF_WIDTH,
							   CONTROL_CARD_SMALL_HEIGHT);
	g_control_theme_section_lbl = lv_label_create(g_control_theme_card);
	lv_label_set_text(g_control_theme_section_lbl, "Look");
	lv_obj_set_pos(g_control_theme_section_lbl, 20, 18);
	lv_obj_set_style_text_font(g_control_theme_section_lbl, &lv_font_montserrat_16, 0);
	g_control_theme_chip = overlay_create_card_chip(g_control_theme_card);

	g_control_theme_hint_lbl = lv_label_create(g_control_theme_card);
	lv_label_set_text(g_control_theme_hint_lbl, "Flip the shell between bright air and night focus.");
	lv_obj_set_pos(g_control_theme_hint_lbl, 20, 48);
	lv_obj_set_width(g_control_theme_hint_lbl, CONTROL_CARD_HALF_WIDTH - 40);
	lv_label_set_long_mode(g_control_theme_hint_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(g_control_theme_hint_lbl, &lv_font_montserrat_16, 0);

	g_control_theme_light_btn = overlay_create_segment_button(g_control_theme_card, 20, 118, "Light",
						  overlay_control_theme_event_cb,
						  (void *)(uintptr_t)SYSTEMUI_THEME_MODE_LIGHT);
	g_control_theme_dark_btn = overlay_create_segment_button(g_control_theme_card,
						 20 + CONTROL_SEGMENT_WIDTH + 12, 118, "Dark",
						 overlay_control_theme_event_cb,
						 (void *)(uintptr_t)SYSTEMUI_THEME_MODE_DARK);

	g_control_network_card = overlay_create_control_card(panel,
				     CONTROL_PANEL_INSET + CONTROL_CARD_HALF_WIDTH + CONTROL_CARD_GAP,
				     CONTROL_CARD_TOP,
				     CONTROL_CARD_HALF_WIDTH,
				     CONTROL_CARD_SMALL_HEIGHT);
	g_control_network_section_lbl = lv_label_create(g_control_network_card);
	lv_label_set_text(g_control_network_section_lbl, "Network");
	lv_obj_set_pos(g_control_network_section_lbl, 20, 18);
	lv_obj_set_style_text_font(g_control_network_section_lbl, &lv_font_montserrat_16, 0);
	g_control_network_chip = overlay_create_card_chip(g_control_network_card);

	g_control_network_hint_lbl = lv_label_create(g_control_network_card);
	lv_label_set_text(g_control_network_hint_lbl,
			  "Pause packet and HTTP traffic without leaving the desktop.");
	lv_obj_set_pos(g_control_network_hint_lbl, 20, 48);
	lv_obj_set_width(g_control_network_hint_lbl, CONTROL_CARD_HALF_WIDTH - 40);
	lv_label_set_long_mode(g_control_network_hint_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(g_control_network_hint_lbl, &lv_font_montserrat_16, 0);

	g_control_network_btn = lv_btn_create(g_control_network_card);
	lv_obj_set_size(g_control_network_btn, CONTROL_CARD_HALF_WIDTH - 40, 52);
	lv_obj_set_pos(g_control_network_btn, 20, 118);
	lv_obj_set_style_radius(g_control_network_btn, 26, 0);
	lv_obj_set_style_border_width(g_control_network_btn, 1, 0);
	lv_obj_set_style_pad_all(g_control_network_btn, 0, 0);
	lv_obj_set_style_shadow_width(g_control_network_btn, 0, 0);
	lv_obj_add_event_cb(g_control_network_btn, overlay_control_network_event_cb, LV_EVENT_CLICKED, NULL);

	g_control_network_btn_label = lv_label_create(g_control_network_btn);
	lv_obj_center(g_control_network_btn_label);
	lv_obj_set_style_text_font(g_control_network_btn_label, &lv_font_montserrat_24, 0);
}

static void overlay_rebuild_ui(uint64_t mono_ms)
{
	lv_obj_t *scr = lv_scr_act();

	lv_obj_clean(scr);
	memset(g_notification_cards, 0, sizeof(g_notification_cards));
	g_root_scr = scr;
	g_recent_overlay = NULL;
	g_recent_panel = NULL;
	g_recent_list = NULL;
	g_recent_title_lbl = NULL;
	g_recent_subtitle_lbl = NULL;
	g_notification_overlay = NULL;
	g_notification_panel = NULL;
	g_notification_title_lbl = NULL;
	g_notification_subtitle_lbl = NULL;
	g_control_overlay = NULL;
	g_control_panel = NULL;
	g_control_title_lbl = NULL;
	g_control_subtitle_lbl = NULL;
	g_control_theme_card = NULL;
	g_control_network_card = NULL;
	g_control_theme_section_lbl = NULL;
	g_control_network_section_lbl = NULL;
	g_control_theme_hint_lbl = NULL;
	g_control_network_hint_lbl = NULL;
	g_control_theme_chip = NULL;
	g_control_network_chip = NULL;
	g_control_theme_light_btn = NULL;
	g_control_theme_dark_btn = NULL;
	g_control_network_btn = NULL;
	g_control_network_btn_label = NULL;
	g_last_drawer_clock_min = (uint64_t)-1;

	lv_obj_set_style_bg_color(scr, lv_color_hex(g_state.palette.overlay), 0);
	lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
	lv_obj_set_style_border_width(scr, 0, 0);
	lv_obj_set_style_pad_all(scr, 0, 0);
	clear_static_flags(scr);

	overlay_create_recent_overlay(scr);
	overlay_create_notification_overlay(scr);
	overlay_create_control_overlay(scr);
	overlay_refresh_drawers();

	if (g_recent_visible) {
		overlay_set_recent_visible(1U, mono_ms);
	} else if (g_notification_visible) {
		overlay_set_notification_visible(1U, mono_ms);
	} else if (g_control_visible) {
		overlay_set_control_visible(1U, mono_ms);
	} else {
		overlay_hide_all();
	}
}

IPC_ENDPOINT void overlay_service_entry(uint64_t cref, uint64_t method,
					uint64_t arg1, uint64_t arg2, uint64_t arg3)
{
	(void)cref;
	(void)arg3;

	if (method == IPC_SYSTEMUI_OVERLAY_SERVICE_FUNCTION_REQUEST) {
		g_pending_panel = (uint32_t)arg1;
		g_pending_action = (uint32_t)arg2;
		if ((uint32_t)arg2 == SYSTEMUI_OVERLAY_ACTION_OPEN ||
		    (uint32_t)arg2 == SYSTEMUI_OVERLAY_ACTION_TOGGLE) {
			overlay_set_window_visible(1U);
		}
		OSIpcEndPointPoolReply(1);
	} else {
		OSIpcEndPointPoolReply(0);
	}

	while (1) {}
}

static void overlay_publish_service(void)
{
	if (!sys_register_service_pool(IPC_SYSTEMUI_OVERLAY_SERVICE_ID, &overlay_service_entry)) {
		log_warn("overlayui: register overlay service failed\n");
		return;
	}

	log_info("overlayui: service published\n");
}

static void overlay_on_create(app_s *app)
{
	(void)app;
	systemui_runtime_init(&g_state);
	(void)systemui_sync_appearance(&g_state, 0U, 1U);
	overlay_rebuild_ui(0U);
	overlay_present_now(2U);
	overlay_publish_service();
	overlay_hide_all();
	overlay_set_window_visible(0U);
}

static void overlay_on_foreground(app_s *app)
{
	uint64_t mono_ms = OSSysCtrlGetMonoTime() / NSEC_PER_MSEC;

	(void)app;
	overlay_process_pending_requests(mono_ms);
	if (systemui_sync_appearance(&g_state, mono_ms, 0U) || g_state.appearance_dirty) {
		g_state.appearance_dirty = 0U;
		overlay_rebuild_ui(mono_ms);
	}
	overlay_present_now(2U);
}

static void overlay_on_update(app_s *app, uint64_t mono_ms)
{
	uint64_t ts = 0;

	(void)app;
	overlay_process_pending_requests(mono_ms);
	if (systemui_sync_appearance(&g_state, mono_ms, 0U) || g_state.appearance_dirty) {
		g_state.appearance_dirty = 0U;
		overlay_rebuild_ui(mono_ms);
	}
	if (g_recent_visible && mono_ms >= g_last_recent_refresh_ms + RECENT_REFRESH_INTERVAL_MS) {
		overlay_refresh_recent_apps(mono_ms);
	}
	if (g_notification_visible || g_control_visible) {
		ts = normalize_timestamp_seconds(OSSysCtrlGetTimestamp(), mono_ms);
		if ((ts / 60ULL) != g_last_drawer_clock_min) {
			overlay_refresh_drawers();
		}
	}
}

int main(int argc, char *argv[])
{
	app_s app = {0};
	app_config_s config = {
		.name = "overlay",
		.x = APP_DEFAULT_X,
		.y = APP_DEFAULT_Y,
		.z = SYSTEMUI_OVERLAY_WINDOW_Z,
		.width = APP_DEFAULT_WIDTH,
		.height = APP_DEFAULT_HEIGHT,
		.window_flags = 0U,
		.enable_input = 1U,
	};
	app_lifecycle_ops_s lifecycle = {
		.on_create = overlay_on_create,
		.on_foreground = overlay_on_foreground,
		.on_update = overlay_on_update,
	};

	(void)argc;
	(void)argv;
	log_info("overlayui start\n");
	app_init(&app, &config, &lifecycle, NULL);
	return app_run(&app);
}
