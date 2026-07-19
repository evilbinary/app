#include "lvgl.h"
#include "app.h"
#include "libwindow/window.h"
#include "log.h"
#include "libkernel/capcall.h"
#include "libsystem/devmgr_client.h"
#include "libsystem/net_client.h"
#include "libsystem/statemgr_client.h"
#include "libsystem/systemd_client.h"
#include "stdio.h"
#include "string.h"
#include "stdint.h"

#define SYSTEM_OVERLAY_GAP 24
#define CONTENT_X 40
#define CONTENT_Y SYSTEM_OVERLAY_GAP
#define CONTENT_W 944
#define CONTENT_H (APP_DEFAULT_HEIGHT - CONTENT_Y - SYSTEM_OVERLAY_GAP)
#define PANEL_INSET 28
#define COLUMN_GAP 16
#define LEFT_COL_W 432
#define RIGHT_COL_W 440
#define HERO_Y 154
#define HERO_H 168
#define MEMORY_Y (HERO_Y + HERO_H + COLUMN_GAP)
#define MEMORY_H 92
#define PROCESS_Y (MEMORY_Y + MEMORY_H + COLUMN_GAP)
#define PROCESS_H (CONTENT_H - PROCESS_Y - PANEL_INSET)
#define PLATFORM_CARD_W 196
#define PLATFORM_CARD_H 64
#define PROCESS_ACTION_REFRESH_W 104
#define PROCESS_ACTION_END_W 132
#define PROCESS_LIST_Y 82
#define PROCESS_LIST_H (PROCESS_H - PROCESS_LIST_Y - 16)
#define PROCESS_ROW_H 54
#define REFRESH_INTERVAL_MS 1000ULL
#define THEME_REFRESH_INTERVAL_MS 100ULL
#define STORAGE_SECTOR_SIZE 512ULL
#define MONITOR_PROCESS_PATH "./bin/monitor.elf"

#define COLOR_BG          0xfff4ee
#define COLOR_BG_ALT      0xeaf4ff
#define COLOR_PANEL       0xfffffb
#define COLOR_PANEL_ALT   0xfff1eb
#define COLOR_PANEL_SOFT  0xe8f8f0
#define COLOR_LINE        0xf0ddd2
#define COLOR_TEXT        0x24324a
#define COLOR_DIM         0x7d8198
#define COLOR_ACCENT      0xff7d5c
#define COLOR_ACCENT_SOFT 0xffe6dd
#define COLOR_WARM        0xffb85e
#define COLOR_WARM_SOFT   0xfff0d2
#define COLOR_ALERT       0xf08d80
#define COLOR_ALERT_SOFT  0xffe5e1

typedef enum monitor_theme_mode {
	MONITOR_THEME_LIGHT = 0,
	MONITOR_THEME_DARK = 1,
} monitor_theme_mode_e;

typedef struct monitor_theme {
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
	uint32_t alert;
	uint32_t alert_soft;
} monitor_theme_s;

typedef struct monitor_datetime {
	int year;
	int month;
	int day;
	int weekday;
	int hour;
	int minute;
	int second;
} monitor_datetime_s;

typedef enum process_action {
	PROCESS_ACTION_REFRESH = 0,
	PROCESS_ACTION_TERMINATE = 1,
} process_action_e;

typedef struct processmgr_state {
	systemd_process_list_response_s processes;
	uint64_t self_pid;
	uint64_t selected_pid;
	uint64_t last_refresh_ms;
	uint64_t interaction_until_ms;
	uint64_t last_proc_count;
	uint64_t last_thread_count;
	uint8_t has_selection;
	char notice_text[128];
	uint32_t notice_color;
} processmgr_state_s;

static processmgr_state_s g_processmgr = {0};

static systemd_client_s *g_systemd = NULL;
static devmgr_client_s *g_devmgr = NULL;
static net_client_s *g_net = NULL;
static statemgr_client_s *g_statemgr = NULL;
static uint32_t g_theme_mode = MONITOR_THEME_LIGHT;
static uint64_t g_last_theme_ms = 0;
static uint64_t g_last_theme_revision = 0;

static lv_obj_t *g_uptime_value = NULL;
static lv_obj_t *g_clock_value = NULL;
static lv_obj_t *g_date_value = NULL;
static lv_obj_t *g_cpu_value = NULL;
static lv_obj_t *g_core_value = NULL;
static lv_obj_t *g_storage_value = NULL;
static lv_obj_t *g_network_value = NULL;
static lv_obj_t *g_memory_value = NULL;
static lv_obj_t *g_memory_meta = NULL;
static lv_obj_t *g_memory_bar = NULL;
static lv_obj_t *g_proc_value = NULL;
static lv_obj_t *g_thread_value = NULL;
static lv_obj_t *g_selected_value = NULL;
static lv_obj_t *g_process_summary = NULL;
static lv_obj_t *g_process_detail = NULL;
static lv_obj_t *g_process_list = NULL;
static lv_obj_t *g_terminate_btn = NULL;
static lv_obj_t *g_terminate_btn_label = NULL;

static const char *g_weekday_names[] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat",
};

static const monitor_theme_s g_monitor_theme_light = {
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
	.alert = COLOR_ALERT,
	.alert_soft = COLOR_ALERT_SOFT,
};

static const monitor_theme_s g_monitor_theme_dark = {
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
	.alert = 0xf3a096,
	.alert_soft = 0x4a3036,
};

static const monitor_theme_s *monitor_theme(void)
{
	return g_theme_mode == MONITOR_THEME_DARK ?
		&g_monitor_theme_dark : &g_monitor_theme_light;
}

static void create_ui(void);
static void processmgr_refresh(uint64_t mono_ms, uint8_t force);
static void processmgr_render_list(void);
static void refresh_clock_and_uptime(uint64_t mono_ms);
static void refresh_platform_metrics(void);
static void refresh_memory_and_processes(void);

static uint8_t monitor_read_theme_mode(uint32_t *theme_mode_out, uint64_t *revision_out)
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

	*theme_mode_out = (uint32_t)response.entry.value_u64 == MONITOR_THEME_DARK ?
		MONITOR_THEME_DARK : MONITOR_THEME_LIGHT;
	if (revision_out != NULL) {
		*revision_out = revision;
	}
	return 1U;
}

static void monitor_rebuild_ui(uint64_t mono_ms)
{
	lv_obj_t *scr = lv_scr_act();

	g_uptime_value = NULL;
	g_clock_value = NULL;
	g_date_value = NULL;
	g_cpu_value = NULL;
	g_core_value = NULL;
	g_storage_value = NULL;
	g_network_value = NULL;
	g_memory_value = NULL;
	g_memory_meta = NULL;
	g_memory_bar = NULL;
	g_proc_value = NULL;
	g_thread_value = NULL;
	g_selected_value = NULL;
	g_process_summary = NULL;
	g_process_detail = NULL;
	g_process_list = NULL;
	g_terminate_btn = NULL;
	g_terminate_btn_label = NULL;
	lv_obj_clean(scr);
	create_ui();
	refresh_clock_and_uptime(mono_ms);
	refresh_platform_metrics();
	refresh_memory_and_processes();
}

static void monitor_refresh_theme(uint64_t mono_ms, uint8_t force)
{
	uint32_t theme_mode = g_theme_mode;
	uint64_t revision = 0;

	if (!force && mono_ms < g_last_theme_ms + THEME_REFRESH_INTERVAL_MS) {
		return;
	}
	g_last_theme_ms = mono_ms;

	if (!monitor_read_theme_mode(&theme_mode, &revision)) {
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
	monitor_rebuild_ui(mono_ms);
	log_info("monitor theme synced: mode=%u rev=%llu\n",
		 (unsigned int)g_theme_mode,
		 (unsigned long long)revision);
}

static void processmgr_mark_interacting(void)
{
	g_processmgr.interaction_until_ms = (OSSysCtrlGetMonoTime() / NSEC_PER_MSEC) + 1500ULL;
}

static void processmgr_row_interaction_event_cb(lv_event_t *e)
{
	lv_event_code_t code = lv_event_get_code(e);

	if (code == LV_EVENT_PRESSED ||
	    code == LV_EVENT_PRESSING ||
	    code == LV_EVENT_RELEASED ||
	    code == LV_EVENT_SCROLL_BEGIN ||
	    code == LV_EVENT_SCROLL ||
	    code == LV_EVENT_SCROLL_END) {
		processmgr_mark_interacting();
	}
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
	lv_obj_set_style_pad_top(chip, 7, 0);
	lv_obj_set_style_pad_bottom(chip, 7, 0);
	lv_obj_set_style_text_font(chip, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(chip, lv_color_hex(fg), 0);
	lv_label_set_text(chip, text);
	clear_static_flags(chip);
	return chip;
}

static lv_obj_t *create_action_button(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
				      lv_coord_t w, const char *text, uint32_t bg,
				      uint32_t border, uint32_t fg, process_action_e action,
				      lv_obj_t **label_out)
{
	const monitor_theme_s *theme = monitor_theme();
	lv_obj_t *btn = lv_btn_create(parent);
	lv_obj_t *label = lv_label_create(btn);

	lv_obj_set_pos(btn, x, y);
	lv_obj_set_size(btn, w, 40);
	lv_obj_set_style_radius(btn, 18, 0);
	lv_obj_set_style_bg_color(btn, lv_color_hex(bg), 0);
	lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
	lv_obj_set_style_bg_color(btn, lv_color_hex(theme->panel_alt), LV_STATE_DISABLED);
	lv_obj_set_style_border_color(btn, lv_color_hex(border), 0);
	lv_obj_set_style_border_color(btn, lv_color_hex(theme->line), LV_STATE_DISABLED);
	lv_obj_set_style_border_width(btn, 1, 0);
	lv_obj_set_style_shadow_width(btn, 0, 0);
	lv_obj_set_style_outline_width(btn, 0, LV_STATE_FOCUSED);
	lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

	lv_label_set_text(label, text);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(fg), 0);
	lv_obj_center(label);

	if (label_out != NULL) {
		*label_out = label;
	}
	(void)action;
	return btn;
}

static void format_two_digits(char *dst, int value)
{
	dst[0] = (char)('0' + ((value / 10) % 10));
	dst[1] = (char)('0' + (value % 10));
	dst[2] = '\0';
}

static void format_hex_byte(char *dst, uint8_t value)
{
	static const char hex[] = "0123456789abcdef";

	dst[0] = hex[(value >> 4) & 0x0F];
	dst[1] = hex[value & 0x0F];
	dst[2] = '\0';
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
	uint32_t m = mp + (mp < 10 ? 3 : (uint32_t)-9);

	y += (m <= 2);
	*year = y;
	*month = (int)m;
	*day = (int)d;
}

static void timestamp_to_local_datetime(uint64_t ts_sec, monitor_datetime_s *out)
{
	int64_t local_sec = (int64_t)ts_sec + (8 * 3600);
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

	if (sample_mono_ms == 0) {
		sample_raw_ts = raw_ts;
		sample_mono_ms = mono_ms;
	}

	if (mono_ms > sample_mono_ms && raw_ts >= sample_raw_ts) {
		uint64_t mono_delta_ms = mono_ms - sample_mono_ms;
		if (mono_delta_ms >= 200) {
			uint64_t raw_delta = raw_ts - sample_raw_ts;
			if (raw_delta >= (mono_delta_ms / 2)) {
				ts_in_msec = 1;
				unit_locked = 1;
			} else if (raw_delta <= ((mono_delta_ms / 200) + 2)) {
				ts_in_msec = 0;
				unit_locked = 1;
			}
			sample_raw_ts = raw_ts;
			sample_mono_ms = mono_ms;
		}
	}

	if (!unit_locked && raw_ts > 4102444800ULL) {
		ts_in_msec = 1;
	}

	return ts_in_msec ? (raw_ts / 1000ULL) : raw_ts;
}

static void format_time_string(const monitor_datetime_s *dt, char *out, uint32_t cap)
{
	char hh[3];
	char mm[3];
	char ss[3];

	if (dt == NULL || out == NULL || cap == 0U) {
		return;
	}

	format_two_digits(hh, dt->hour);
	format_two_digits(mm, dt->minute);
	format_two_digits(ss, dt->second);
	snprintf(out, cap, "%s:%s:%s", hh, mm, ss);
}

static void format_date_string(const monitor_datetime_s *dt, char *out, uint32_t cap)
{
	char month[3];
	char day[3];

	if (dt == NULL || out == NULL || cap == 0U) {
		return;
	}

	format_two_digits(month, dt->month);
	format_two_digits(day, dt->day);
	snprintf(out, cap, "%d-%s-%s %s",
		 dt->year, month, day, g_weekday_names[dt->weekday]);
}

static void format_uptime_string(uint64_t mono_ms, char *out, uint32_t cap)
{
	uint64_t total_sec = mono_ms / 1000ULL;
	uint64_t days = total_sec / 86400ULL;
	uint64_t hours = (total_sec / 3600ULL) % 24ULL;
	uint64_t minutes = (total_sec / 60ULL) % 60ULL;
	uint64_t seconds = total_sec % 60ULL;
	char hh[3];
	char mm[3];
	char ss[3];

	if (out == NULL || cap == 0U) {
		return;
	}

	format_two_digits(hh, (int)hours);
	format_two_digits(mm, (int)minutes);
	format_two_digits(ss, (int)seconds);

	if (days > 0ULL) {
		snprintf(out, cap, "%llud %sh %sm",
			 (unsigned long long)days, hh, mm);
		return;
	}
	if (hours > 0ULL) {
		snprintf(out, cap, "%sh %sm %ss", hh, mm, ss);
		return;
	}
	snprintf(out, cap, "%sm %ss", mm, ss);
}

static void format_binary_size(uint64_t bytes, char *out, uint32_t cap)
{
	uint64_t unit = 1ULL;
	const char *label = "B";

	if (out == NULL || cap == 0U) {
		return;
	}

	if (bytes >= (1024ULL * 1024ULL * 1024ULL)) {
		unit = 1024ULL * 1024ULL * 1024ULL;
		label = "GiB";
	} else if (bytes >= (1024ULL * 1024ULL)) {
		unit = 1024ULL * 1024ULL;
		label = "MiB";
	} else if (bytes >= 1024ULL) {
		unit = 1024ULL;
		label = "KiB";
	}

	if (unit == 1ULL) {
		snprintf(out, cap, "%llu %s", (unsigned long long)bytes, label);
	} else {
		uint64_t whole = bytes / unit;
		uint64_t tenths = ((bytes % unit) * 10ULL) / unit;
		snprintf(out, cap, "%llu.%llu %s",
			 (unsigned long long)whole,
			 (unsigned long long)tenths,
			 label);
	}
}

static void format_mac_string(uint64_t packed, char *out, uint32_t cap)
{
	uint8_t bytes[6] = {0};
	char parts[6][3];

	if (out == NULL || cap == 0U) {
		return;
	}
	if (packed == 0ULL) {
		snprintf(out, cap, "offline");
		return;
	}

	for (uint32_t i = 0; i < 6U; i++) {
		bytes[i] = (uint8_t)((packed >> (i * 8U)) & 0xFFU);
		format_hex_byte(parts[i], bytes[i]);
	}
	snprintf(out, cap, "%s:%s:%s:%s:%s:%s",
		 parts[0], parts[1], parts[2],
		 parts[3], parts[4], parts[5]);
}

static void set_value_label(lv_obj_t *obj, const char *text, uint32_t color)
{
	if (obj == NULL || text == NULL) {
		return;
	}
	lv_label_set_text(obj, text);
	lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
}

static void processmgr_set_notice(const char *text, uint32_t color)
{
	if (text == NULL || text[0] == '\0') {
		g_processmgr.notice_text[0] = '\0';
		g_processmgr.notice_color = monitor_theme()->dim;
		return;
	}

	strncpy(g_processmgr.notice_text, text, sizeof(g_processmgr.notice_text) - 1U);
	g_processmgr.notice_text[sizeof(g_processmgr.notice_text) - 1U] = '\0';
	g_processmgr.notice_color = color;
}

static void processmgr_update_selection_metric(void)
{
	char text[32] = {0};

	if (g_processmgr.has_selection && g_processmgr.selected_pid != 0U) {
		snprintf(text, sizeof(text), "%llu",
			 (unsigned long long)g_processmgr.selected_pid);
		set_value_label(g_selected_value, text, monitor_theme()->text);
		return;
	}

	set_value_label(g_selected_value, "--", monitor_theme()->dim);
}

static int32_t processmgr_find_selected_index(void)
{
	if (!g_processmgr.has_selection || g_processmgr.selected_pid == 0U) {
		return -1;
	}

	for (uint32_t i = 0; i < g_processmgr.processes.count; i++) {
		if (g_processmgr.processes.processes[i].pid == g_processmgr.selected_pid) {
			return (int32_t)i;
		}
	}
	return -1;
}

static void processmgr_sync_selection(void)
{
	if (!g_processmgr.has_selection) {
		return;
	}

	if (processmgr_find_selected_index() >= 0) {
		return;
	}

	g_processmgr.has_selection = 0;
	g_processmgr.selected_pid = 0;
	if (g_processmgr.notice_text[0] == '\0') {
		processmgr_set_notice("The selected process is no longer running.", monitor_theme()->warm);
	}
}

static void processmgr_update_terminate_button(void)
{
	const monitor_theme_s *theme = monitor_theme();
	uint8_t enabled = 0;

	if (g_terminate_btn == NULL || g_terminate_btn_label == NULL) {
		return;
	}

	enabled = g_processmgr.has_selection &&
		  g_processmgr.selected_pid != 0U &&
		  g_processmgr.selected_pid != g_processmgr.self_pid;
	if (enabled) {
		lv_obj_clear_state(g_terminate_btn, LV_STATE_DISABLED);
		lv_obj_set_style_text_color(g_terminate_btn_label, lv_color_hex(theme->panel), 0);
	} else {
		lv_obj_add_state(g_terminate_btn, LV_STATE_DISABLED);
		lv_obj_set_style_text_color(g_terminate_btn_label, lv_color_hex(theme->dim), 0);
	}
}

static void processmgr_update_process_labels(uint64_t proc_count, uint64_t thread_count)
{
	const monitor_theme_s *theme = monitor_theme();
	char summary[96] = {0};
	char detail[160] = {0};
	int32_t selected_index = processmgr_find_selected_index();
	uint32_t detail_color = theme->dim;

	snprintf(summary, sizeof(summary), "%llu processes online. %llu threads active.",
		 (unsigned long long)proc_count,
		 (unsigned long long)thread_count);
	set_value_label(g_process_summary, summary, theme->dim);

	if (g_processmgr.notice_text[0] != '\0') {
		set_value_label(g_process_detail, g_processmgr.notice_text, g_processmgr.notice_color);
		processmgr_update_terminate_button();
		processmgr_update_selection_metric();
		return;
	}

	if (selected_index < 0) {
		snprintf(detail, sizeof(detail),
			 "Select a process below to inspect it or end a stalled task.");
		set_value_label(g_process_detail, detail, theme->dim);
		processmgr_update_terminate_button();
		processmgr_update_selection_metric();
		return;
	}

	snprintf(detail, sizeof(detail), "Selected pid %llu. %s. %llu thread%s.",
		 (unsigned long long)g_processmgr.processes.processes[selected_index].pid,
		 g_processmgr.processes.processes[selected_index].name,
		 (unsigned long long)g_processmgr.processes.processes[selected_index].thread_count,
		 g_processmgr.processes.processes[selected_index].thread_count == 1U ? "" : "s");
	detail_color = theme->accent;
	if (g_processmgr.selected_pid == g_processmgr.self_pid) {
		snprintf(detail, sizeof(detail),
			 "Selected pid %llu. %s. Current app cannot terminate itself.",
			 (unsigned long long)g_processmgr.processes.processes[selected_index].pid,
			 g_processmgr.processes.processes[selected_index].name);
		detail_color = theme->warm;
	}
	set_value_label(g_process_detail, detail, detail_color);
	processmgr_update_terminate_button();
	processmgr_update_selection_metric();
}

static void processmgr_row_event_cb(lv_event_t *e)
{
	const systemd_process_info_s *target = NULL;

	if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
		return;
	}

	target = (const systemd_process_info_s *)lv_event_get_user_data(e);
	if (target == NULL) {
		return;
	}

	processmgr_mark_interacting();
	g_processmgr.selected_pid = target->pid;
	g_processmgr.has_selection = target->pid != 0U;
	processmgr_set_notice(NULL, monitor_theme()->dim);
	processmgr_update_selection_metric();
	processmgr_render_list();
	processmgr_update_process_labels(g_processmgr.last_proc_count, g_processmgr.last_thread_count);
}

static void processmgr_list_event_cb(lv_event_t *e)
{
	lv_event_code_t code = lv_event_get_code(e);

	if (code == LV_EVENT_PRESSED ||
	    code == LV_EVENT_PRESSING ||
	    code == LV_EVENT_RELEASED ||
	    code == LV_EVENT_SCROLL_BEGIN ||
	    code == LV_EVENT_SCROLL ||
	    code == LV_EVENT_SCROLL_END) {
		processmgr_mark_interacting();
	}
}

static void processmgr_render_list(void)
{
	const monitor_theme_s *theme = monitor_theme();
	lv_coord_t scroll_y = 0;

	if (g_process_list == NULL) {
		return;
	}

	scroll_y = lv_obj_get_scroll_y(g_process_list);
	lv_obj_clean(g_process_list);
	if (g_processmgr.processes.count == 0U) {
		lv_obj_t *empty = lv_label_create(g_process_list);

		lv_obj_set_width(empty, 808);
		lv_label_set_long_mode(empty, LV_LABEL_LONG_WRAP);
		lv_obj_set_style_text_font(empty, &lv_font_montserrat_16, 0);
		lv_obj_set_style_text_color(empty, lv_color_hex(theme->dim), 0);
		lv_label_set_text(empty, "No processes are visible through systemd right now.");
		if (scroll_y != 0) {
			lv_obj_scroll_to_y(g_process_list, scroll_y, LV_ANIM_OFF);
		}
		return;
	}

	for (uint32_t i = 0; i < g_processmgr.processes.count; i++) {
		systemd_process_info_s *info = &g_processmgr.processes.processes[i];
		lv_obj_t *row = lv_btn_create(g_process_list);
		lv_obj_t *badge = NULL;
		lv_obj_t *title = NULL;
		lv_obj_t *meta = NULL;
		char pid_text[24] = {0};
		char meta_text[128] = {0};
		uint8_t selected = g_processmgr.has_selection &&
				   info->pid == g_processmgr.selected_pid;

		lv_obj_set_size(row, 820, PROCESS_ROW_H);
		lv_obj_set_style_radius(row, 22, 0);
		lv_obj_set_style_bg_color(row, lv_color_hex(selected ? theme->accent_soft :
							 (info->pid == g_processmgr.self_pid ?
							  theme->warm_soft : theme->panel_alt)), 0);
		lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
		lv_obj_set_style_border_color(row, lv_color_hex(selected ? theme->accent : theme->line), 0);
		lv_obj_set_style_border_width(row, selected ? 2 : 1, 0);
		lv_obj_set_style_shadow_width(row, 0, 0);
		lv_obj_set_style_outline_width(row, 0, LV_STATE_FOCUSED);
		lv_obj_set_style_pad_all(row, 0, 0);
		lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_PRESS_LOCK);
		lv_obj_add_flag(row, LV_OBJ_FLAG_SCROLL_CHAIN_VER | LV_OBJ_FLAG_GESTURE_BUBBLE);
		lv_obj_add_event_cb(row, processmgr_row_interaction_event_cb, LV_EVENT_ALL, NULL);
		lv_obj_add_event_cb(row, processmgr_row_event_cb, LV_EVENT_CLICKED, info);

		snprintf(pid_text, sizeof(pid_text), "PID %llu", (unsigned long long)info->pid);
		badge = create_chip(row, 14, 10, pid_text,
				    info->pid == g_processmgr.self_pid ? theme->warm_soft : theme->accent_soft,
				    info->pid == g_processmgr.self_pid ? theme->warm : theme->accent);

		title = lv_label_create(row);
		lv_obj_set_pos(title, 106, 10);
		lv_obj_set_width(title, 680);
		lv_label_set_long_mode(title, LV_LABEL_LONG_WRAP);
		lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
		lv_obj_set_style_text_color(title, lv_color_hex(theme->text), 0);
		lv_label_set_text(title, info->name);

		meta = lv_label_create(row);
		lv_obj_set_pos(meta, 106, 30);
		lv_obj_set_width(meta, 680);
		lv_label_set_long_mode(meta, LV_LABEL_LONG_WRAP);
		lv_obj_set_style_text_font(meta, &lv_font_montserrat_14, 0);
		lv_obj_set_style_text_color(meta, lv_color_hex(theme->dim), 0);
		snprintf(meta_text, sizeof(meta_text), "%llu thread%s%s",
			 (unsigned long long)info->thread_count,
			 info->thread_count == 1U ? "" : "s",
			 info->pid == g_processmgr.self_pid ? "  -  current app" : "");
		lv_label_set_text(meta, meta_text);
		(void)badge;
	}
	if (scroll_y != 0) {
		lv_obj_scroll_to_y(g_process_list, scroll_y, LV_ANIM_OFF);
	}
}

static void processmgr_action_event_cb(lv_event_t *e)
{
	process_action_e action = PROCESS_ACTION_REFRESH;
	uint64_t mono_ms = OSSysCtrlGetMonoTime() / NSEC_PER_MSEC;
	char text[96] = {0};

	if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
		return;
	}

	action = (process_action_e)(uintptr_t)lv_event_get_user_data(e);
	if (action == PROCESS_ACTION_REFRESH) {
		processmgr_set_notice(NULL, monitor_theme()->dim);
		processmgr_refresh(mono_ms, 1U);
		return;
	}

	if (!g_processmgr.has_selection || g_processmgr.selected_pid == 0U ||
	    g_processmgr.selected_pid == g_processmgr.self_pid ||
	    g_systemd == NULL || g_systemd->ops.exit_process_by_pid == NULL) {
		return;
	}

	if (g_systemd->ops.exit_process_by_pid(g_systemd, g_processmgr.selected_pid) != 0U) {
		snprintf(text, sizeof(text), "Ended process pid %llu.",
			 (unsigned long long)g_processmgr.selected_pid);
		g_processmgr.has_selection = 0;
		g_processmgr.selected_pid = 0;
		processmgr_set_notice(text, monitor_theme()->accent);
		processmgr_refresh(mono_ms, 1U);
		return;
	}

	snprintf(text, sizeof(text), "Failed to end pid %llu.",
		 (unsigned long long)g_processmgr.selected_pid);
	processmgr_set_notice(text, monitor_theme()->alert);
	processmgr_update_process_labels(g_processmgr.last_proc_count, g_processmgr.last_thread_count);
}

static void create_platform_card(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
				 const char *title, lv_obj_t **value_out,
				 const lv_font_t *value_font)
{
	const monitor_theme_s *theme = monitor_theme();
	lv_obj_t *card = create_panel(parent, x, y, PLATFORM_CARD_W, PLATFORM_CARD_H,
				      theme->panel_alt, theme->line);
	lv_obj_t *title_lbl = lv_label_create(card);
	lv_obj_t *value_lbl = lv_label_create(card);

	lv_obj_set_pos(title_lbl, 16, 12);
	lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(title_lbl, lv_color_hex(theme->dim), 0);
	lv_label_set_text(title_lbl, title);

	lv_obj_set_pos(value_lbl, 16, 34);
	lv_obj_set_width(value_lbl, PLATFORM_CARD_W - 32);
	lv_label_set_long_mode(value_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(value_lbl, value_font, 0);
	lv_obj_set_style_text_color(value_lbl, lv_color_hex(theme->text), 0);
	lv_label_set_text(value_lbl, "--");

	if (value_out != NULL) {
		*value_out = value_lbl;
	}
}

static void create_metric_value(lv_obj_t *parent, lv_coord_t x, const char *title,
				lv_obj_t **value_out)
{
	const monitor_theme_s *theme = monitor_theme();
	lv_obj_t *title_lbl = lv_label_create(parent);
	lv_obj_t *value_lbl = lv_label_create(parent);

	lv_obj_set_pos(title_lbl, x, 16);
	lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(title_lbl, lv_color_hex(theme->dim), 0);
	lv_label_set_text(title_lbl, title);

	lv_obj_set_pos(value_lbl, x, 40);
	lv_obj_set_style_text_font(value_lbl, &lv_font_montserrat_24, 0);
	lv_obj_set_style_text_color(value_lbl, lv_color_hex(theme->text), 0);
	lv_label_set_text(value_lbl, "--");

	if (value_out != NULL) {
		*value_out = value_lbl;
	}
}

static void create_ui(void)
{
	const monitor_theme_s *theme = monitor_theme();
	lv_obj_t *scr = lv_scr_act();
	lv_obj_t *panel = NULL;
	lv_obj_t *hero = NULL;
	lv_obj_t *platform = NULL;
	lv_obj_t *memory = NULL;
	lv_obj_t *process_panel = NULL;
	lv_obj_t *label = NULL;
	lv_obj_t *bar = NULL;
	lv_obj_t *refresh_btn = NULL;

	lv_obj_set_style_bg_color(scr, lv_color_hex(theme->bg), 0);
	lv_obj_set_style_bg_grad_color(scr, lv_color_hex(theme->bg_alt), 0);
	lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, 0);
	lv_obj_set_style_border_width(scr, 0, 0);
	lv_obj_set_style_pad_all(scr, 0, 0);
	clear_static_flags(scr);

	panel = create_panel(scr, CONTENT_X, CONTENT_Y, CONTENT_W, CONTENT_H, theme->panel, theme->line);

	create_chip(panel, PANEL_INSET, 28, "PROCESS", theme->accent_soft, theme->accent);

	label = lv_label_create(panel);
	lv_obj_set_pos(label, PANEL_INSET, 56);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_48, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->text), 0);
	lv_label_set_text(label, "Process manager");

	label = lv_label_create(panel);
	lv_obj_set_pos(label, PANEL_INSET, 118);
	lv_obj_set_width(label, CONTENT_W - (PANEL_INSET * 2));
	lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->dim), 0);
	lv_label_set_text(label, "Inspect live processes, watch runtime pressure, and end stalled tasks.");

	hero = create_panel(panel, PANEL_INSET, HERO_Y, LEFT_COL_W, HERO_H, theme->panel_alt, theme->line);
	label = lv_label_create(hero);
	lv_obj_set_pos(label, 24, 18);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->dim), 0);
	lv_label_set_text(label, "UPTIME");

	g_uptime_value = lv_label_create(hero);
	lv_obj_set_pos(g_uptime_value, 24, 44);
	lv_obj_set_width(g_uptime_value, LEFT_COL_W - 48);
	lv_label_set_long_mode(g_uptime_value, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(g_uptime_value, &lv_font_montserrat_32, 0);
	lv_obj_set_style_text_color(g_uptime_value, lv_color_hex(theme->text), 0);
	lv_label_set_text(g_uptime_value, "--");

	g_clock_value = lv_label_create(hero);
	lv_obj_set_pos(g_clock_value, 24, 92);
	lv_obj_set_style_text_font(g_clock_value, &lv_font_montserrat_32, 0);
	lv_obj_set_style_text_color(g_clock_value, lv_color_hex(theme->accent), 0);
	lv_label_set_text(g_clock_value, "--:--:--");

	g_date_value = lv_label_create(hero);
	lv_obj_set_pos(g_date_value, 24, 128);
	lv_obj_set_style_text_font(g_date_value, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(g_date_value, lv_color_hex(theme->dim), 0);
	lv_label_set_text(g_date_value, "----");

	platform = create_panel(panel, PANEL_INSET + LEFT_COL_W + COLUMN_GAP, HERO_Y,
			       RIGHT_COL_W, HERO_H, theme->panel, theme->line);
	create_platform_card(platform, 24, 18, "Current CPU", &g_cpu_value, &lv_font_montserrat_24);
	create_platform_card(platform, 24 + PLATFORM_CARD_W + 12, 18, "Core count",
			    &g_core_value, &lv_font_montserrat_24);
	create_platform_card(platform, 24, 18 + PLATFORM_CARD_H + 10, "Storage",
			    &g_storage_value, &lv_font_montserrat_24);
	create_platform_card(platform, 24 + PLATFORM_CARD_W + 12, 18 + PLATFORM_CARD_H + 10,
			    "Network", &g_network_value, &lv_font_montserrat_16);

	memory = create_panel(panel, PANEL_INSET, MEMORY_Y, CONTENT_W - (PANEL_INSET * 2),
			      MEMORY_H, theme->panel, theme->line);
	label = lv_label_create(memory);
	lv_obj_set_pos(label, 24, 12);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->dim), 0);
	lv_label_set_text(label, "MEMORY");

	g_memory_value = lv_label_create(memory);
	lv_obj_set_pos(g_memory_value, 24, 28);
	lv_obj_set_style_text_font(g_memory_value, &lv_font_montserrat_24, 0);
	lv_obj_set_style_text_color(g_memory_value, lv_color_hex(theme->text), 0);
	lv_label_set_text(g_memory_value, "--");

	bar = lv_bar_create(memory);
	lv_obj_set_pos(bar, 24, 56);
	lv_obj_set_size(bar, 404, 12);
	lv_bar_set_range(bar, 0, 100);
	lv_obj_set_style_bg_color(bar, lv_color_hex(theme->panel_alt), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_radius(bar, 999, LV_PART_MAIN);
	lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
	lv_obj_set_style_bg_color(bar, lv_color_hex(theme->accent), LV_PART_INDICATOR);
	lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
	lv_obj_set_style_radius(bar, 999, LV_PART_INDICATOR);
	lv_obj_set_style_border_width(bar, 0, LV_PART_INDICATOR);
	clear_static_flags(bar);
	g_memory_bar = bar;

	g_memory_meta = lv_label_create(memory);
	lv_obj_set_pos(g_memory_meta, 24, 72);
	lv_obj_set_width(g_memory_meta, 430);
	lv_label_set_long_mode(g_memory_meta, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(g_memory_meta, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(g_memory_meta, lv_color_hex(theme->dim), 0);
	lv_label_set_text(g_memory_meta, "--");

	create_metric_value(memory, 520, "Processes", &g_proc_value);
	create_metric_value(memory, 648, "Threads", &g_thread_value);
	create_metric_value(memory, 772, "Selected PID", &g_selected_value);

	process_panel = create_panel(panel, PANEL_INSET, PROCESS_Y, CONTENT_W - (PANEL_INSET * 2),
				     PROCESS_H, theme->panel, theme->line);
	label = lv_label_create(process_panel);
	lv_obj_set_pos(label, 24, 16);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->text), 0);
	lv_label_set_text(label, "Processes");

	g_process_summary = lv_label_create(process_panel);
	lv_obj_set_pos(g_process_summary, 24, 44);
	lv_obj_set_width(g_process_summary, 540);
	lv_label_set_long_mode(g_process_summary, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(g_process_summary, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(g_process_summary, lv_color_hex(theme->dim), 0);
	lv_label_set_text(g_process_summary, "Collecting runtime state...");

	g_process_detail = lv_label_create(process_panel);
	lv_obj_set_pos(g_process_detail, 24, 60);
	lv_obj_set_width(g_process_detail, 540);
	lv_label_set_long_mode(g_process_detail, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(g_process_detail, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(g_process_detail, lv_color_hex(theme->dim), 0);
	lv_label_set_text(g_process_detail, "Select a process below to inspect it or end a stalled task.");

	refresh_btn = create_action_button(process_panel, 628, 16, PROCESS_ACTION_REFRESH_W, "Refresh",
					      theme->panel_alt, theme->line, theme->text,
					      PROCESS_ACTION_REFRESH, NULL);
	g_terminate_btn = create_action_button(process_panel, 744, 16, PROCESS_ACTION_END_W, "End Process",
					       theme->alert, theme->alert, theme->panel,
					       PROCESS_ACTION_TERMINATE, &g_terminate_btn_label);
	lv_obj_add_event_cb(refresh_btn, processmgr_action_event_cb, LV_EVENT_CLICKED,
			    (void *)(uintptr_t)PROCESS_ACTION_REFRESH);
	lv_obj_add_event_cb(g_terminate_btn, processmgr_action_event_cb, LV_EVENT_CLICKED,
			    (void *)(uintptr_t)PROCESS_ACTION_TERMINATE);

	g_process_list = lv_obj_create(process_panel);
	lv_obj_set_pos(g_process_list, 20, PROCESS_LIST_Y);
	lv_obj_set_size(g_process_list, 848, PROCESS_LIST_H);
	lv_obj_set_style_bg_color(g_process_list, lv_color_hex(theme->panel), 0);
	lv_obj_set_style_bg_opa(g_process_list, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(g_process_list, 0, 0);
	lv_obj_set_style_radius(g_process_list, 0, 0);
	lv_obj_set_style_pad_all(g_process_list, 0, 0);
	lv_obj_set_style_pad_row(g_process_list, 8, 0);
	lv_obj_set_style_pad_column(g_process_list, 0, 0);
	lv_obj_add_flag(g_process_list, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_scroll_dir(g_process_list, LV_DIR_VER);
	lv_obj_set_scrollbar_mode(g_process_list, LV_SCROLLBAR_MODE_AUTO);
	lv_obj_set_layout(g_process_list, LV_LAYOUT_FLEX);
	lv_obj_set_flex_flow(g_process_list, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_flex_align(g_process_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
			      LV_FLEX_ALIGN_START);
	lv_obj_add_event_cb(g_process_list, processmgr_list_event_cb, LV_EVENT_ALL, NULL);

	processmgr_update_selection_metric();
	processmgr_update_terminate_button();
}

static void refresh_clock_and_uptime(uint64_t mono_ms)
{
	const monitor_theme_s *theme = monitor_theme();
	monitor_datetime_s dt = {0};
	uint64_t ts = normalize_timestamp_seconds(OSSysCtrlGetTimestamp(), mono_ms);
	char text[64] = {0};

	format_uptime_string(mono_ms, text, sizeof(text));
	set_value_label(g_uptime_value, text, theme->text);

	timestamp_to_local_datetime(ts, &dt);
	format_time_string(&dt, text, sizeof(text));
	set_value_label(g_clock_value, text, theme->accent);
	format_date_string(&dt, text, sizeof(text));
	set_value_label(g_date_value, text, theme->dim);
}

static void refresh_platform_metrics(void)
{
	const monitor_theme_s *theme = monitor_theme();
	char text[64] = {0};
	uint64_t cpu_id = OSSysCtrlGetCpuId();
	uint64_t cpu_nr = OSSysCtrlGetCpuNr();
	uint64_t packed_mac = 0;
	uint64_t storage_bytes = 0;

	snprintf(text, sizeof(text), "CPU %llu", (unsigned long long)cpu_id);
	set_value_label(g_cpu_value, text, theme->text);

	snprintf(text, sizeof(text), "%llu online", (unsigned long long)(cpu_nr == 0U ? 1U : cpu_nr));
	set_value_label(g_core_value, text, theme->text);

	if (g_devmgr != NULL && g_devmgr->ops.blk_get_capacity != NULL) {
		storage_bytes = g_devmgr->ops.blk_get_capacity(g_devmgr) * STORAGE_SECTOR_SIZE;
	}
	if (storage_bytes == 0U) {
		set_value_label(g_storage_value, "unavailable", theme->dim);
	} else {
		format_binary_size(storage_bytes, text, sizeof(text));
		set_value_label(g_storage_value, text, theme->text);
	}

	if (g_net != NULL && g_net->ops.get_mac != NULL && g_net->net_pool_cref != 0U) {
		packed_mac = g_net->ops.get_mac(g_net);
	}
	format_mac_string(packed_mac, text, sizeof(text));
	set_value_label(g_network_value, text, packed_mac == 0U ? theme->dim : theme->text);
}

static void refresh_memory_and_processes(void)
{
	const monitor_theme_s *theme = monitor_theme();
	char text[128] = {0};
	uint64_t mem_total = 0;
	uint64_t mem_free = 0;
	uint64_t mem_used = 0;
	uint64_t mem_total_mib = 0;
	uint64_t mem_free_mib = 0;
	uint64_t mem_used_mib = 0;
	uint64_t proc_count = 0;
	uint64_t thread_count = 0;
	uint32_t mem_percent = 0;

	if (g_systemd != NULL) {
		if (g_systemd->ops.get_process_pid_by_path != NULL) {
			g_processmgr.self_pid =
				g_systemd->ops.get_process_pid_by_path(g_systemd, MONITOR_PROCESS_PATH);
		}
		if (g_systemd->ops.get_mem_total != NULL) {
			mem_total = g_systemd->ops.get_mem_total(g_systemd);
		}
		if (g_systemd->ops.get_mem_free != NULL) {
			mem_free = g_systemd->ops.get_mem_free(g_systemd);
		}
		if (g_systemd->ops.get_proc_count != NULL) {
			proc_count = g_systemd->ops.get_proc_count(g_systemd);
		}
		if (g_systemd->ops.get_thread_count != NULL) {
			thread_count = g_systemd->ops.get_thread_count(g_systemd);
		}
		if (g_systemd->ops.list_processes != NULL) {
			memset(&g_processmgr.processes, 0, sizeof(g_processmgr.processes));
			if (g_systemd->ops.list_processes(g_systemd, &g_processmgr.processes) == 0U) {
				processmgr_set_notice("Process list is unavailable from systemd.", theme->alert);
			}
		}
	}
	g_processmgr.last_proc_count = proc_count;
	g_processmgr.last_thread_count = thread_count;

	if (mem_total >= mem_free) {
		mem_used = mem_total - mem_free;
	}
	mem_total_mib = mem_total / (1024ULL * 1024ULL);
	mem_free_mib = mem_free / (1024ULL * 1024ULL);
	mem_used_mib = mem_used / (1024ULL * 1024ULL);
	if (mem_total != 0U) {
		mem_percent = (uint32_t)((mem_used * 100ULL) / mem_total);
		if (mem_percent > 100U) {
			mem_percent = 100U;
		}
	}

	if (mem_total == 0U) {
		set_value_label(g_memory_value, "Memory offline", theme->dim);
		set_value_label(g_memory_meta, "systemd metrics are unavailable", theme->dim);
		lv_bar_set_value(g_memory_bar, 0, LV_ANIM_OFF);
	} else {
		snprintf(text, sizeof(text), "%llu / %llu MiB",
			 (unsigned long long)mem_used_mib,
			 (unsigned long long)mem_total_mib);
		set_value_label(g_memory_value, text, theme->text);
		snprintf(text, sizeof(text), "%llu MiB free  %u%% used",
			 (unsigned long long)mem_free_mib, (unsigned int)mem_percent);
		set_value_label(g_memory_meta, text, theme->dim);
		lv_bar_set_value(g_memory_bar, (int16_t)mem_percent, LV_ANIM_OFF);
	}

	snprintf(text, sizeof(text), "%llu", (unsigned long long)proc_count);
	set_value_label(g_proc_value, text, proc_count == 0U ? theme->dim : theme->text);

	snprintf(text, sizeof(text), "%llu", (unsigned long long)thread_count);
	set_value_label(g_thread_value, text, thread_count == 0U ? theme->dim : theme->text);

	processmgr_sync_selection();
	processmgr_update_selection_metric();
	processmgr_render_list();
	processmgr_update_process_labels(proc_count, thread_count);
}

static void processmgr_refresh(uint64_t mono_ms, uint8_t force)
{
	if (!force && mono_ms < g_processmgr.interaction_until_ms) {
		return;
	}

	if (!force && mono_ms - g_processmgr.last_refresh_ms < REFRESH_INTERVAL_MS) {
		return;
	}

	g_processmgr.last_refresh_ms = mono_ms;
	refresh_clock_and_uptime(mono_ms);
	refresh_platform_metrics();
	refresh_memory_and_processes();
}

static void monitor_on_create(app_s *app)
{
	(void)app;

	g_systemd = systemd_client_get();
	g_devmgr = devmgr_client_get();
	g_net = net_client_get();
	(void)monitor_read_theme_mode(&g_theme_mode, &g_last_theme_revision);
	g_processmgr.notice_color = monitor_theme()->dim;

	create_ui();
	processmgr_refresh(OSSysCtrlGetMonoTime() / NSEC_PER_MSEC, 1U);
	log_info("monitor ready\n");
}

static void monitor_on_update(app_s *app, uint64_t mono_ms)
{
	(void)app;
	monitor_refresh_theme(mono_ms, 0U);
	processmgr_refresh(mono_ms, 0U);
}

int main(int argc, char *argv[])
{
	app_s app = {0};
	app_config_s config = {
		.name = "monitor",
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

	log_info("monitor start!\n");
	lifecycle.on_create = monitor_on_create;
	lifecycle.on_update = monitor_on_update;
	app_init(&app, &config, &lifecycle, NULL);
	return app_run(&app);
}
