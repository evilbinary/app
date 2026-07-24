#include "lvgl.h"
#include "app.h"
#include "libwindow/window.h"
#include "log.h"
#include "libsystem/ipc.h"
#include "libsystem/statemgr_client.h"
#include "stdint.h"
#include "string.h"
#include "stdio.h"

#define SYSTEM_OVERLAY_GAP 24
#define CONTENT_X 40
#define CONTENT_Y SYSTEM_OVERLAY_GAP
#define CONTENT_W 944
#define REFRESH_INTERVAL_MS 1000ULL
#define SETTINGS_BODY_Y 176
#define SETTINGS_SIDEBAR_X CONTENT_X
#define SETTINGS_SIDEBAR_Y SETTINGS_BODY_Y
#define SETTINGS_SIDEBAR_W 304
#define SETTINGS_SIDEBAR_H 660
#define SETTINGS_DETAIL_X (SETTINGS_SIDEBAR_X + SETTINGS_SIDEBAR_W + 16)
#define SETTINGS_DETAIL_Y SETTINGS_BODY_Y
#define SETTINGS_DETAIL_W (CONTENT_W - SETTINGS_SIDEBAR_W - 16)
#define SETTINGS_DETAIL_H SETTINGS_SIDEBAR_H
#define SETTINGS_SWATCH_COUNT 5U
#define SETTINGS_IDLE_TIMEOUT_OPTION_COUNT 3U
#define SETTINGS_STATEMGR_SYNC_OFFLINE 0U
#define SETTINGS_STATEMGR_SYNC_OK 1U
#define SETTINGS_STATEMGR_SYNC_FAILED 2U
#define SETTINGS_STATE_KEY_IDLE_LOCK_TIMEOUT_SEC "ui.lockscreen.timeout_sec"

#define COLOR_BG          0xfff4ee
#define COLOR_BG_ALT      0xffe5d8
#define COLOR_PANEL       0xfffffb
#define COLOR_PANEL_ALT   0xffefe7
#define COLOR_PANEL_SOFT  0xffe6db
#define COLOR_LINE        0xf0ddd2
#define COLOR_TEXT        0x24324a
#define COLOR_DIM         0x7e6d68
#define COLOR_ACCENT      0xff7d5c
#define COLOR_ACCENT_SOFT 0xffe6db
#define COLOR_WARM        0xffb458
#define COLOR_WARM_SOFT   0xffefd1
#define COLOR_DARK        0x24324a
#define COLOR_DARK_SOFT   0xe9ddd7

typedef struct settings_swatch_spec {
	const char *label;
	uint32_t color;
} settings_swatch_spec_s;

typedef struct settings_timeout_option {
	const char *label;
	uint32_t seconds;
} settings_timeout_option_s;

typedef enum settings_theme_mode {
	SYSTEMUI_THEME_MODE_LIGHT = 0,
	SYSTEMUI_THEME_MODE_DARK = 1,
} settings_theme_mode_t;

typedef enum settings_section {
	SETTINGS_SECTION_PERSONALIZATION = 0,
	SETTINGS_SECTION_NETWORK = 1,
	SETTINGS_SECTION_COUNT = 2,
} settings_section_t;

typedef struct settings_state {
	uint64_t last_refresh_ms;
	uint32_t theme_mode;
	uint32_t desktop_color;
	uint32_t lock_timeout_seconds;
	uint64_t net_pool_cref;
	uint8_t network_enabled;
	uint8_t statemgr_ready;
	uint8_t net_ready;
	uint32_t selected_section;
	char notice_text[128];
	uint32_t notice_color;
} settings_state_s;

typedef struct settings_theme {
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
	uint32_t dark;
	uint32_t dark_soft;
} settings_theme_s;

static const settings_swatch_spec_s g_swatch_specs[SETTINGS_SWATCH_COUNT] = {
	{ "Sky", 0xeaf8ff },
	{ "Mint", 0xe9fbf2 },
	{ "Peach", 0xffefe3 },
	{ "Slate", 0x253550 },
	{ "Ink", 0x131f33 },
};

static const settings_timeout_option_s g_timeout_options[SETTINGS_IDLE_TIMEOUT_OPTION_COUNT] = {
	{ "10s", 10U },
	{ "15s", 15U },
	{ "30s", 30U },
};

static settings_state_s g_settings = {
	.theme_mode = SYSTEMUI_THEME_MODE_LIGHT,
	.desktop_color = 0xffefe3,
	.lock_timeout_seconds = 10U,
	.network_enabled = 1U,
	.selected_section = SETTINGS_SECTION_PERSONALIZATION,
	.notice_color = COLOR_DIM,
};

static const settings_theme_s g_settings_theme_light = {
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
	.dark = COLOR_DARK,
	.dark_soft = COLOR_DARK_SOFT,
};

static const settings_theme_s g_settings_theme_dark = {
	.bg = 0x171828,
	.bg_alt = 0x22253c,
	.panel = 0x20243a,
	.panel_alt = 0x2a3048,
	.panel_soft = 0x4a2f29,
	.line = 0x46506f,
	.text = 0xfff6ee,
	.dim = 0xc6b5ad,
	.accent = 0xffa07c,
	.accent_soft = 0x4a2f29,
	.warm = 0xf5c575,
	.warm_soft = 0x4b3928,
	.dark = 0xfff6ee,
	.dark_soft = 0x545c76,
};

static statemgr_client_s *g_statemgr = NULL;

static lv_obj_t *g_status_chip = NULL;
static lv_obj_t *g_section_buttons[SETTINGS_SECTION_COUNT] = {0};
static lv_obj_t *g_section_titles[SETTINGS_SECTION_COUNT] = {0};
static lv_obj_t *g_theme_buttons[2] = {0};
static lv_obj_t *g_swatch_buttons[SETTINGS_SWATCH_COUNT] = {0};
static lv_obj_t *g_timeout_buttons[SETTINGS_IDLE_TIMEOUT_OPTION_COUNT] = {0};
static lv_obj_t *g_network_button = NULL;
static lv_obj_t *g_network_button_label = NULL;
static lv_obj_t *g_network_chip = NULL;
static lv_obj_t *g_personalization_view = NULL;
static lv_obj_t *g_network_view = NULL;
static lv_obj_t *g_preview_stage = NULL;
static lv_obj_t *g_preview_shell = NULL;
static lv_obj_t *g_preview_bar = NULL;
static lv_obj_t *g_preview_title = NULL;
static lv_obj_t *g_preview_meta = NULL;
static lv_obj_t *g_lock_timeout_meta = NULL;
static lv_obj_t *g_network_state_value = NULL;
static lv_obj_t *g_network_state_meta = NULL;
static lv_obj_t *g_network_scope_meta = NULL;

static void settings_refresh(uint64_t mono_ms, uint8_t force);
static void create_ui(void);
static void settings_theme_event_cb(lv_event_t *e);
static void settings_swatch_event_cb(lv_event_t *e);
static void settings_timeout_event_cb(lv_event_t *e);
static void settings_network_event_cb(lv_event_t *e);
static void settings_section_event_cb(lv_event_t *e);

static const settings_theme_s *settings_theme(void)
{
	return g_settings.theme_mode == SYSTEMUI_THEME_MODE_DARK ?
		&g_settings_theme_dark : &g_settings_theme_light;
}

static uint32_t settings_normalize_timeout_seconds(uint32_t seconds)
{
	for (uint32_t i = 0; i < SETTINGS_IDLE_TIMEOUT_OPTION_COUNT; i++) {
		if (g_timeout_options[i].seconds == seconds) {
			return seconds;
		}
	}

	return g_timeout_options[0].seconds;
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

static lv_obj_t *create_segment_button(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
				       lv_coord_t w, const char *text,
				       lv_event_cb_t cb, void *user_data)
{
	const settings_theme_s *theme = settings_theme();
	lv_obj_t *btn = lv_btn_create(parent);
	lv_obj_t *label = lv_label_create(btn);

	lv_obj_set_pos(btn, x, y);
	lv_obj_set_size(btn, w, 48);
	lv_obj_set_style_radius(btn, 22, 0);
	lv_obj_set_style_bg_color(btn, lv_color_hex(theme->panel_alt), 0);
	lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
	lv_obj_set_style_border_color(btn, lv_color_hex(theme->line), 0);
	lv_obj_set_style_border_width(btn, 1, 0);
	lv_obj_set_style_shadow_width(btn, 0, 0);
	lv_obj_set_style_outline_width(btn, 0, LV_STATE_FOCUSED);
	lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);

	lv_label_set_text(label, text);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->text), 0);
	lv_obj_center(label);
	return btn;
}

static lv_obj_t *create_content_view(lv_obj_t *parent)
{
	lv_obj_t *view = lv_obj_create(parent);

	lv_obj_set_pos(view, 0, 0);
	lv_obj_set_size(view, SETTINGS_DETAIL_W, SETTINGS_DETAIL_H);
	lv_obj_set_style_bg_opa(view, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(view, 0, 0);
	lv_obj_set_style_radius(view, 0, 0);
	lv_obj_set_style_pad_all(view, 0, 0);
	lv_obj_set_style_shadow_width(view, 0, 0);
	clear_static_flags(view);
	return view;
}

static lv_obj_t *create_sidebar_button(lv_obj_t *parent, lv_coord_t y,
				       const char *title, settings_section_t section)
{
	const settings_theme_s *theme = settings_theme();
	lv_obj_t *btn = lv_btn_create(parent);
	lv_obj_t *title_label = lv_label_create(btn);

	lv_obj_set_pos(btn, 20, y);
	lv_obj_set_size(btn, SETTINGS_SIDEBAR_W - 40, 78);
	lv_obj_set_style_radius(btn, 20, 0);
	lv_obj_set_style_bg_color(btn, lv_color_hex(theme->panel_alt), 0);
	lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
	lv_obj_set_style_border_color(btn, lv_color_hex(theme->line), 0);
	lv_obj_set_style_border_width(btn, 1, 0);
	lv_obj_set_style_shadow_width(btn, 0, 0);
	lv_obj_set_style_outline_width(btn, 0, LV_STATE_FOCUSED);
	lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_event_cb(btn, settings_section_event_cb, LV_EVENT_CLICKED,
			    (void *)(uintptr_t)section);

	lv_obj_center(title_label);
	lv_obj_set_style_text_font(title_label, &lv_font_montserrat_24, 0);
	lv_obj_set_style_text_color(title_label, lv_color_hex(theme->text), 0);
	lv_label_set_text(title_label, title);

	g_section_buttons[section] = btn;
	g_section_titles[section] = title_label;
	return btn;
}

static void settings_set_notice(const char *text, uint32_t color)
{
	if (text == NULL) {
		return;
	}

	strncpy(g_settings.notice_text, text, sizeof(g_settings.notice_text) - 1U);
	g_settings.notice_text[sizeof(g_settings.notice_text) - 1U] = '\0';
	g_settings.notice_color = color;
}

static void settings_set_section_notice(uint32_t section)
{
	if (section == SETTINGS_SECTION_NETWORK) {
		settings_set_notice("Network is selected. Toggle traffic state and review propagation on the right.",
				    COLOR_DIM);
		return;
	}

	settings_set_notice("Perference is selected. Adjust theme mode, lock timing, and desktop color on the right.",
			    COLOR_DIM);
}

static uint64_t settings_peek_service_pool(uint64_t service_id)
{
	return OSIpcEndPointCall3(IPC_NAME_SERVICE_ENDPOINT_CREF,
				  IPC_NAME_SERVICE_FUNCTION_GET_SERVICE_POOL,
				  service_id);
}

static uint8_t settings_statemgr_get_u64(const char *key, uint32_t expected_type,
					 uint64_t *value_out)
{
	statemgr_get_response_s response = {0};

	if (key == NULL || value_out == NULL || !g_settings.statemgr_ready ||
	    g_statemgr == NULL || g_statemgr->ops.get == NULL) {
		return 0U;
	}

	if (g_statemgr->ops.get(g_statemgr, key, &response) == 0U || !response.found) {
		return 0U;
	}
	if (response.entry.type != expected_type) {
		return 0U;
	}

	*value_out = response.entry.value_u64;
	return 1U;
}

static uint64_t settings_statemgr_set_u64(const char *key, uint32_t type, uint64_t value)
{
	statemgr_entry_s entry = {0};

	if (key == NULL || !g_settings.statemgr_ready ||
	    g_statemgr == NULL || g_statemgr->ops.set == NULL) {
		return 0U;
	}

	strncpy(entry.key, key, STATEMGR_KEY_MAX - 1U);
	entry.type = type;
	entry.value_u64 = value;
	return g_statemgr->ops.set(g_statemgr, &entry);
}

static void settings_update_status_chip(void)
{
	const settings_theme_s *theme = settings_theme();
	const char *text = "Live";
	uint32_t bg = theme->accent_soft;
	uint32_t fg = theme->accent;

	if (!g_settings.statemgr_ready && !g_settings.net_ready) {
		text = "Offline";
		bg = theme->warm_soft;
		fg = theme->warm;
	} else if (!g_settings.statemgr_ready || !g_settings.net_ready) {
		text = "Limited";
		bg = theme->warm_soft;
		fg = theme->warm;
	}

	if (g_status_chip != NULL) {
		lv_label_set_text(g_status_chip, text);
		lv_obj_set_style_bg_color(g_status_chip, lv_color_hex(bg), 0);
		lv_obj_set_style_text_color(g_status_chip, lv_color_hex(fg), 0);
	}
}

static void settings_update_navigation(void)
{
	const settings_theme_s *theme = settings_theme();

	for (uint32_t i = 0; i < SETTINGS_SECTION_COUNT; i++) {
		uint8_t active = g_settings.selected_section == i;

		if (g_section_buttons[i] != NULL) {
			lv_obj_set_style_bg_color(g_section_buttons[i],
						  lv_color_hex(active ? theme->accent_soft : theme->panel_alt), 0);
			lv_obj_set_style_border_color(g_section_buttons[i],
						      lv_color_hex(active ? theme->accent : theme->line), 0);
			lv_obj_set_style_shadow_width(g_section_buttons[i], active ? 12 : 0, 0);
			lv_obj_set_style_shadow_color(g_section_buttons[i], lv_color_hex(theme->accent), 0);
			lv_obj_set_style_shadow_opa(g_section_buttons[i],
						    active ? LV_OPA_20 : LV_OPA_TRANSP, 0);
		}
		if (g_section_titles[i] != NULL) {
			lv_obj_set_style_text_color(g_section_titles[i],
						    lv_color_hex(active ? theme->text : theme->text), 0);
		}
	}

	if (g_personalization_view != NULL) {
		if (g_settings.selected_section == SETTINGS_SECTION_PERSONALIZATION) {
			lv_obj_clear_flag(g_personalization_view, LV_OBJ_FLAG_HIDDEN);
		} else {
			lv_obj_add_flag(g_personalization_view, LV_OBJ_FLAG_HIDDEN);
		}
	}
	if (g_network_view != NULL) {
		if (g_settings.selected_section == SETTINGS_SECTION_NETWORK) {
			lv_obj_clear_flag(g_network_view, LV_OBJ_FLAG_HIDDEN);
		} else {
			lv_obj_add_flag(g_network_view, LV_OBJ_FLAG_HIDDEN);
		}
	}
}

static void settings_update_theme_buttons(void)
{
	const settings_theme_s *theme = settings_theme();
	for (uint32_t i = 0; i < 2U; i++) {
		uint8_t active = g_settings.theme_mode == i;
		lv_obj_t *btn = g_theme_buttons[i];
		lv_obj_t *label = NULL;

		if (btn == NULL) {
			continue;
		}

		label = lv_obj_get_child(btn, 0);
		lv_obj_set_style_bg_color(btn,
					  lv_color_hex(active ? theme->accent_soft : theme->panel_alt),
					  0);
		lv_obj_set_style_border_color(btn,
					      lv_color_hex(active ? theme->accent : theme->line),
					      0);
		if (label != NULL) {
			lv_obj_set_style_text_color(label,
						    lv_color_hex(active ? theme->accent : theme->text),
						    0);
		}
	}
}

static void settings_update_swatch_buttons(void)
{
	for (uint32_t i = 0; i < SETTINGS_SWATCH_COUNT; i++) {
		lv_obj_t *btn = g_swatch_buttons[i];
		lv_obj_t *label = NULL;
		uint8_t selected = g_settings.desktop_color == g_swatch_specs[i].color;
		uint32_t label_color = g_swatch_specs[i].color < 0x888888U ? 0xf3f7fb : COLOR_TEXT;
		const settings_theme_s *theme = settings_theme();

		if (btn == NULL) {
			continue;
		}

		label = lv_obj_get_child(btn, 0);
		lv_obj_set_style_border_color(btn,
					      lv_color_hex(selected ? theme->accent : theme->line),
					      0);
		lv_obj_set_style_border_width(btn, selected ? 2 : 1, 0);
		lv_obj_set_style_shadow_width(btn, selected ? 10 : 0, 0);
		lv_obj_set_style_shadow_color(btn, lv_color_hex(theme->accent), 0);
		lv_obj_set_style_shadow_opa(btn, selected ? LV_OPA_20 : LV_OPA_TRANSP, 0);
		if (label != NULL) {
			lv_obj_set_style_text_color(label, lv_color_hex(label_color), 0);
		}
	}
}

static void settings_update_timeout_buttons(void)
{
	const settings_theme_s *theme = settings_theme();
	char meta_text[96] = {0};

	for (uint32_t i = 0; i < SETTINGS_IDLE_TIMEOUT_OPTION_COUNT; i++) {
		uint8_t active = g_settings.lock_timeout_seconds == g_timeout_options[i].seconds;
		lv_obj_t *btn = g_timeout_buttons[i];
		lv_obj_t *label = NULL;

		if (btn == NULL) {
			continue;
		}

		label = lv_obj_get_child(btn, 0);
		lv_obj_set_style_bg_color(btn,
					  lv_color_hex(active ? theme->accent_soft : theme->panel_alt),
					  0);
		lv_obj_set_style_border_color(btn,
					      lv_color_hex(active ? theme->accent : theme->line),
					      0);
		if (label != NULL) {
			lv_obj_set_style_text_color(label,
						    lv_color_hex(active ? theme->accent : theme->text),
						    0);
		}
	}

	if (g_lock_timeout_meta != NULL) {
		snprintf(meta_text, sizeof(meta_text),
			 "Lock screen appears after %u seconds without mouse, keyboard, or touch input.",
			 (unsigned int)g_settings.lock_timeout_seconds);
		lv_label_set_text(g_lock_timeout_meta, meta_text);
		lv_obj_set_style_text_color(g_lock_timeout_meta, lv_color_hex(theme->dim), 0);
	}
}

static void settings_update_network_section(void)
{
	const settings_theme_s *theme = settings_theme();
	const char *chip_text = g_settings.network_enabled ? "Online" : "Paused";
	const char *button_text = g_settings.network_enabled ? "Turn network off" : "Turn network on";
	uint32_t chip_bg = g_settings.network_enabled ? theme->accent_soft : theme->dark_soft;
	uint32_t chip_fg = g_settings.network_enabled ? theme->accent : theme->dark;
	const char *state_text = g_settings.network_enabled ? "Traffic online" : "Traffic paused";
	const char *state_meta = g_settings.network_enabled ?
		"Packet and HTTP traffic are flowing through Netmgr for this session." :
		"Packet and HTTP traffic are paused until you re-enable the network.";
	const char *scope_text = g_settings.net_ready ?
		"Netmgr is connected. Changes apply immediately." :
		"Netmgr is offline. Statemgr keeps the change queued until the service is back.";

	if (!g_settings.net_ready) {
		chip_text = g_settings.network_enabled ? "Queued on" : "Queued off";
		chip_bg = theme->warm_soft;
		chip_fg = theme->warm;
		state_text = g_settings.network_enabled ? "Queueing online" : "Queueing pause";
	}

	if (g_network_chip != NULL) {
		lv_label_set_text(g_network_chip, chip_text);
		lv_obj_set_style_bg_color(g_network_chip, lv_color_hex(chip_bg), 0);
		lv_obj_set_style_text_color(g_network_chip, lv_color_hex(chip_fg), 0);
	}
	if (g_network_button != NULL) {
		lv_obj_set_style_bg_color(g_network_button,
					  lv_color_hex(theme->panel_alt),
					  0);
		lv_obj_set_style_border_color(g_network_button,
					      lv_color_hex(g_settings.net_ready ? theme->line : theme->warm),
					      0);
	}
	if (g_network_button_label != NULL) {
		lv_label_set_text(g_network_button_label, button_text);
		lv_obj_set_style_text_color(g_network_button_label, lv_color_hex(theme->text), 0);
	}
	if (g_network_state_value != NULL) {
		lv_label_set_text(g_network_state_value, state_text);
		lv_obj_set_style_text_color(g_network_state_value, lv_color_hex(theme->text), 0);
	}
	if (g_network_state_meta != NULL) {
		lv_label_set_text(g_network_state_meta, state_meta);
		lv_obj_set_style_text_color(g_network_state_meta, lv_color_hex(theme->dim), 0);
	}
	if (g_network_scope_meta != NULL) {
		lv_label_set_text(g_network_scope_meta, scope_text);
		lv_obj_set_style_text_color(g_network_scope_meta, lv_color_hex(theme->text), 0);
	}
}

static void settings_update_preview(void)
{
	char preview_meta[96] = {0};
	uint32_t shell_bg = g_settings.theme_mode == SYSTEMUI_THEME_MODE_DARK ? 0x192737 : 0xffffff;
	uint32_t bar_bg = g_settings.theme_mode == SYSTEMUI_THEME_MODE_DARK ? 0x243447 : 0xf1f6fb;
	uint32_t title_color = g_settings.theme_mode == SYSTEMUI_THEME_MODE_DARK ? 0xeaf3fb : COLOR_TEXT;
	uint32_t meta_color = g_settings.theme_mode == SYSTEMUI_THEME_MODE_DARK ? 0x9bb0c4 : COLOR_DIM;

	if (g_preview_stage != NULL) {
		lv_obj_set_style_bg_color(g_preview_stage, lv_color_hex(g_settings.desktop_color), 0);
	}
	if (g_preview_shell != NULL) {
		lv_obj_set_style_bg_color(g_preview_shell, lv_color_hex(shell_bg), 0);
		lv_obj_set_style_border_color(g_preview_shell, lv_color_hex(g_settings.theme_mode == SYSTEMUI_THEME_MODE_DARK ?
								      0x31465a : COLOR_LINE), 0);
	}
	if (g_preview_bar != NULL) {
		lv_obj_set_style_bg_color(g_preview_bar, lv_color_hex(bar_bg), 0);
	}
	if (g_preview_title != NULL) {
		lv_obj_set_style_text_color(g_preview_title, lv_color_hex(title_color), 0);
		lv_label_set_text(g_preview_title,
				  g_settings.theme_mode == SYSTEMUI_THEME_MODE_DARK ?
				  "Dark desktop" : "Light desktop");
	}
	if (g_preview_meta != NULL) {
		lv_obj_set_style_text_color(g_preview_meta, lv_color_hex(meta_color), 0);
		snprintf(preview_meta, sizeof(preview_meta),
			 "Theme, desktop color, and auto-lock (%us) apply across the shell.",
			 (unsigned int)g_settings.lock_timeout_seconds);
		lv_label_set_text(g_preview_meta, preview_meta);
	}
}

static void settings_sync_from_services(void)
{
	uint64_t value = 0;

	if (g_statemgr == NULL || g_statemgr->pool_cref == 0U) {
		g_statemgr = statemgr_client_get();
	}
	g_settings.statemgr_ready = (g_statemgr != NULL &&
				     g_statemgr->pool_cref != 0U &&
				     g_statemgr->pool_cref != IPC_NAME_SERVICE_ENDPOINT_CREF) ? 1U : 0U;

	if (g_settings.net_pool_cref == 0U) {
		g_settings.net_pool_cref = settings_peek_service_pool(IPC_NET_SERVICE_ID);
	}
	g_settings.net_ready = (g_settings.net_pool_cref != 0U &&
				g_settings.net_pool_cref != IPC_NAME_SERVICE_ENDPOINT_CREF) ? 1U : 0U;

	if (settings_statemgr_get_u64("ui.theme.mode",
				      STATEMGR_VALUE_TYPE_U32, &value)) {
		g_settings.theme_mode = (uint32_t)value;
	}
	if (settings_statemgr_get_u64("ui.desktop.color",
				      STATEMGR_VALUE_TYPE_U32, &value)) {
		g_settings.desktop_color = (uint32_t)value;
	}
	if (settings_statemgr_get_u64(SETTINGS_STATE_KEY_IDLE_LOCK_TIMEOUT_SEC,
				      STATEMGR_VALUE_TYPE_U32, &value)) {
		g_settings.lock_timeout_seconds = settings_normalize_timeout_seconds((uint32_t)value);
	}

	if (settings_statemgr_get_u64("net.enabled",
				      STATEMGR_VALUE_TYPE_BOOL, &value)) {
		g_settings.network_enabled = value ? 1U : 0U;
	}
}

static uint32_t settings_sync_theme_to_statemgr(uint32_t theme_mode)
{
	if (!g_settings.statemgr_ready) {
		return SETTINGS_STATEMGR_SYNC_OFFLINE;
	}
	if (settings_statemgr_set_u64("ui.theme.mode", STATEMGR_VALUE_TYPE_U32,
				      theme_mode) == 0U) {
		return SETTINGS_STATEMGR_SYNC_FAILED;
	}

	return SETTINGS_STATEMGR_SYNC_OK;
}

static uint32_t settings_sync_color_to_statemgr(uint32_t color)
{
	if (!g_settings.statemgr_ready) {
		return SETTINGS_STATEMGR_SYNC_OFFLINE;
	}
	if (settings_statemgr_set_u64("ui.desktop.color", STATEMGR_VALUE_TYPE_U32,
				      color) == 0U) {
		return SETTINGS_STATEMGR_SYNC_FAILED;
	}

	return SETTINGS_STATEMGR_SYNC_OK;
}

static uint32_t settings_sync_timeout_to_statemgr(uint32_t seconds)
{
	if (!g_settings.statemgr_ready) {
		return SETTINGS_STATEMGR_SYNC_OFFLINE;
	}
	if (settings_statemgr_set_u64(SETTINGS_STATE_KEY_IDLE_LOCK_TIMEOUT_SEC,
				      STATEMGR_VALUE_TYPE_U32, seconds) == 0U) {
		return SETTINGS_STATEMGR_SYNC_FAILED;
	}

	return SETTINGS_STATEMGR_SYNC_OK;
}

static uint32_t settings_sync_network_to_statemgr(uint8_t enabled)
{
	if (!g_settings.statemgr_ready) {
		return SETTINGS_STATEMGR_SYNC_OFFLINE;
	}
	if (settings_statemgr_set_u64("net.enabled", STATEMGR_VALUE_TYPE_BOOL,
				      enabled ? 1U : 0U) == 0U) {
		return SETTINGS_STATEMGR_SYNC_FAILED;
	}

	return SETTINGS_STATEMGR_SYNC_OK;
}

static void settings_apply_controls(void)
{
	settings_update_status_chip();
	settings_update_navigation();
	settings_update_theme_buttons();
	settings_update_swatch_buttons();
	settings_update_timeout_buttons();
	settings_update_network_section();
	settings_update_preview();
}

static void settings_rebuild_ui(void)
{
	lv_obj_t *scr = lv_scr_act();

	g_status_chip = NULL;
	memset(g_section_buttons, 0, sizeof(g_section_buttons));
	memset(g_section_titles, 0, sizeof(g_section_titles));
	memset(g_theme_buttons, 0, sizeof(g_theme_buttons));
	memset(g_swatch_buttons, 0, sizeof(g_swatch_buttons));
	memset(g_timeout_buttons, 0, sizeof(g_timeout_buttons));
	g_network_button = NULL;
	g_network_button_label = NULL;
	g_network_chip = NULL;
	g_personalization_view = NULL;
	g_network_view = NULL;
	g_preview_stage = NULL;
	g_preview_shell = NULL;
	g_preview_bar = NULL;
	g_preview_title = NULL;
	g_preview_meta = NULL;
	g_lock_timeout_meta = NULL;
	g_network_state_value = NULL;
	g_network_state_meta = NULL;
	g_network_scope_meta = NULL;
	lv_obj_clean(scr);
	create_ui();
	settings_apply_controls();
}

static void settings_apply_theme_mode(uint32_t theme_mode)
{
	uint32_t sync_status = SETTINGS_STATEMGR_SYNC_OFFLINE;

	if (!g_settings.statemgr_ready) {
		settings_set_notice("State service is offline. Theme change was not applied.", COLOR_WARM);
		return;
	}

	g_settings.theme_mode = theme_mode;
	sync_status = settings_sync_theme_to_statemgr(theme_mode);
	settings_rebuild_ui();
	if (sync_status == SETTINGS_STATEMGR_SYNC_OK) {
		settings_set_notice(theme_mode == SYSTEMUI_THEME_MODE_DARK ?
				    "Dark theme written to Statemgr. SystemUI will pick it up." :
				    "Light theme written to Statemgr. SystemUI will pick it up.",
				    COLOR_ACCENT);
	} else {
		settings_set_notice(theme_mode == SYSTEMUI_THEME_MODE_DARK ?
				    "Dark theme update failed. Statemgr rejected the change." :
				    "Light theme update failed. Statemgr rejected the change.",
				    COLOR_WARM);
	}
}

static void settings_apply_desktop_color(uint32_t color)
{
	uint32_t sync_status = SETTINGS_STATEMGR_SYNC_OFFLINE;

	if (!g_settings.statemgr_ready) {
		settings_set_notice("State service is offline. Background color was not applied.", COLOR_WARM);
		return;
	}

	g_settings.desktop_color = color;
	sync_status = settings_sync_color_to_statemgr(color);
	settings_apply_controls();
	if (sync_status == SETTINGS_STATEMGR_SYNC_OK) {
		settings_set_notice("Desktop background color written to Statemgr. Launcher will pick it up.",
				    COLOR_ACCENT);
	} else {
		settings_set_notice("Desktop background color update failed. Statemgr rejected the change.",
				    COLOR_WARM);
	}
}

static void settings_apply_lock_timeout(uint32_t seconds)
{
	uint32_t sync_status = SETTINGS_STATEMGR_SYNC_OFFLINE;
	uint32_t previous_seconds = g_settings.lock_timeout_seconds;

	if (!g_settings.statemgr_ready) {
		settings_set_notice("State service is offline. Lock screen delay was not applied.", COLOR_WARM);
		return;
	}

	g_settings.lock_timeout_seconds = settings_normalize_timeout_seconds(seconds);
	sync_status = settings_sync_timeout_to_statemgr(g_settings.lock_timeout_seconds);
	if (sync_status == SETTINGS_STATEMGR_SYNC_OK) {
		settings_apply_controls();
		if (g_settings.lock_timeout_seconds == 10U) {
			settings_set_notice("Lock screen delay set to 10 seconds.", COLOR_ACCENT);
		} else if (g_settings.lock_timeout_seconds == 15U) {
			settings_set_notice("Lock screen delay set to 15 seconds.", COLOR_ACCENT);
		} else {
			settings_set_notice("Lock screen delay set to 30 seconds.", COLOR_ACCENT);
		}
		return;
	}

	g_settings.lock_timeout_seconds = previous_seconds;
	settings_apply_controls();
	settings_set_notice("Lock screen delay update failed. Statemgr rejected the change.",
			    COLOR_WARM);
}

static void settings_toggle_network(void)
{
	uint32_t sync_status = SETTINGS_STATEMGR_SYNC_OFFLINE;
	uint8_t next_enabled = 0U;

	if (!g_settings.statemgr_ready) {
		settings_set_notice("State service is offline. Network change was not applied.", COLOR_WARM);
		return;
	}

	next_enabled = g_settings.network_enabled ? 0U : 1U;
	g_settings.network_enabled = next_enabled;
	sync_status = settings_sync_network_to_statemgr(g_settings.network_enabled);
	settings_apply_controls();
	if (sync_status == SETTINGS_STATEMGR_SYNC_OK) {
		if (g_settings.net_ready) {
			settings_set_notice(g_settings.network_enabled ?
					    "Network state written to Statemgr. Netmgr will pick it up." :
					    "Network state written to Statemgr. Netmgr will pick it up.",
					    g_settings.network_enabled ? COLOR_ACCENT : COLOR_DARK);
		} else {
			settings_set_notice(g_settings.network_enabled ?
					    "Network state written to Statemgr. Netmgr is offline, so the change is queued." :
					    "Network state written to Statemgr. Netmgr is offline, so the change is queued.",
					    COLOR_WARM);
		}
	} else {
		g_settings.network_enabled = g_settings.network_enabled ? 0U : 1U;
		settings_apply_controls();
		settings_set_notice(next_enabled ?
				    "Network enable update failed. Statemgr rejected the change." :
				    "Network pause update failed. Statemgr rejected the change.",
				    COLOR_WARM);
	}
}

static void create_ui(void)
{
	const settings_theme_s *theme = settings_theme();
	lv_obj_t *scr = lv_scr_act();
	lv_obj_t *title = NULL;
	lv_obj_t *subtitle = NULL;
	lv_obj_t *sidebar = NULL;
	lv_obj_t *detail = NULL;
	lv_obj_t *panel = NULL;
	lv_obj_t *label = NULL;

	lv_obj_set_style_bg_color(scr, lv_color_hex(theme->bg), 0);
	lv_obj_set_style_bg_grad_color(scr, lv_color_hex(theme->bg_alt), 0);
	lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, 0);
	lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
	lv_obj_set_style_pad_all(scr, 0, 0);
	lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

	create_chip(scr, CONTENT_X, CONTENT_Y + 4, "CONTROL", theme->accent_soft, theme->accent);
	g_status_chip = create_chip(scr, CONTENT_X + CONTENT_W - 92, CONTENT_Y + 6,
				    "Live", theme->accent_soft, theme->accent);

	title = lv_label_create(scr);
	lv_obj_set_pos(title, CONTENT_X, CONTENT_Y + 28);
	lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
	lv_obj_set_style_text_color(title, lv_color_hex(theme->text), 0);
	lv_label_set_text(title, "Settings");
	clear_static_flags(title);

	subtitle = lv_label_create(scr);
	lv_obj_set_pos(subtitle, CONTENT_X, CONTENT_Y + 74);
	lv_obj_set_width(subtitle, CONTENT_W - 120);
	lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(subtitle, lv_color_hex(theme->dim), 0);
	lv_label_set_text(subtitle, "Perference, lock timing, and network controls for this session.");
	clear_static_flags(subtitle);

	sidebar = create_panel(scr, SETTINGS_SIDEBAR_X, SETTINGS_SIDEBAR_Y,
			       SETTINGS_SIDEBAR_W, SETTINGS_SIDEBAR_H,
			       theme->panel, theme->line);
	detail = create_panel(scr, SETTINGS_DETAIL_X, SETTINGS_DETAIL_Y,
			      SETTINGS_DETAIL_W, SETTINGS_DETAIL_H,
			      theme->panel, theme->line);

	label = lv_label_create(sidebar);
	lv_obj_set_pos(label, 20, 20);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->text), 0);
	lv_label_set_text(label, "Sections");
	clear_static_flags(label);

	(void)create_sidebar_button(sidebar, 92, "Perference",
				    SETTINGS_SECTION_PERSONALIZATION);
	(void)create_sidebar_button(sidebar, 184, "Network",
				    SETTINGS_SECTION_NETWORK);

	g_personalization_view = create_content_view(detail);
	g_network_view = create_content_view(detail);

	label = lv_label_create(g_personalization_view);
	lv_obj_set_pos(label, 24, 22);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_32, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->text), 0);
	lv_label_set_text(label, "Perference");
	clear_static_flags(label);

	label = lv_label_create(g_personalization_view);
	lv_obj_set_pos(label, 24, 68);
	lv_obj_set_width(label, SETTINGS_DETAIL_W - 48);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->dim), 0);
	lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
	lv_label_set_text(label, "Theme mode, lock timing, and desktop accent color update immediately across the shell.");
	clear_static_flags(label);

	label = lv_label_create(g_personalization_view);
	lv_obj_set_pos(label, 24, 120);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->dim), 0);
	lv_label_set_text(label, "Theme");
	clear_static_flags(label);

	g_theme_buttons[0] = create_segment_button(g_personalization_view, 24, 150, 154, "Light",
						       settings_theme_event_cb,
						       (void *)(uintptr_t)SYSTEMUI_THEME_MODE_LIGHT);
	g_theme_buttons[1] = create_segment_button(g_personalization_view, 188, 150, 154, "Dark",
						       settings_theme_event_cb,
						       (void *)(uintptr_t)SYSTEMUI_THEME_MODE_DARK);

	label = lv_label_create(g_personalization_view);
	lv_obj_set_pos(label, 24, 222);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->dim), 0);
	lv_label_set_text(label, "Desktop color");
	clear_static_flags(label);

	for (uint32_t i = 0; i < SETTINGS_SWATCH_COUNT; i++) {
		lv_obj_t *btn = lv_btn_create(g_personalization_view);
		lv_obj_t *btn_label = lv_label_create(btn);

		g_swatch_buttons[i] = btn;
		lv_obj_set_pos(btn, 24 + (lv_coord_t)i * 108, 248);
		lv_obj_set_size(btn, 96, 56);
		lv_obj_set_style_radius(btn, 22, 0);
		lv_obj_set_style_bg_color(btn, lv_color_hex(g_swatch_specs[i].color), 0);
		lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
		lv_obj_set_style_border_color(btn, lv_color_hex(theme->line), 0);
		lv_obj_set_style_border_width(btn, 1, 0);
		lv_obj_set_style_shadow_width(btn, 0, 0);
		lv_obj_set_style_outline_width(btn, 0, LV_STATE_FOCUSED);
		lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
		lv_obj_add_event_cb(btn, settings_swatch_event_cb, LV_EVENT_CLICKED,
				    (void *)(uintptr_t)i);

		lv_label_set_text(btn_label, g_swatch_specs[i].label);
		lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_14, 0);
		lv_obj_center(btn_label);
	}

	label = lv_label_create(g_personalization_view);
	lv_obj_set_pos(label, 24, 480);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->dim), 0);
	lv_label_set_text(label, "Lock screen after");
	clear_static_flags(label);

	for (uint32_t i = 0; i < SETTINGS_IDLE_TIMEOUT_OPTION_COUNT; i++) {
		g_timeout_buttons[i] = create_segment_button(g_personalization_view,
							     24 + (lv_coord_t)i * 188, 506,
							     176, g_timeout_options[i].label,
							     settings_timeout_event_cb,
							     (void *)(uintptr_t)g_timeout_options[i].seconds);
	}

	g_lock_timeout_meta = lv_label_create(g_personalization_view);
	lv_obj_set_pos(g_lock_timeout_meta, 24, 566);
	lv_obj_set_width(g_lock_timeout_meta, SETTINGS_DETAIL_W - 48);
	lv_obj_set_style_text_font(g_lock_timeout_meta, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(g_lock_timeout_meta, lv_color_hex(theme->dim), 0);
	lv_label_set_long_mode(g_lock_timeout_meta, LV_LABEL_LONG_WRAP);
	lv_label_set_text(g_lock_timeout_meta, "");
	clear_static_flags(g_lock_timeout_meta);

	g_preview_stage = create_panel(g_personalization_view, 24, 332, SETTINGS_DETAIL_W - 48, 116,
				       g_settings.desktop_color, theme->line);
	g_preview_shell = create_panel(g_preview_stage, 18, 14, SETTINGS_DETAIL_W - 84, 88,
				       theme->panel, theme->line);
	g_preview_bar = lv_obj_create(g_preview_shell);
	lv_obj_set_pos(g_preview_bar, 0, 0);
	lv_obj_set_size(g_preview_bar, SETTINGS_DETAIL_W - 84, 18);
	lv_obj_set_style_radius(g_preview_bar, 20, 0);
	lv_obj_set_style_bg_color(g_preview_bar, lv_color_hex(theme->panel_alt), 0);
	lv_obj_set_style_bg_opa(g_preview_bar, LV_OPA_COVER, 0);
	lv_obj_set_style_border_width(g_preview_bar, 0, 0);
	clear_static_flags(g_preview_bar);

	g_preview_title = lv_label_create(g_preview_shell);
	lv_obj_set_pos(g_preview_title, 18, 30);
	lv_obj_set_style_text_font(g_preview_title, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(g_preview_title, lv_color_hex(theme->text), 0);
	lv_label_set_text(g_preview_title, "Light desktop");
	clear_static_flags(g_preview_title);

	g_preview_meta = lv_label_create(g_preview_shell);
	lv_obj_set_pos(g_preview_meta, 18, 52);
	lv_obj_set_width(g_preview_meta, SETTINGS_DETAIL_W - 128);
	lv_obj_set_style_text_font(g_preview_meta, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(g_preview_meta, lv_color_hex(theme->dim), 0);
	lv_label_set_text(g_preview_meta, "");
	clear_static_flags(g_preview_meta);

	label = lv_label_create(g_network_view);
	lv_obj_set_pos(label, 24, 22);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_32, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->text), 0);
	lv_label_set_text(label, "Network");
	clear_static_flags(label);

	g_network_chip = create_chip(g_network_view, SETTINGS_DETAIL_W - 108, 28, "Online",
				     theme->accent_soft, theme->accent);

	label = lv_label_create(g_network_view);
	lv_obj_set_pos(label, 24, 68);
	lv_obj_set_width(label, SETTINGS_DETAIL_W - 48);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->dim), 0);
	lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
	lv_label_set_text(label, "Pause packet and HTTP traffic without leaving the desktop. The MAC identity remains visible.");
	clear_static_flags(label);

	panel = create_panel(g_network_view, 24, 128, SETTINGS_DETAIL_W - 48, 126,
			     theme->panel_alt, theme->line);
	label = lv_label_create(panel);
	lv_obj_set_pos(label, 22, 20);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->dim), 0);
	lv_label_set_text(label, "Traffic");
	clear_static_flags(label);

	g_network_state_value = lv_label_create(panel);
	lv_obj_set_pos(g_network_state_value, 22, 42);
	lv_obj_set_style_text_font(g_network_state_value, &lv_font_montserrat_24, 0);
	lv_obj_set_style_text_color(g_network_state_value, lv_color_hex(theme->text), 0);
	lv_label_set_text(g_network_state_value, "Traffic online");
	clear_static_flags(g_network_state_value);

	g_network_state_meta = lv_label_create(panel);
	lv_obj_set_pos(g_network_state_meta, 22, 62);
	lv_obj_set_width(g_network_state_meta, SETTINGS_DETAIL_W - 92);
	lv_obj_set_style_text_font(g_network_state_meta, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(g_network_state_meta, lv_color_hex(theme->dim), 0);
	lv_label_set_long_mode(g_network_state_meta, LV_LABEL_LONG_WRAP);
	lv_label_set_text(g_network_state_meta, "");
	clear_static_flags(g_network_state_meta);

	g_network_button = lv_btn_create(g_network_view);
	lv_obj_set_pos(g_network_button, 24, 274);
	lv_obj_set_size(g_network_button, SETTINGS_DETAIL_W - 48, 44);
	lv_obj_set_style_radius(g_network_button, 20, 0);
	lv_obj_set_style_bg_color(g_network_button, lv_color_hex(theme->panel_alt), 0);
	lv_obj_set_style_bg_opa(g_network_button, LV_OPA_COVER, 0);
	lv_obj_set_style_border_color(g_network_button, lv_color_hex(theme->line), 0);
	lv_obj_set_style_border_width(g_network_button, 1, 0);
	lv_obj_set_style_shadow_width(g_network_button, 0, 0);
	lv_obj_set_style_outline_width(g_network_button, 0, LV_STATE_FOCUSED);
	lv_obj_clear_flag(g_network_button, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_event_cb(g_network_button, settings_network_event_cb, LV_EVENT_CLICKED, NULL);

	g_network_button_label = lv_label_create(g_network_button);
	lv_label_set_text(g_network_button_label, "Turn network off");
	lv_obj_set_style_text_font(g_network_button_label, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(g_network_button_label, lv_color_hex(theme->text), 0);
	lv_obj_center(g_network_button_label);

	panel = create_panel(g_network_view, 24, 338, SETTINGS_DETAIL_W - 48, 78,
			     theme->panel_alt, theme->line);
	label = lv_label_create(panel);
	lv_obj_set_pos(label, 16, 12);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->dim), 0);
	lv_label_set_text(label, "Propagation");
	clear_static_flags(label);

	g_network_scope_meta = lv_label_create(panel);
	lv_obj_set_pos(g_network_scope_meta, 16, 36);
	lv_obj_set_width(g_network_scope_meta, SETTINGS_DETAIL_W - 96);
	lv_obj_set_style_text_font(g_network_scope_meta, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(g_network_scope_meta, lv_color_hex(theme->text), 0);
	lv_label_set_long_mode(g_network_scope_meta, LV_LABEL_LONG_WRAP);
	lv_label_set_text(g_network_scope_meta, "");
	clear_static_flags(g_network_scope_meta);

	lv_obj_set_height(g_preview_stage, 116);
	lv_obj_set_height(g_preview_shell, 88);
	lv_obj_set_size(g_preview_bar, SETTINGS_DETAIL_W - 84, 18);
	lv_obj_set_pos(g_preview_title, 18, 30);
	lv_obj_set_pos(g_preview_meta, 18, 52);
	lv_obj_set_width(g_preview_meta, SETTINGS_DETAIL_W - 128);
}

static void settings_section_event_cb(lv_event_t *e)
{
	uint32_t section = (uint32_t)(uintptr_t)lv_event_get_user_data(e);

	if (lv_event_get_code(e) != LV_EVENT_CLICKED || section >= SETTINGS_SECTION_COUNT) {
		return;
	}
	if (g_settings.selected_section == section) {
		return;
	}

	g_settings.selected_section = section;
	settings_apply_controls();
}

static void settings_theme_event_cb(lv_event_t *e)
{
	uint32_t theme_mode = (uint32_t)(uintptr_t)lv_event_get_user_data(e);

	if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
		return;
	}
	settings_apply_theme_mode(theme_mode);
}

static void settings_swatch_event_cb(lv_event_t *e)
{
	uint32_t index = (uint32_t)(uintptr_t)lv_event_get_user_data(e);

	if (lv_event_get_code(e) != LV_EVENT_CLICKED || index >= SETTINGS_SWATCH_COUNT) {
		return;
	}
	settings_apply_desktop_color(g_swatch_specs[index].color);
}

static void settings_timeout_event_cb(lv_event_t *e)
{
	uint32_t seconds = (uint32_t)(uintptr_t)lv_event_get_user_data(e);

	if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
		return;
	}
	if (g_settings.lock_timeout_seconds == settings_normalize_timeout_seconds(seconds)) {
		return;
	}

	settings_apply_lock_timeout(seconds);
}

static void settings_network_event_cb(lv_event_t *e)
{
	if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
		return;
	}
	settings_toggle_network();
}

static void settings_refresh(uint64_t mono_ms, uint8_t force)
{
	uint32_t previous_theme_mode = g_settings.theme_mode;

	if (!force && mono_ms < g_settings.last_refresh_ms + REFRESH_INTERVAL_MS) {
		return;
	}

	g_settings.last_refresh_ms = mono_ms;
	settings_sync_from_services();
	if (previous_theme_mode != g_settings.theme_mode) {
		settings_rebuild_ui();
		return;
	}
	settings_apply_controls();
}

static void settings_on_create(app_s *app)
{
	(void)app;

	g_statemgr = statemgr_client_get();
	settings_sync_from_services();
	settings_set_notice("Select a section to adjust this session.", COLOR_DIM);

	create_ui();
	settings_refresh(0U, 1U);
	log_info("settings ready\n");
}

static void settings_on_update(app_s *app, uint64_t mono_ms)
{
	(void)app;
	settings_refresh(mono_ms, 0U);
}

int main(int argc, char *argv[])
{
	app_s app = {0};
	app_config_s config = {
		.name = "settings",
		.x = APP_DEFAULT_X,
		.y = APP_DEFAULT_Y,
		.z = 0,
		.width = APP_DEFAULT_WIDTH,
		.height = APP_DEFAULT_HEIGHT,
		.window_flags = WINDOW_FLAG_FOCUSABLE,
		.enable_input = 1,
	};
	app_lifecycle_ops_s lifecycle = {
		.on_create = settings_on_create,
		.on_update = settings_on_update,
	};

	(void)argc;
	(void)argv;

	log_info("settings start!\n");
	app_init(&app, &config, &lifecycle, NULL);
	return app_run(&app);
}
