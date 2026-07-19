#include "lvgl.h"
#include "app.h"
#include "liblvgl/lv_port_disp.h"
#include "liblvgl/lv_port_indev.h"
#include "libwindow/window.h"
#include "log.h"
#include "libkernel/capcall.h"
#include "libhttps/https_client.h"
#include "libsystem/fs_client.h"
#include "libsystem/ipc.h"
#include "libsystem/statemgr_client.h"
#include "libsystem/systemd_client.h"
#include "string.h"
#include "stddef.h"
#include "stdint.h"

#define SYSTEM_OVERLAY_GAP 24
#define CHAT_CARD_X 40
#define CHAT_CARD_Y SYSTEM_OVERLAY_GAP
#define CHAT_CARD_WIDTH 944
#define CHAT_CARD_HEIGHT (APP_DEFAULT_HEIGHT - CHAT_CARD_Y - SYSTEM_OVERLAY_GAP)
#define CHAT_CARD_INSET 28
#define CHAT_SECTION_GAP 24
#define CHAT_LEFT_COL_WIDTH 296
#define CHAT_RIGHT_COL_WIDTH 568
#define CHAT_TOP_ROW_Y 74
#define CHAT_CORE_HEIGHT 430
#define CHAT_QUEUE_Y CHAT_TOP_ROW_Y
#define CHAT_QUEUE_HEIGHT CHAT_THREAD_HEIGHT
#define CHAT_RIGHT_X (CHAT_CARD_INSET + CHAT_LEFT_COL_WIDTH + CHAT_SECTION_GAP)
#define CHAT_THREAD_HEIGHT (CHAT_CARD_HEIGHT - CHAT_TOP_ROW_Y - CHAT_CARD_INSET)
#define CHAT_THREAD_INSET 20
#define CHAT_THREAD_HEADER_Y 24
#define CHAT_HISTORY_Y 86
#define CHAT_HISTORY_WIDTH (CHAT_RIGHT_COL_WIDTH - CHAT_THREAD_INSET * 2)
#define CHAT_QUEUE_STATUS_Y 54
#define CHAT_QUEUE_ROW_Y 96
#define CHAT_QUEUE_ROW_GAP 78
#define CHAT_INPUT_INSET 12
#define CHAT_INPUT_GAP 12
#define CHAT_SEND_WIDTH 128
#define CHAT_INPUT_FIELD_WIDTH (CHAT_HISTORY_WIDTH - CHAT_INPUT_INSET * 2 - CHAT_INPUT_GAP - CHAT_SEND_WIDTH)
#define CHAT_INPUT_HEIGHT 72
#define CHAT_INPUT_Y (CHAT_THREAD_HEIGHT - CHAT_THREAD_INSET - CHAT_INPUT_HEIGHT)
#define CHAT_HISTORY_HEIGHT (CHAT_INPUT_Y - CHAT_HISTORY_Y - 18)
#define CHAT_LOOP_COUNT 3U
#define CHAT_AGENT_NODE_COUNT 4U
#define TIMEZONE_OFFSET_HOURS 8
#define INTERACTIVE_MAX 12U
#define KIMI_INITIAL_DELAY_MS 8000ULL
#define KIMI_RETRY_INTERVAL_MS 5000ULL
#define CHAT_TRANSCRIPT_CAP 1536U
#define CHAT_INPUT_CAP 120U
#define CHAT_REQUEST_BODY_CAP 512U
#define MOONSHOT_AUTH_CAP 192U
#define MOONSHOT_API_KEY_PATH "/root/system/apps/ai/moonshot.txt"
#define THEME_REFRESH_INTERVAL_MS 100ULL

#define COLOR_BG          0xfff4ee
#define COLOR_BG_GRAD     0xeaf4ff
#define COLOR_PANEL       0xfffffb
#define COLOR_PANEL_ALT   0xfff1eb
#define COLOR_PANEL_SOFT  0xeaf7f1
#define COLOR_ACCENT      0xff7d5c
#define COLOR_ACCENT_ALT  0x62c7b1
#define COLOR_WARM        0xffb85e
#define COLOR_ALERT       0xf08d80
#define COLOR_SUCCESS     0x51c89a
#define COLOR_TEXT        0x24324a
#define COLOR_DIM         0x7d8198
#define COLOR_LINE        0xf0ddd2

typedef struct {
	int year;
	int month;
	int day;
	int weekday;
	int hour;
	int minute;
	int second;
} ai_datetime_s;

typedef enum ai_theme_mode {
	AI_THEME_LIGHT = 0,
	AI_THEME_DARK = 1,
} ai_theme_mode_e;

typedef struct ai_theme {
	uint32_t bg;
	uint32_t bg_grad;
	uint32_t panel;
	uint32_t panel_alt;
	uint32_t panel_soft;
	uint32_t accent;
	uint32_t accent_alt;
	uint32_t warm;
	uint32_t alert;
	uint32_t success;
	uint32_t text;
	uint32_t dim;
	uint32_t line;
	uint32_t chip_text;
} ai_theme_s;

LV_FONT_DECLARE(systemui_font_cn_16);
LV_FONT_DECLARE(systemui_font_cn_32);

#define FONT_UI (&systemui_font_cn_16)
#define FONT_TITLE (&lv_font_montserrat_32)
#define FONT_SECTION (&lv_font_montserrat_24)
#define FONT_ASCII_UI (&lv_font_montserrat_16)
#define FONT_TIME (&lv_font_montserrat_48)

#define KIMI_URL "https://api.moonshot.cn/v1/chat/completions"
#define KIMI_EXPECTED_REPLY "KIMI OK"
#define KIMI_CHAT_SYSTEM_PROMPT \
	"Reply with plain ASCII only. Behave like a concise resident agent. Restate the goal briefly, " \
	"give the next action or plan, and mention blockers if any."
#define KIMI_REQUEST_BODY \
	"{\"model\":\"moonshot-v1-8k\",\"messages\":[{\"role\":\"system\",\"content\":\"Reply with plain ASCII only.\"}," \
	"{\"role\":\"user\",\"content\":\"Reply exactly: KIMI OK\"}],\"temperature\":0.1}"

typedef enum {
	KIMI_STATE_WAITING_NET = 0,
	KIMI_STATE_CONNECTING,
	KIMI_STATE_READY,
	KIMI_STATE_ERROR,
} kimi_state_e;

static lv_obj_t *chat_badge_lbl = NULL;
static lv_obj_t *chat_title_lbl = NULL;
static lv_obj_t *chat_trace_lbl = NULL;
static lv_obj_t *chat_status_lbl = NULL;
static lv_obj_t *chat_history_panel = NULL;
static lv_obj_t *chat_history_lbl = NULL;
static lv_obj_t *chat_history_hint_lbl = NULL;
static lv_obj_t *chat_input_panel = NULL;
static lv_obj_t *chat_input_ta = NULL;
static lv_obj_t *chat_send_btn = NULL;
static lv_obj_t *chat_send_btn_label = NULL;
static lv_obj_t *chat_loop_status[CHAT_LOOP_COUNT] = {0};
static lv_obj_t *chat_loop_title[CHAT_LOOP_COUNT] = {0};
static lv_obj_t *chat_loop_body[CHAT_LOOP_COUNT] = {0};
static lv_obj_t *chat_agent_visual = NULL;
static lv_obj_t *chat_agent_ring_outer = NULL;
static lv_obj_t *chat_agent_ring_inner = NULL;
static lv_obj_t *chat_agent_core = NULL;
static lv_obj_t *chat_agent_iris = NULL;
static lv_obj_t *chat_agent_nodes[CHAT_AGENT_NODE_COUNT] = {0};

static lv_obj_t *g_interactive_objs[INTERACTIVE_MAX] = {0};
static uint32_t g_interactive_count = 0;

static https_client_s *g_https = NULL;
static statemgr_client_s *g_statemgr = NULL;
static uint32_t g_theme_mode = AI_THEME_LIGHT;
static uint64_t g_last_theme_ms = 0;
static uint64_t g_last_theme_revision = 0;

static uint8_t g_kimi_probe_done = 0;
static uint32_t g_kimi_status = 0;
static uint32_t g_chat_user_turns = 0;
static lv_obj_t *g_hover_obj = NULL;
static uint8_t g_kimi_state = KIMI_STATE_WAITING_NET;
static uint64_t g_kimi_last_attempt_ms = 0;
static uint8_t g_moonshot_auth_loaded = 0;
static char g_moonshot_auth_header[MOONSHOT_AUTH_CAP] = "";
static char g_chat_input_draft[CHAT_INPUT_CAP + 1] = "";
static char g_kimi_reply[96] = "waiting for first request";
static char g_kimi_trace[96] = "warming after boot";
static char g_chat_transcript[CHAT_TRANSCRIPT_CAP] =
	"SYSTEM\nResident agent is booting.\nWaiting for model access.\n\n";

static const ai_theme_s g_ai_theme_light = {
	.bg = COLOR_BG,
	.bg_grad = COLOR_BG_GRAD,
	.panel = COLOR_PANEL,
	.panel_alt = COLOR_PANEL_ALT,
	.panel_soft = COLOR_PANEL_SOFT,
	.accent = COLOR_ACCENT,
	.accent_alt = COLOR_ACCENT_ALT,
	.warm = COLOR_WARM,
	.alert = COLOR_ALERT,
	.success = COLOR_SUCCESS,
	.text = COLOR_TEXT,
	.dim = COLOR_DIM,
	.line = COLOR_LINE,
	.chip_text = 0xfffffb,
};

static const ai_theme_s g_ai_theme_dark = {
	.bg = 0x171828,
	.bg_grad = 0x22253c,
	.panel = 0x20243a,
	.panel_alt = 0x2a3048,
	.panel_soft = 0x253948,
	.accent = 0xffa07c,
	.accent_alt = 0x6fd7c0,
	.warm = 0xf6c86f,
	.alert = 0xf3a096,
	.success = 0x68d7ad,
	.text = 0xfff6ef,
	.dim = 0xc7b7b0,
	.line = 0x46506f,
	.chip_text = 0x20243a,
};

static const char *weekday_names[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};

static const ai_theme_s *ai_theme(void)
{
	return g_theme_mode == AI_THEME_DARK ? &g_ai_theme_dark : &g_ai_theme_light;
}

static void format_two_digits(char *dst, int value)
{
	dst[0] = (char)('0' + ((value / 10) % 10));
	dst[1] = (char)('0' + (value % 10));
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

static void timestamp_to_local_datetime(uint64_t ts_sec, ai_datetime_s *out)
{
	int64_t local_sec = (int64_t)ts_sec + (TIMEZONE_OFFSET_HOURS * 3600);
	int64_t days = local_sec / 86400;
	int64_t sec_of_day = local_sec % 86400;

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

static void set_panel_style(lv_obj_t *obj, uint32_t bg_color, lv_opa_t bg_opa,
			    uint32_t border_color, lv_opa_t border_opa, lv_coord_t radius)
{
	lv_obj_set_style_bg_color(obj, lv_color_hex(bg_color), 0);
	lv_obj_set_style_bg_opa(obj, bg_opa, 0);
	lv_obj_set_style_border_color(obj, lv_color_hex(border_color), 0);
	lv_obj_set_style_border_opa(obj, border_opa, 0);
	lv_obj_set_style_border_width(obj, 0, 0);
	lv_obj_set_style_radius(obj, radius, 0);
	lv_obj_set_style_pad_all(obj, 0, 0);
	lv_obj_set_style_shadow_width(obj, 18, 0);
	lv_obj_set_style_shadow_color(obj, lv_color_hex(border_color), 0);
	lv_obj_set_style_shadow_opa(obj, 28, 0);
	lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

static void set_interactive_style(lv_obj_t *obj)
{
	const ai_theme_s *theme = ai_theme();

	lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_set_style_border_color(obj, lv_color_hex(theme->accent), LV_STATE_HOVERED);
	lv_obj_set_style_border_width(obj, 2, LV_STATE_HOVERED);
	lv_obj_set_style_border_opa(obj, LV_OPA_100, LV_STATE_HOVERED);
	lv_obj_set_style_translate_y(obj, -6, LV_STATE_HOVERED);
	lv_obj_set_style_bg_opa(obj, 242, LV_STATE_HOVERED);
	lv_obj_set_style_border_color(obj, lv_color_hex(theme->accent), LV_STATE_PRESSED);
	lv_obj_set_style_border_width(obj, 2, LV_STATE_PRESSED);
	lv_obj_set_style_border_opa(obj, LV_OPA_100, LV_STATE_PRESSED);
	lv_obj_set_style_translate_y(obj, -2, LV_STATE_PRESSED);
}

static lv_obj_t *create_panel(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
			      lv_coord_t width, lv_coord_t height, uint32_t bg_color,
			      lv_opa_t bg_opa, uint32_t border_color, lv_opa_t border_opa,
			      lv_coord_t radius)
{
	lv_obj_t *panel = lv_obj_create(parent);
	lv_obj_set_pos(panel, x, y);
	lv_obj_set_size(panel, width, height);
	set_panel_style(panel, bg_color, bg_opa, border_color, border_opa, radius);
	return panel;
}

static lv_obj_t *create_label(lv_obj_t *parent, const char *text, const lv_font_t *font,
			      uint32_t color, lv_coord_t x, lv_coord_t y, lv_coord_t width)
{
	lv_obj_t *label = lv_label_create(parent);
	lv_label_set_text(label, text);
	if (width > 0) {
		lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
		lv_obj_set_width(label, width);
	}
	lv_obj_set_style_text_font(label, font, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
	lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, 0);
	lv_obj_set_pos(label, x, y);
	return label;
}

static lv_obj_t *create_chip_label(lv_obj_t *parent, const char *text,
				   const lv_font_t *font, uint32_t bg_color,
				   lv_opa_t bg_opa, uint32_t text_color,
				   lv_coord_t x, lv_coord_t y,
				   lv_coord_t pad_x, lv_coord_t pad_y)
{
	lv_obj_t *chip = lv_label_create(parent);
	lv_label_set_text(chip, text);
	lv_obj_set_style_text_font(chip, font, 0);
	lv_obj_set_style_text_color(chip, lv_color_hex(text_color), 0);
	lv_obj_set_style_bg_color(chip, lv_color_hex(bg_color), 0);
	lv_obj_set_style_bg_opa(chip, bg_opa, 0);
	lv_obj_set_style_radius(chip, 999, 0);
	lv_obj_set_style_pad_left(chip, pad_x, 0);
	lv_obj_set_style_pad_right(chip, pad_x, 0);
	lv_obj_set_style_pad_top(chip, pad_y, 0);
	lv_obj_set_style_pad_bottom(chip, pad_y, 0);
	lv_obj_set_pos(chip, x, y);
	return chip;
}

static lv_obj_t *create_button(lv_obj_t *parent, const char *text,
			       lv_coord_t x, lv_coord_t y, lv_coord_t width,
			       lv_coord_t height)
{
	const ai_theme_s *theme = ai_theme();
	lv_obj_t *btn = create_panel(parent, x, y, width, height, theme->accent,
				     LV_OPA_COVER, theme->accent, LV_OPA_COVER, 24);
	set_interactive_style(btn);

	lv_obj_t *label = lv_label_create(btn);
	lv_label_set_text(label, text);
	if (width > 24) {
		lv_obj_set_width(label, width - 24);
		lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
	}
	lv_obj_set_style_text_font(label, FONT_SECTION, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->chip_text), 0);
	lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_center(label);
	chat_send_btn_label = label;
	return btn;
}

static lv_coord_t wave_offset(uint64_t mono_ms, uint32_t period_ms,
			      lv_coord_t amplitude, uint32_t phase_ms)
{
	uint32_t cycle = period_ms == 0U ? 1U : period_ms;
	uint32_t half = cycle / 2U;
	uint32_t local = (uint32_t)((mono_ms + phase_ms) % cycle);
	int32_t value;

	if (half == 0U || amplitude == 0) {
		return 0;
	}
	if (local < half) {
		value = ((int32_t)local * (int32_t)amplitude * 2) / (int32_t)half;
		value -= (int32_t)amplitude;
	} else {
		value = ((int32_t)(cycle - local) * (int32_t)amplitude * 2) / (int32_t)half;
		value -= (int32_t)amplitude;
	}
	return (lv_coord_t)value;
}

static lv_opa_t wave_opa(uint64_t mono_ms, uint32_t period_ms,
			 lv_opa_t low, lv_opa_t high, uint32_t phase_ms)
{
	uint32_t cycle = period_ms == 0U ? 1U : period_ms;
	uint32_t half = cycle / 2U;
	uint32_t local = (uint32_t)((mono_ms + phase_ms) % cycle);
	uint32_t slope;

	if (half == 0U || high <= low) {
		return low;
	}
	slope = local < half ? local : (cycle - local);
	return (lv_opa_t)(low + ((uint32_t)(high - low) * slope) / half);
}

static void create_loop_row(lv_obj_t *parent, uint32_t index, lv_coord_t y)
{
	const ai_theme_s *theme = ai_theme();
	lv_obj_t *row = NULL;
	lv_obj_t *dot = NULL;

	if (index >= CHAT_LOOP_COUNT) {
		return;
	}

	row = create_panel(parent, 16, y, 264, 56, theme->panel_alt, 220,
			   theme->panel_alt, LV_OPA_COVER, 18);
	lv_obj_set_style_shadow_width(row, 0, 0);
	dot = lv_obj_create(row);
	lv_obj_set_pos(dot, 14, 16);
	lv_obj_set_size(dot, 8, 8);
	lv_obj_set_style_bg_color(dot, lv_color_hex(theme->accent_alt), 0);
	lv_obj_set_style_bg_opa(dot, LV_OPA_80, 0);
	lv_obj_set_style_border_width(dot, 0, 0);
	lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_shadow_width(dot, 0, 0);
	lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CLICK_FOCUSABLE);
	chat_loop_title[index] = create_label(row, "Observe", FONT_ASCII_UI, theme->text, 30, 8, 142);
	chat_loop_body[index] = create_label(row, "Waiting", FONT_ASCII_UI, theme->dim, 30, 28, 150);
	lv_label_set_long_mode(chat_loop_body[index], LV_LABEL_LONG_DOT);
	chat_loop_status[index] = create_chip_label(row, "IDLE", FONT_ASCII_UI, theme->panel_soft,
						      LV_OPA_COVER, theme->accent_alt,
						      184, 12, 10, 6);
}

static void set_loop_row(uint32_t index, const char *title, const char *body,
			 const char *state, uint32_t state_bg, uint32_t state_fg)
{
	const ai_theme_s *theme = ai_theme();

	if (index >= CHAT_LOOP_COUNT) {
		return;
	}
	if (chat_loop_title[index] != NULL && title != NULL) {
		lv_label_set_text(chat_loop_title[index], title);
		lv_obj_set_style_text_color(chat_loop_title[index], lv_color_hex(theme->text), 0);
	}
	if (chat_loop_body[index] != NULL && body != NULL) {
		lv_label_set_text(chat_loop_body[index], body);
		lv_obj_set_style_text_color(chat_loop_body[index], lv_color_hex(theme->dim), 0);
	}
	if (chat_loop_status[index] != NULL && state != NULL) {
		lv_label_set_text(chat_loop_status[index], state);
		lv_obj_set_style_bg_color(chat_loop_status[index], lv_color_hex(state_bg), 0);
		lv_obj_set_style_text_color(chat_loop_status[index], lv_color_hex(state_fg), 0);
	}
}

static uint8_t ai_read_theme_mode(uint32_t *theme_mode_out, uint64_t *revision_out)
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

	*theme_mode_out = (uint32_t)response.entry.value_u64 == AI_THEME_DARK ?
		AI_THEME_DARK : AI_THEME_LIGHT;
	if (revision_out != NULL) {
		*revision_out = revision;
	}
	return 1U;
}

static void ai_reset_ui_refs(void)
{
	chat_badge_lbl = NULL;
	chat_title_lbl = NULL;
	chat_trace_lbl = NULL;
	chat_status_lbl = NULL;
	chat_history_panel = NULL;
	chat_history_lbl = NULL;
	chat_history_hint_lbl = NULL;
	chat_input_panel = NULL;
	chat_input_ta = NULL;
	chat_send_btn = NULL;
	chat_send_btn_label = NULL;
	memset(chat_loop_status, 0, sizeof(chat_loop_status));
	memset(chat_loop_title, 0, sizeof(chat_loop_title));
	memset(chat_loop_body, 0, sizeof(chat_loop_body));
	chat_agent_visual = NULL;
	chat_agent_ring_outer = NULL;
	chat_agent_ring_inner = NULL;
	chat_agent_core = NULL;
	chat_agent_iris = NULL;
	memset(chat_agent_nodes, 0, sizeof(chat_agent_nodes));
	g_hover_obj = NULL;
	g_interactive_count = 0;
}

static void register_interactive(lv_obj_t *obj, const char *name);
static void refresh_chat_card(void);
static void scroll_chat_history_to_bottom(void);
static void copy_text_safe(char *dst, uint64_t cap, const char *src);
static void build_log_preview(char *dst, uint64_t cap, const char *src);
static uint8_t ai_ensure_moonshot_auth(void);
static void request_kimi_probe(uint64_t mono_ms);
static void request_kimi_chat(uint64_t mono_ms, const char *user_text);

static int point_on_clickable_obj(lv_obj_t *obj, const lv_point_t *point)
{
	if (obj == NULL || point == NULL) {
		return 0;
	}
	if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) {
		return 0;
	}
	return lv_obj_hit_test(obj, point) ? 1 : 0;
}

static lv_obj_t *find_hover_target(const lv_point_t *point)
{
	int32_t idx;
	for (idx = (int32_t)g_interactive_count - 1; idx >= 0; idx--) {
		if (point_on_clickable_obj(g_interactive_objs[idx], point)) {
			return g_interactive_objs[idx];
		}
	}
	return NULL;
}

static void copy_text_safe(char *dst, uint64_t cap, const char *src)
{
	if (dst == NULL || cap == 0) {
		return;
	}
	if (src == NULL) {
		dst[0] = '\0';
		return;
	}
	strncpy(dst, src, cap - 1);
	dst[cap - 1] = '\0';
}

static void trim_trailing_space(char *text)
{
	uint64_t len = 0;

	if (text == NULL) {
		return;
	}

	len = strlen(text);
	while (len > 0) {
		char ch = text[len - 1];
		if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') {
			break;
		}
		text[len - 1] = '\0';
		len--;
	}
}

static uint8_t ai_read_text_file(const char *path, char *dst, uint64_t cap)
{
	fs_client_s *fs = NULL;
	systemd_client_s *systemd = NULL;
	uint64_t shm = 0;
	uint64_t fd = 0;
	uint64_t read_len = 0;

	if (path == NULL || dst == NULL || cap == 0) {
		return 0;
	}

	dst[0] = '\0';
	fs = fs_client_get();
	systemd = systemd_client_get();
	if (fs == NULL || systemd == NULL) {
		return 0;
	}

	fd = fs->ops.open(fs, (char *)path);
	if ((int64_t)fd < 0) {
		return 0;
	}

	shm = systemd->ops.alloc_shm(systemd, PAGE_SIZE);
	if (shm == 0) {
		fs->ops.close(fs, fd);
		return 0;
	}

	memset((void *)(uint64_t)shm, 0, PAGE_SIZE);
	read_len = fs->ops.read(fs, fd, shm, cap - 1);
	if (read_len > 0) {
		if (read_len >= cap) {
			read_len = cap - 1;
		}
		memcpy(dst, (void *)(uint64_t)shm, read_len);
		dst[read_len] = '\0';
		trim_trailing_space(dst);
	}

	systemd->ops.free_shm(systemd, shm);
	fs->ops.close(fs, fd);
	return read_len > 0 ? 1U : 0U;
}

static uint8_t ai_ensure_moonshot_auth(void)
{
	char api_key[MOONSHOT_AUTH_CAP] = {0};

	if (g_moonshot_auth_loaded) {
		return g_moonshot_auth_header[0] != '\0' ? 1U : 0U;
	}

	g_moonshot_auth_loaded = 1U;
	if (!ai_read_text_file(MOONSHOT_API_KEY_PATH, api_key, sizeof(api_key))) {
		return 0U;
	}

	sprintf(g_moonshot_auth_header, "Bearer %s", api_key);
	return 1U;
}

static void build_log_preview(char *dst, uint64_t cap, const char *src)
{
	uint64_t index = 0;

	if (dst == NULL || cap == 0) {
		return;
	}
	dst[0] = '\0';
	if (src == NULL) {
		return;
	}

	while (src[index] != '\0' && index + 4 < cap) {
		char ch = src[index];
		if (ch == '\n' || ch == '\r' || ch == '\t') {
			ch = ' ';
		}
		dst[index] = ch;
		index++;
	}

	if (src[index] != '\0' && cap >= 4) {
		dst[index++] = '.';
		dst[index++] = '.';
		dst[index++] = '.';
	}
	dst[index] = '\0';
}

static const char *find_last_text(const char *text, const char *pattern)
{
	const char *last = NULL;
	uint64_t text_len;
	uint64_t pattern_len;
	uint64_t idx;

	if (text == NULL || pattern == NULL) {
		return NULL;
	}

	text_len = strlen(text);
	pattern_len = strlen(pattern);
	if (pattern_len == 0 || text_len < pattern_len) {
		return NULL;
	}

	for (idx = 0; idx + pattern_len <= text_len; idx++) {
		if (memcmp(text + idx, pattern, pattern_len) == 0) {
			last = text + idx;
		}
	}

	return last;
}

static void set_chat_transcript(const char *text)
{
	copy_text_safe(g_chat_transcript, sizeof(g_chat_transcript), text);
}

static void append_chat_block(const char *speaker, const char *text)
{
	static char block[384];
	uint64_t cur_len;
	uint64_t block_len;
	uint64_t speaker_len;
	uint64_t text_len;

	if (speaker == NULL || text == NULL) {
		return;
	}

	speaker_len = strlen(speaker);
	text_len = strlen(text);
	if (speaker_len + text_len + 4 >= sizeof(block)) {
		copy_text_safe(g_chat_transcript, sizeof(g_chat_transcript),
			      "SYSTEM\nmessage truncated\n\n");
		return;
	}

	sprintf(block, "%s\n%s\n\n", speaker, text);
	block_len = strlen(block);
	if (block_len >= sizeof(g_chat_transcript)) {
		copy_text_safe(g_chat_transcript, sizeof(g_chat_transcript),
			      "SYSTEM\nmessage truncated\n\n");
		return;
	}

	cur_len = strlen(g_chat_transcript);
	if (cur_len + block_len >= sizeof(g_chat_transcript)) {
		copy_text_safe(g_chat_transcript, sizeof(g_chat_transcript),
			      "SYSTEM\nolder messages cleared\n\n");
		cur_len = strlen(g_chat_transcript);
	}

	memcpy(g_chat_transcript + cur_len, block, block_len + 1);
}

static void json_escape_string(char *dst, uint64_t cap, const char *src)
{
	uint64_t out = 0;

	if (dst == NULL || cap == 0) {
		return;
	}
	dst[0] = '\0';
	if (src == NULL) {
		return;
	}

	while (*src != '\0' && out + 1 < cap) {
		char ch = *src++;

		if ((ch == '\\' || ch == '"') && out + 2 < cap) {
			dst[out++] = '\\';
			dst[out++] = ch;
		} else if ((ch == '\n' || ch == '\r') && out + 2 < cap) {
			dst[out++] = '\\';
			dst[out++] = 'n';
		} else {
			dst[out++] = ch;
		}
	}

	dst[out] = '\0';
}

static void extract_kimi_content(const char *json, char *dst, uint64_t cap)
{
	const char *marker;
	const char *cursor;
	uint64_t out = 0;

	if (dst == NULL || cap == 0) {
		return;
	}
	dst[0] = '\0';
	if (json == NULL) {
		return;
	}

	marker = find_last_text(json, "\"content\":\"");
	if (marker == NULL) {
		return;
	}

	cursor = marker + 11;
	while (*cursor != '\0' && out + 1 < cap) {
		char ch = *cursor++;

		if (ch == '"') {
			break;
		}
		if (ch == '\\') {
			char esc = *cursor++;
			if (esc == 'n' || esc == 'r') {
				ch = '\n';
			} else if (esc == '"' || esc == '\\' || esc == '/') {
				ch = esc;
			} else {
				ch = esc;
			}
		}
		dst[out++] = ch;
	}

	dst[out] = '\0';
}

static void build_chat_request_body(const char *user_text, char *body, uint64_t cap)
{
	char escaped_user[CHAT_INPUT_CAP * 2];
	static const char *fmt =
		"{\"model\":\"moonshot-v1-8k\",\"messages\":[{\"role\":\"system\",\"content\":\"%s\"},"
		"{\"role\":\"user\",\"content\":\"%s\"}],\"temperature\":0.3}";
	uint64_t needed = 0;

	if (body == NULL || cap == 0) {
		return;
	}
	body[0] = '\0';
	if (user_text == NULL || user_text[0] == '\0') {
		return;
	}

	json_escape_string(escaped_user, sizeof(escaped_user), user_text);
	needed = strlen(fmt) + strlen(KIMI_CHAT_SYSTEM_PROMPT) + strlen(escaped_user) - 4 + 1;
	if (needed > cap) {
		return;
	}

	sprintf(body, fmt, KIMI_CHAT_SYSTEM_PROMPT, escaped_user);
}

static int text_contains(const char *haystack, const char *needle)
{
	uint64_t haystack_len;
	uint64_t needle_len;
	uint64_t idx;

	if (haystack == NULL || needle == NULL) {
		return 0;
	}

	haystack_len = strlen(haystack);
	needle_len = strlen(needle);
	if (needle_len == 0 || haystack_len < needle_len) {
		return 0;
	}

	for (idx = 0; idx + needle_len <= haystack_len; idx++) {
		if (memcmp(haystack + idx, needle, needle_len) == 0) {
			return 1;
		}
	}
	return 0;
}

static void refresh_chat_card(void)
{
	const ai_theme_s *theme = ai_theme();
	char trace_text[128] = {0};
	char reply_preview[96] = {0};
	const char *badge_text = "WAIT";
	const char *status_text = "Waiting for network access.";
	uint32_t badge_bg = theme->panel_soft;
	uint32_t badge_fg = theme->text;
	uint32_t trace_color = theme->dim;

	if (chat_history_lbl == NULL || chat_trace_lbl == NULL) {
		return;
	}

	build_log_preview(reply_preview, sizeof(reply_preview), g_kimi_reply);
	if (chat_title_lbl != NULL) {
		lv_label_set_text(chat_title_lbl, "Resident Agent");
	}
	lv_label_set_text(chat_history_lbl, g_chat_transcript);

	if (g_kimi_state == KIMI_STATE_READY) {
		badge_text = "LIVE";
		badge_bg = theme->success;
		badge_fg = theme->chip_text;
		trace_color = theme->success;
		if (g_chat_user_turns == 0U) {
			status_text = "Ready for the next goal.";
			copy_text_safe(trace_text, sizeof(trace_text), "Connected. Standing by for intent.");
			set_loop_row(0U, "Observe goal", "Model and network connected.", "LIVE",
				     theme->success, theme->chip_text);
			set_loop_row(1U, "Hold context", "Waiting for operator brief.", "IDLE",
				     theme->panel_soft, theme->accent_alt);
			set_loop_row(2U, "Prepare reply", "Reply lane is idle.", "WAIT",
				     theme->panel_alt, theme->dim);
		} else {
			status_text = "Context loaded. Ready to continue.";
			snprintf(trace_text, sizeof(trace_text), "Last trace: %s", g_kimi_trace);
			set_loop_row(0U, "Observe goal", g_kimi_trace, "LIVE",
				     theme->success, theme->chip_text);
			set_loop_row(1U, "Hold context", "Tracking live operator thread.", "LIVE",
				     theme->success, theme->chip_text);
			set_loop_row(2U, "Prepare reply", reply_preview[0] != '\0' ? reply_preview : "Reply ready.",
				     "READY", theme->panel_soft, theme->accent_alt);
		}
	} else if (g_kimi_state == KIMI_STATE_CONNECTING) {
		badge_text = "THINK";
		badge_bg = theme->accent;
		badge_fg = theme->chip_text;
		trace_color = theme->accent;
		status_text = "Reasoning on current goal.";
		snprintf(trace_text, sizeof(trace_text), "Trace: %s", g_kimi_trace);
		set_loop_row(0U, "Observe goal", "Reading operator prompt.", "ACTIVE",
			     theme->panel_soft, theme->accent_alt);
		set_loop_row(1U, "Hold context", "Loading context into model.", "ACTIVE",
			     theme->accent, theme->chip_text);
		set_loop_row(2U, "Prepare reply", "Drafting next action.", "WAIT",
			     theme->panel_alt, theme->dim);
	} else if (g_kimi_state == KIMI_STATE_ERROR) {
		badge_text = "ERROR";
		badge_bg = theme->alert;
		badge_fg = theme->chip_text;
		trace_color = theme->alert;
		status_text = "Agent is blocked by network or auth.";
		snprintf(trace_text, sizeof(trace_text), "Trace: %s", g_kimi_trace);
		set_loop_row(0U, "Observe goal", "Model unreachable.", "BLOCK",
			     theme->alert, theme->chip_text);
		set_loop_row(1U, "Hold context", "Check network or API key.", "WAIT",
			     theme->panel_alt, theme->dim);
		set_loop_row(2U, "Prepare reply", "Reply lane blocked.", "HOLD",
			     theme->warm, theme->chip_text);
	} else {
		badge_text = "WAIT";
		badge_bg = theme->panel_soft;
		badge_fg = theme->accent_alt;
		trace_color = theme->dim;
		status_text = "Waiting for model access.";
		snprintf(trace_text, sizeof(trace_text), "Trace: %s", g_kimi_trace);
		set_loop_row(0U, "Observe goal", "Waiting for network service.", "WAIT",
			     theme->panel_alt, theme->dim);
		set_loop_row(1U, "Hold context", "Loading model availability.", "WAIT",
			     theme->panel_alt, theme->dim);
		set_loop_row(2U, "Prepare reply", "Reply lane is idle.", "IDLE",
			     theme->panel_soft, theme->accent_alt);
	}

	if (chat_badge_lbl != NULL) {
		lv_label_set_text(chat_badge_lbl, badge_text);
		lv_obj_set_style_bg_color(chat_badge_lbl, lv_color_hex(badge_bg), 0);
		lv_obj_set_style_text_color(chat_badge_lbl, lv_color_hex(badge_fg), 0);
	}
	if (chat_status_lbl != NULL) {
		lv_label_set_text(chat_status_lbl, status_text);
		lv_obj_set_style_text_color(chat_status_lbl, lv_color_hex(theme->dim), 0);
	}
	lv_label_set_text(chat_trace_lbl, trace_text);
	if (chat_title_lbl != NULL) {
		lv_obj_set_style_text_color(chat_title_lbl, lv_color_hex(0xfff7f1), 0);
	}
	lv_obj_set_style_text_color(chat_history_lbl, lv_color_hex(0xf8efe6), 0);
	lv_obj_set_style_text_color(chat_trace_lbl, lv_color_hex(trace_color), 0);
	if (chat_history_hint_lbl != NULL) {
		if (g_chat_user_turns == 0U) {
			lv_label_set_text(chat_history_hint_lbl, "Awaiting first operator task");
		} else {
			lv_label_set_text(chat_history_hint_lbl, "");
		}
	}
	scroll_chat_history_to_bottom();
}

static void scroll_chat_history_to_bottom(void)
{
	lv_coord_t target_y;

	if (chat_history_panel == NULL || chat_history_lbl == NULL) {
		return;
	}

	lv_obj_update_layout(chat_history_panel);
	lv_obj_update_layout(chat_history_lbl);
	target_y = lv_obj_get_scroll_y(chat_history_panel) +
		   lv_obj_get_scroll_bottom(chat_history_panel);
	lv_obj_scroll_to_y(chat_history_panel, target_y, LV_ANIM_OFF);
}

static void request_kimi_probe(uint64_t mono_ms)
{
	char response[1024] = {0};
	char preview[96];
	uint64_t result;

	g_kimi_last_attempt_ms = mono_ms;
	if (!ai_ensure_moonshot_auth()) {
		g_kimi_state = KIMI_STATE_ERROR;
		g_kimi_status = 0;
			copy_text_safe(g_kimi_reply, sizeof(g_kimi_reply), "moonshot api key missing");
			copy_text_safe(g_kimi_trace, sizeof(g_kimi_trace), MOONSHOT_API_KEY_PATH);
			if (g_chat_user_turns == 0) {
				set_chat_transcript("SYSTEM\nMoonshot API key missing.\nInstall the resident agent key to continue.\n\n");
			}
		refresh_chat_card();
		return;
	}

	g_https = https_client_get();
	if (g_https == NULL) {
		g_kimi_state = KIMI_STATE_WAITING_NET;
			copy_text_safe(g_kimi_reply, sizeof(g_kimi_reply), "net service unavailable");
			copy_text_safe(g_kimi_trace, sizeof(g_kimi_trace), "waiting for network service");
			if (g_chat_user_turns == 0) {
				set_chat_transcript("SYSTEM\nWaiting for network service.\nThe resident agent cannot act until network returns.\n\n");
			}
		refresh_chat_card();
		return;
	}

	g_kimi_state = KIMI_STATE_CONNECTING;
	g_kimi_status = 0;
	copy_text_safe(g_kimi_reply, sizeof(g_kimi_reply), "waiting...");
	copy_text_safe(g_kimi_trace, sizeof(g_kimi_trace), "POST /v1/chat/completions");
	refresh_chat_card();

	result = g_https->ops.post_json(g_https, KIMI_URL, g_moonshot_auth_header,
					KIMI_REQUEST_BODY, response, sizeof(response));
	g_kimi_status = https_result_status(result);
	if (g_kimi_status == 200 && text_contains(response, KIMI_EXPECTED_REPLY)) {
		g_kimi_state = KIMI_STATE_READY;
			copy_text_safe(g_kimi_reply, sizeof(g_kimi_reply), KIMI_EXPECTED_REPLY);
				sprintf(g_kimi_trace, "HTTP %u / moonshot-v1-8k", g_kimi_status);
				g_kimi_probe_done = 1;
				if (g_chat_user_turns == 0) {
					set_chat_transcript("SYSTEM\nResident agent online.\nSend a goal, context, or blocker.\n\n");
				}
		build_log_preview(preview, sizeof(preview), response);
		log_info("kimi probe ok: len=%llu preview=%s\n",
			 (unsigned long long)strlen(response), preview);
	} else {
		g_kimi_state = KIMI_STATE_ERROR;
		if (response[0] != '\0') {
			copy_text_safe(g_kimi_reply, sizeof(g_kimi_reply), response);
		} else {
			copy_text_safe(g_kimi_reply, sizeof(g_kimi_reply), "no reply");
		}
		if (g_kimi_status != 0) {
			g_kimi_probe_done = 1;
			sprintf(g_kimi_trace, "HTTP %u / moonshot-v1-8k", g_kimi_status);
		} else {
				copy_text_safe(g_kimi_trace, sizeof(g_kimi_trace), "network timeout / auto retry");
			}
			if (g_chat_user_turns == 0) {
				set_chat_transcript("SYSTEM\nAgent is blocked.\nCheck network or API key, then retry.\n\n");
			}
		build_log_preview(preview, sizeof(preview), response);
		log_warn("kimi probe failed: status=%d len=%llu preview=%s\n",
			 g_kimi_status, (unsigned long long)strlen(response), preview);
	}
	refresh_chat_card();
}

static void request_kimi_chat(uint64_t mono_ms, const char *user_text)
{
	char body[CHAT_REQUEST_BODY_CAP];
	char response[1024] = {0};
	char assistant_text[320];
	char preview[96];
	uint64_t result;

	if (user_text == NULL || user_text[0] == '\0') {
		copy_text_safe(g_kimi_trace, sizeof(g_kimi_trace), "describe a goal first");
		refresh_chat_card();
		return;
	}

		build_chat_request_body(user_text, body, sizeof(body));
	append_chat_block("OPERATOR", user_text);
	g_chat_user_turns++;

	g_kimi_last_attempt_ms = mono_ms;
	if (!ai_ensure_moonshot_auth()) {
		g_kimi_state = KIMI_STATE_ERROR;
		g_kimi_status = 0;
		copy_text_safe(g_kimi_reply, sizeof(g_kimi_reply), "moonshot api key missing");
		copy_text_safe(g_kimi_trace, sizeof(g_kimi_trace), MOONSHOT_API_KEY_PATH);
		append_chat_block("SYSTEM", "Moonshot API key missing for the resident agent.");
		refresh_chat_card();
		return;
	}

	g_https = https_client_get();
	if (g_https == NULL) {
		g_kimi_state = KIMI_STATE_WAITING_NET;
		g_kimi_status = 0;
		copy_text_safe(g_kimi_reply, sizeof(g_kimi_reply), "net service unavailable");
		copy_text_safe(g_kimi_trace, sizeof(g_kimi_trace), "waiting for network service");
		append_chat_block("SYSTEM", "Network service unavailable.");
		refresh_chat_card();
		return;
	}

	g_kimi_state = KIMI_STATE_CONNECTING;
	g_kimi_status = 0;
	copy_text_safe(g_kimi_reply, sizeof(g_kimi_reply), "waiting...");
	copy_text_safe(g_kimi_trace, sizeof(g_kimi_trace), "routing prompt to model");

	result = g_https->ops.post_json(g_https, KIMI_URL, g_moonshot_auth_header,
					body, response, sizeof(response));
	g_kimi_status = https_result_status(result);
	extract_kimi_content(response, assistant_text, sizeof(assistant_text));

	if (g_kimi_status == 200 && assistant_text[0] != '\0') {
		g_kimi_state = KIMI_STATE_READY;
			copy_text_safe(g_kimi_reply, sizeof(g_kimi_reply), assistant_text);
			sprintf(g_kimi_trace, "HTTP %u / moonshot-v1-8k", g_kimi_status);
			g_kimi_probe_done = 1;
			append_chat_block("AGENT", assistant_text);
		build_log_preview(preview, sizeof(preview), assistant_text);
		log_info("kimi chat ok: len=%llu preview=%s\n",
			 (unsigned long long)strlen(assistant_text), preview);
	} else {
		g_kimi_state = KIMI_STATE_ERROR;
		if (assistant_text[0] != '\0') {
			copy_text_safe(g_kimi_reply, sizeof(g_kimi_reply), assistant_text);
		} else if (response[0] != '\0') {
			copy_text_safe(g_kimi_reply, sizeof(g_kimi_reply), response);
		} else {
			copy_text_safe(g_kimi_reply, sizeof(g_kimi_reply), "no reply");
		}
		if (g_kimi_status != 0) {
			g_kimi_probe_done = 1;
			sprintf(g_kimi_trace, "HTTP %u / moonshot-v1-8k", g_kimi_status);
		} else {
			copy_text_safe(g_kimi_trace, sizeof(g_kimi_trace), "network timeout / retry");
		}
		append_chat_block("SYSTEM", g_kimi_reply);
		build_log_preview(preview, sizeof(preview), response);
		log_warn("kimi chat failed: status=%d len=%llu preview=%s\n",
			 g_kimi_status, (unsigned long long)strlen(response), preview);
	}

	refresh_chat_card();
}

static void update_kimi_probe(uint64_t mono_ms)
{
	if (g_kimi_probe_done || g_kimi_state == KIMI_STATE_CONNECTING) {
		return;
	}
	if (mono_ms < KIMI_INITIAL_DELAY_MS) {
		return;
	}
	if (mono_ms - g_kimi_last_attempt_ms < KIMI_RETRY_INTERVAL_MS) {
		return;
	}
	request_kimi_probe(mono_ms);
}

static void interactive_event_cb(lv_event_t *e)
{
	const char *name = (const char *)lv_event_get_user_data(e);

	if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
		return;
	}

	if (lv_event_get_target(e) == chat_send_btn) {
		request_kimi_chat(OSSysCtrlGetMonoTime() / NSEC_PER_MSEC,
				 lv_textarea_get_text(chat_input_ta));
		lv_textarea_set_text(chat_input_ta, "");
		lv_group_focus_obj(chat_input_ta);
	}

	log_info("%s clicked\n", name);
}

static void chat_input_event_cb(lv_event_t *e)
{
	if (lv_event_get_code(e) != LV_EVENT_READY || chat_input_ta == NULL) {
		return;
	}

	request_kimi_chat(OSSysCtrlGetMonoTime() / NSEC_PER_MSEC,
			 lv_textarea_get_text(chat_input_ta));
	lv_textarea_set_text(chat_input_ta, "");
	lv_group_focus_obj(chat_input_ta);
}

static void register_interactive(lv_obj_t *obj, const char *name)
{
	if (obj == NULL || name == NULL || g_interactive_count >= INTERACTIVE_MAX) {
		return;
	}

	g_interactive_objs[g_interactive_count] = obj;
	g_interactive_count++;

	lv_obj_add_event_cb(obj, interactive_event_cb, LV_EVENT_CLICKED, (void *)name);
}

static void update_hover_state(void)
{
	lv_obj_t *hover_target = NULL;
	lv_indev_state_t state = lv_port_indev_get_touch_state();

	if (state == LV_INDEV_STATE_PRESSED) {
		lv_point_t point = lv_port_indev_get_touch_point();
		hover_target = find_hover_target(&point);
	}

	if (hover_target == g_hover_obj) {
		return;
	}

	if (g_hover_obj != NULL) {
		lv_obj_clear_state(g_hover_obj, LV_STATE_HOVERED);
	}

	g_hover_obj = hover_target;
	if (g_hover_obj != NULL) {
		lv_obj_add_state(g_hover_obj, LV_STATE_HOVERED);
	}
}

static void update_agent_motion(uint64_t mono_ms)
{
	uint32_t i;

	if (chat_agent_visual == NULL || chat_agent_core == NULL || chat_agent_iris == NULL) {
		return;
	}

	lv_obj_set_style_translate_y(chat_agent_visual, wave_offset(mono_ms, 3600U, 3, 0U), 0);
	lv_obj_set_style_translate_y(chat_agent_core, wave_offset(mono_ms, 2800U, 4, 420U), 0);
	lv_obj_set_x(chat_agent_iris, (lv_coord_t)(28 + wave_offset(mono_ms, 2200U, 7, 180U)));
	if (chat_agent_ring_outer != NULL) {
		lv_obj_set_style_border_opa(chat_agent_ring_outer,
					    wave_opa(mono_ms, 4400U, 26, 72, 0U), 0);
	}
	if (chat_agent_ring_inner != NULL) {
		lv_obj_set_style_border_opa(chat_agent_ring_inner,
					    wave_opa(mono_ms, 3200U, 32, 92, 760U), 0);
		lv_obj_set_style_bg_opa(chat_agent_ring_inner,
					wave_opa(mono_ms, 3200U, 8, 18, 760U), 0);
	}

	for (i = 0; i < CHAT_AGENT_NODE_COUNT; ++i) {
		if (chat_agent_nodes[i] == NULL) {
			continue;
		}
		lv_obj_set_style_bg_opa(chat_agent_nodes[i],
					wave_opa(mono_ms, 2600U, 18, 72, i * 380U), 0);
		lv_obj_set_style_border_opa(chat_agent_nodes[i],
					    wave_opa(mono_ms, 2600U, 32, 100, i * 380U), 0);
	}
}

static void create_background(lv_obj_t *scr)
{
	const ai_theme_s *theme = ai_theme();

	lv_obj_set_style_bg_color(scr, lv_color_hex(theme->bg), 0);
	lv_obj_set_style_bg_grad_color(scr, lv_color_hex(theme->bg_grad), 0);
	lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, 0);
	lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

	lv_obj_t *glow_a = lv_obj_create(scr);
	lv_obj_set_size(glow_a, 280, 280);
	lv_obj_align(glow_a, LV_ALIGN_TOP_LEFT, -96, -86);
	lv_obj_set_style_bg_color(glow_a, lv_color_hex(theme->accent_alt), 0);
	lv_obj_set_style_bg_opa(glow_a, LV_OPA_10, 0);
	lv_obj_set_style_border_width(glow_a, 0, 0);
	lv_obj_set_style_radius(glow_a, LV_RADIUS_CIRCLE, 0);
	lv_obj_clear_flag(glow_a, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CLICK_FOCUSABLE);

	lv_obj_t *glow_b = lv_obj_create(scr);
	lv_obj_set_size(glow_b, 336, 336);
	lv_obj_align(glow_b, LV_ALIGN_BOTTOM_RIGHT, 118, 132);
	lv_obj_set_style_bg_color(glow_b, lv_color_hex(theme->warm), 0);
	lv_obj_set_style_bg_opa(glow_b, 8, 0);
	lv_obj_set_style_border_width(glow_b, 0, 0);
	lv_obj_set_style_radius(glow_b, LV_RADIUS_CIRCLE, 0);
	lv_obj_clear_flag(glow_b, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CLICK_FOCUSABLE);
}

static void create_chat_card(lv_obj_t *scr)
{
	const ai_theme_s *theme = ai_theme();
	uint32_t core_bg = g_theme_mode == AI_THEME_DARK ? 0x1b2032 : 0x263148;
	uint32_t core_line = g_theme_mode == AI_THEME_DARK ? 0x3a4562 : 0x32415f;
	lv_obj_t *chat_card;
	lv_obj_t *model_chip;
	lv_obj_t *queue_panel;
	lv_obj_t *thread_panel;

	chat_card = create_panel(scr, CHAT_CARD_X, CHAT_CARD_Y, CHAT_CARD_WIDTH, CHAT_CARD_HEIGHT, theme->panel, 235,
				 theme->line, LV_OPA_70, 32);

	create_chip_label(chat_card, "AGENT CONSOLE", FONT_ASCII_UI, theme->panel_alt, LV_OPA_COVER,
			  theme->accent, CHAT_CARD_INSET, CHAT_CARD_INSET, 12, 7);
	model_chip = create_chip_label(chat_card, "moonshot-v1-8k", FONT_ASCII_UI, theme->panel_alt, LV_OPA_COVER,
				       theme->dim, 0, CHAT_CARD_INSET, 12, 7);
	lv_obj_align(model_chip, LV_ALIGN_TOP_RIGHT, -CHAT_CARD_INSET, CHAT_CARD_INSET);

	queue_panel = create_panel(chat_card, CHAT_CARD_INSET, CHAT_QUEUE_Y, CHAT_LEFT_COL_WIDTH, CHAT_QUEUE_HEIGHT,
				   theme->panel_alt, LV_OPA_COVER, theme->line, LV_OPA_60, 24);
	create_label(queue_panel, "Active tasks", FONT_SECTION, theme->text, 16, 16, 148);
	create_chip_label(queue_panel, "3 lanes", FONT_ASCII_UI, theme->panel_soft, LV_OPA_COVER,
			  theme->accent_alt, 208, 16, 10, 5);
	chat_status_lbl = create_label(queue_panel, "Waiting for model access.", FONT_ASCII_UI, theme->dim,
				       16, CHAT_QUEUE_STATUS_Y, 248);
	create_loop_row(queue_panel, 0U, CHAT_QUEUE_ROW_Y);
	create_loop_row(queue_panel, 1U, CHAT_QUEUE_ROW_Y + CHAT_QUEUE_ROW_GAP);
	create_loop_row(queue_panel, 2U, CHAT_QUEUE_ROW_Y + CHAT_QUEUE_ROW_GAP * 2);

	thread_panel = create_panel(chat_card, CHAT_RIGHT_X, CHAT_TOP_ROW_Y, CHAT_RIGHT_COL_WIDTH, CHAT_THREAD_HEIGHT,
				    theme->panel_alt, 220, theme->line, LV_OPA_60, 28);
	create_label(thread_panel, "Mission thread", FONT_TITLE, theme->text, CHAT_THREAD_INSET, CHAT_THREAD_HEADER_Y, 280);
	create_chip_label(thread_panel, "live", FONT_ASCII_UI, theme->panel_soft, LV_OPA_COVER,
			  theme->success, 448, 24, 10, 5);
	chat_trace_lbl = create_label(thread_panel, "warming after boot", FONT_ASCII_UI, theme->dim,
				      CHAT_THREAD_INSET, 60, 400);

	chat_history_panel = create_panel(thread_panel, CHAT_THREAD_INSET, CHAT_HISTORY_Y,
					  CHAT_HISTORY_WIDTH, CHAT_HISTORY_HEIGHT, core_bg,
					  LV_OPA_COVER, core_line, LV_OPA_COVER, 24);
	lv_obj_add_flag(chat_history_panel, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_scroll_dir(chat_history_panel, LV_DIR_VER);
	lv_obj_set_scrollbar_mode(chat_history_panel, LV_SCROLLBAR_MODE_AUTO);
	lv_obj_set_style_bg_color(chat_history_panel, lv_color_hex(core_bg), 0);
	lv_obj_set_style_bg_opa(chat_history_panel, LV_OPA_COVER, 0);
	lv_obj_set_style_pad_left(chat_history_panel, 20, 0);
	lv_obj_set_style_pad_right(chat_history_panel, 28, 0);
	lv_obj_set_style_pad_top(chat_history_panel, 18, 0);
	lv_obj_set_style_pad_bottom(chat_history_panel, 18, 0);
	lv_obj_set_style_shadow_width(chat_history_panel, 0, 0);
	lv_obj_set_style_width(chat_history_panel, 6, LV_PART_SCROLLBAR);
	lv_obj_set_style_bg_color(chat_history_panel, lv_color_hex(theme->accent_alt), LV_PART_SCROLLBAR);
	lv_obj_set_style_bg_opa(chat_history_panel, LV_OPA_50, LV_PART_SCROLLBAR);
	lv_obj_set_style_radius(chat_history_panel, 999, LV_PART_SCROLLBAR);
	lv_obj_set_style_pad_right(chat_history_panel, 6, LV_PART_SCROLLBAR);
	lv_obj_set_style_pad_top(chat_history_panel, 8, LV_PART_SCROLLBAR);
	lv_obj_set_style_pad_bottom(chat_history_panel, 8, LV_PART_SCROLLBAR);
	lv_obj_set_style_width(chat_history_panel, 8, LV_PART_SCROLLBAR | LV_STATE_SCROLLED);
	lv_obj_set_style_bg_opa(chat_history_panel, LV_OPA_80, LV_PART_SCROLLBAR | LV_STATE_SCROLLED);

	chat_history_lbl = create_label(chat_history_panel,
					g_chat_transcript,
					FONT_UI,
					0xf8efe6,
					0,
					0,
					(CHAT_HISTORY_WIDTH - 92));
	lv_obj_set_style_text_line_space(chat_history_lbl, 10, 0);
	lv_obj_set_style_text_opa(chat_history_lbl, LV_OPA_90, 0);
	chat_history_hint_lbl = create_label(chat_history_panel, "", FONT_ASCII_UI, 0x7f8aa6,
					     20, (lv_coord_t)(CHAT_HISTORY_HEIGHT - 44), 280);

	chat_input_panel = create_panel(thread_panel, CHAT_THREAD_INSET, CHAT_INPUT_Y,
					 CHAT_HISTORY_WIDTH, CHAT_INPUT_HEIGHT, core_bg,
					 220, core_line, LV_OPA_COVER, 24);
	lv_obj_set_style_shadow_width(chat_input_panel, 0, 0);
	chat_input_ta = lv_textarea_create(chat_input_panel);
	lv_obj_set_pos(chat_input_ta, CHAT_INPUT_INSET, 12);
	lv_obj_set_size(chat_input_ta, CHAT_INPUT_FIELD_WIDTH, 48);
	lv_obj_set_style_bg_color(chat_input_ta, lv_color_hex(0x2e3a56), 0);
	lv_obj_set_style_bg_opa(chat_input_ta, LV_OPA_COVER, 0);
	lv_obj_set_style_border_color(chat_input_ta, lv_color_hex(core_line), 0);
	lv_obj_set_style_border_opa(chat_input_ta, LV_OPA_80, 0);
	lv_obj_set_style_border_width(chat_input_ta, 1, 0);
	lv_obj_set_style_radius(chat_input_ta, 16, 0);
	lv_obj_set_style_text_font(chat_input_ta, FONT_UI, 0);
	lv_obj_set_style_text_color(chat_input_ta, lv_color_hex(0xf8efe6), 0);
	lv_obj_set_style_pad_left(chat_input_ta, 14, 0);
	lv_obj_set_style_pad_right(chat_input_ta, 14, 0);
	lv_obj_set_style_pad_top(chat_input_ta, 14, 0);
	lv_obj_set_style_pad_bottom(chat_input_ta, 14, 0);
	lv_obj_clear_flag(chat_input_ta, LV_OBJ_FLAG_SCROLLABLE);
	lv_textarea_set_one_line(chat_input_ta, true);
	lv_textarea_set_max_length(chat_input_ta, CHAT_INPUT_CAP);
	lv_textarea_set_placeholder_text(chat_input_ta, "Give the agent a goal or constraint");
	lv_obj_add_event_cb(chat_input_ta, chat_input_event_cb, LV_EVENT_READY, NULL);
	if (g_chat_input_draft[0] != '\0') {
		lv_textarea_set_text(chat_input_ta, g_chat_input_draft);
	}

	chat_send_btn = create_button(chat_input_panel, "Dispatch",
					     CHAT_INPUT_INSET + CHAT_INPUT_FIELD_WIDTH + CHAT_INPUT_GAP,
					     12, CHAT_SEND_WIDTH, 48);
	register_interactive(chat_send_btn, "send");
}

static void create_ui(void)
{
	lv_obj_t *scr = lv_scr_act();

	ai_reset_ui_refs();
	create_background(scr);
	create_chat_card(scr);

	refresh_chat_card();
	if (chat_input_ta != NULL) {
		lv_group_focus_obj(chat_input_ta);
	}
}

static void ai_rebuild_ui(void)
{
	lv_obj_t *scr = lv_scr_act();

	if (chat_input_ta != NULL) {
		copy_text_safe(g_chat_input_draft, sizeof(g_chat_input_draft),
			      lv_textarea_get_text(chat_input_ta));
	}
	lv_obj_clean(scr);
	create_ui();
}

static void ai_refresh_theme(uint64_t mono_ms, uint8_t force)
{
	uint32_t theme_mode = g_theme_mode;
	uint64_t revision = 0;

	if (!force && mono_ms < g_last_theme_ms + THEME_REFRESH_INTERVAL_MS) {
		return;
	}
	g_last_theme_ms = mono_ms;

	if (!ai_read_theme_mode(&theme_mode, &revision)) {
		return;
	}
	if (!force && revision == g_last_theme_revision) {
		return;
	}
	g_last_theme_revision = revision;
	if (!force && theme_mode == g_theme_mode) {
		return;
	}

	g_theme_mode = theme_mode;
	ai_rebuild_ui();
	log_info("ai theme synced: mode=%u rev=%llu\n",
		 (unsigned int)g_theme_mode,
		 (unsigned long long)revision);
}

static void update_ui(uint64_t mono_ms)
{
	ai_refresh_theme(mono_ms, 0U);
	update_kimi_probe(mono_ms);
	update_hover_state();
	update_agent_motion(mono_ms);
}

static void ai_on_create(app_s *app)
{
	(void)app;

	(void)ai_read_theme_mode(&g_theme_mode, &g_last_theme_revision);
	(void)fs_client_get();
	create_ui();
	log_info("AI app ready\n");
}

static void ai_on_update(app_s *app, uint64_t mono_ms)
{
	(void)app;
	update_ui(mono_ms);
}

int main(int argc, char *argv[])
{
	app_s app = {0};
	app_config_s config = {
		.name = "ai",
		.x = APP_DEFAULT_X,
		.y = APP_DEFAULT_Y,
		.z = 0,
		.width = APP_DEFAULT_WIDTH,
		.height = APP_DEFAULT_HEIGHT,
		.window_flags = WINDOW_FLAG_FOCUSABLE,
		.enable_input = 1,
	};
	app_lifecycle_ops_s lifecycle = {
		.on_create = ai_on_create,
		.on_update = ai_on_update,
	};

	(void)argc;
	(void)argv;

	log_info("AI app starting...\n");

	app_init(&app, &config, &lifecycle, NULL);
	return app_run(&app);
}
