#include "lvgl.h"
#include "app.h"
#include "liblvgl/lv_port_disp.h"
#include "libwindow/window.h"
#include "log.h"
#include "libkernel/capcall.h"
#include "libsystem/appmgr_client.h"
#include "libsystem/fs_client.h"
#include "libsystem/ipc.h"
#include "libsystem/process_loader.h"
#include "libsystem/statemgr_client.h"
#include "libsystem/systemd_client.h"
#include "stdio.h"
#include "string.h"
#include "stdint.h"

#define COLOR_BG           0xfff4ee
#define COLOR_BG_ALT       0xffe5d8
#define COLOR_PANEL        0xfffffb
#define COLOR_PANEL_ALT    0xffefe7
#define COLOR_LINE         0xf0ddd2
#define COLOR_TEXT         0x24324a
#define COLOR_DIM          0x7e6d68
#define COLOR_ACCENT       0xff7d5c
#define COLOR_ACCENT_ALT   0x57c2d1
#define COLOR_ACCENT_SOFT  0xffe6db
#define COLOR_WARM_SOFT    0xffefd1
#define COLOR_SUCCESS_SOFT 0xe7faef
#define COLOR_SUCCESS      0x44be8b
#define COLOR_WARN         0xffb458
#define BOOT_ANIMATION_NOTIFY_RETRY_MS 250ULL
#define LAUNCHER_APPEARANCE_REFRESH_INTERVAL_MS 100ULL
#define LAUNCHER_APPS_PER_PAGE 8U
#define LAUNCHER_AGENT_TASK_COUNT 3U
#define LAUNCHER_APP_PAGE_COUNT_MAX ((APPMGR_APP_LIST_MAX + LAUNCHER_APPS_PER_PAGE - 1U) / LAUNCHER_APPS_PER_PAGE)
#define LAUNCHER_PAGE_COUNT_MAX (LAUNCHER_APP_PAGE_COUNT_MAX + 1U)
#define LAUNCHER_CARD_X 40
#define LAUNCHER_CARD_Y 24
#define LAUNCHER_CARD_W 944
#define LAUNCHER_CARD_H 796
#define LAUNCHER_PAGE_INSET 28
#define LAUNCHER_PAGE_COL_GAP 24
#define LAUNCHER_PAGE_COL_W 432
#define LAUNCHER_PAGE_TOP_ROW_Y 74
#define LAUNCHER_PAGE_TOP_ROW_H 340
#define LAUNCHER_PAGE_BOTTOM_ROW_Y 438
#define LAUNCHER_PAGE_BOTTOM_ROW_H 262
#define LAUNCHER_HEADER_X 28
#define LAUNCHER_HEADER_TITLE_Y 52
#define LAUNCHER_HEADER_TITLE_W 520
#define LAUNCHER_HEADER_SUMMARY_Y 64
#define LAUNCHER_HEADER_SUMMARY_W 640
#define LAUNCHER_PAGE_VIEW_X 28
#define LAUNCHER_PAGE_VIEW_Y 84
#define LAUNCHER_PAGE_VIEW_W 888
#define LAUNCHER_PAGE_VIEW_H 576
#define LAUNCHER_PAGE_GAP 24
#define LAUNCHER_PAGE_STRIDE (LAUNCHER_CARD_W + LAUNCHER_PAGE_GAP)
#define LAUNCHER_TILE_W 436
#define LAUNCHER_TILE_H 132
#define LAUNCHER_TILE_BADGE_MARGIN 18
#define LAUNCHER_TILE_BADGE_SIZE 76
#define LAUNCHER_TILE_COL_GAP 16
#define LAUNCHER_TILE_ROW_GAP 16
#define LAUNCHER_PAGE_INDICATOR_Y (LAUNCHER_CARD_Y + LAUNCHER_CARD_H - LAUNCHER_PAGE_INSET - LAUNCHER_PAGE_INDICATOR_H)
#define LAUNCHER_PAGE_INDICATOR_H 24
#define LAUNCHER_PAGE_INDICATOR_SIDE_PAD 18
#define LAUNCHER_PAGE_DOT_SLOT_W 24
#define LAUNCHER_PAGE_DOT_H 8
#define LAUNCHER_PAGE_DOT_ACTIVE_W 24
#define LAUNCHER_PAGE_DOT_INACTIVE_W 8
#define DESKTOP_COLOR_DEFAULT 0xffefe3U
#define LAUNCHER_APP_SLOT_EMPTY 0xffffffffU

typedef enum launcher_theme_mode {
	LAUNCHER_THEME_LIGHT = 0,
	LAUNCHER_THEME_DARK = 1,
} launcher_theme_mode_e;

typedef struct launcher_theme {
	uint32_t panel;
	uint32_t panel_alt;
	uint32_t line;
	uint32_t text;
	uint32_t dim;
	uint32_t accent_soft;
	uint32_t warm_soft;
	uint32_t success_soft;
	uint32_t warn;
} launcher_theme_s;

typedef struct launcher_app_entry {
	char app_id[APPMGR_APP_ID_MAX];
	char process_path[APPMGR_PROCESS_PATH_MAX];
	char name[APPMGR_APP_NAME_MAX];
	char subtitle[APPMGR_APP_SUBTITLE_MAX];
	char recommendation[APPMGR_RECOMMENDATION_MAX];
	char badge_text[4];
	char icon_path[FS_PATH_MAX];
	lv_obj_t *tile;
	lv_obj_t *status_label;
	uint32_t accent_color;
	uint8_t ready;
} launcher_app_entry_s;

typedef enum launcher_mode {
	LAUNCHER_MODE_ASK = 0,
	LAUNCHER_MODE_PLAN,
	LAUNCHER_MODE_DO,
} launcher_mode_e;

#define PRIMARY_APP_INDEX 0

static systemd_client_s *g_systemd = NULL;
static fs_client_s *g_fs = NULL;
static appmgr_client_s *g_appmgr = NULL;
static statemgr_client_s *g_statemgr = NULL;
static launcher_app_entry_s *g_recommended_app = NULL;

static lv_obj_t *g_root_scr = NULL;
static lv_obj_t *g_launch_btn = NULL;
static lv_obj_t *g_launch_btn_label = NULL;
static lv_obj_t *g_hero_summary[LAUNCHER_PAGE_COUNT_MAX] = {0};
static lv_obj_t *g_ready_chip = NULL;
static lv_obj_t *g_proc_chip = NULL;
static lv_obj_t *g_mem_chip = NULL;
static lv_obj_t *g_task_summary = NULL;
static lv_obj_t *g_task_row_panel[LAUNCHER_AGENT_TASK_COUNT] = {0};
static lv_obj_t *g_task_status[LAUNCHER_AGENT_TASK_COUNT] = {0};
static lv_obj_t *g_task_title[LAUNCHER_AGENT_TASK_COUNT] = {0};
static lv_obj_t *g_task_body[LAUNCHER_AGENT_TASK_COUNT] = {0};
static lv_obj_t *g_memory_workspace = NULL;
static lv_obj_t *g_memory_live = NULL;
static lv_obj_t *g_app_pager = NULL;
static lv_obj_t *g_page_indicator_track = NULL;
static lv_obj_t *g_page_dots[LAUNCHER_PAGE_COUNT_MAX] = {0};

static launcher_app_entry_s g_apps[APPMGR_APP_LIST_MAX] = {0};
static uint32_t g_page_app_indices[LAUNCHER_APP_PAGE_COUNT_MAX][LAUNCHER_APPS_PER_PAGE] = {{0}};
static uint32_t g_app_count = 0;
static uint32_t g_app_page_count = 0;
static uint32_t g_page_count = 0;
static uint32_t g_active_page = 0;
static uint32_t g_installed_app_count = 0;
static uint32_t g_theme_mode = LAUNCHER_THEME_LIGHT;
static uint32_t g_desktop_color = DESKTOP_COLOR_DEFAULT;
static uint64_t g_last_appearance_ms = 0;
static uint64_t g_last_appearance_revision = 0;
static uint64_t g_last_metrics_ms = 0;
static uint8_t g_boot_animation_exit_requested = 0;
static uint64_t g_last_boot_animation_notify_ms = 0;

static void launch_app_event_cb(lv_event_t *e);
static void launch_primary_event_cb(lv_event_t *e);
static void launch_mode_event_cb(lv_event_t *e);
static void launcher_pager_event_cb(lv_event_t *e);
static void launcher_set_icon_path(launcher_app_entry_s *app, const char *path);
static void create_ui(void);
static void refresh_metrics(uint64_t mono_ms, uint8_t rescan_storage);
static launcher_app_entry_s *launcher_find_app_by_id(const char *app_id);
static launcher_app_entry_s *launcher_primary_action_app(void);
static launcher_app_entry_s *launcher_mode_target_app(launcher_mode_e mode);
static void launcher_update_intro_panel(void);

static const launcher_theme_s g_launcher_theme_light = {
	.panel = COLOR_PANEL,
	.panel_alt = COLOR_PANEL_ALT,
	.line = COLOR_LINE,
	.text = COLOR_TEXT,
	.dim = COLOR_DIM,
	.accent_soft = COLOR_ACCENT_SOFT,
	.warm_soft = COLOR_WARM_SOFT,
	.success_soft = COLOR_SUCCESS_SOFT,
	.warn = COLOR_WARN,
};

static const launcher_theme_s g_launcher_theme_dark = {
	.panel = 0x1f2434,
	.panel_alt = 0x2a3045,
	.line = 0x434b67,
	.text = 0xfff6ee,
	.dim = 0xc6b5ad,
	.accent_soft = 0x4a2f29,
	.warm_soft = 0x4b3928,
	.success_soft = 0x213d31,
	.warn = 0xf5c575,
};

static const launcher_theme_s *launcher_theme(void)
{
	return g_theme_mode == LAUNCHER_THEME_DARK ?
		&g_launcher_theme_dark : &g_launcher_theme_light;
}

static uint32_t launcher_mix_color(uint32_t from, uint32_t to, uint32_t numerator, uint32_t denominator)
{
	uint32_t from_r = (from >> 16) & 0xFFU;
	uint32_t from_g = (from >> 8) & 0xFFU;
	uint32_t from_b = from & 0xFFU;
	uint32_t to_r = (to >> 16) & 0xFFU;
	uint32_t to_g = (to >> 8) & 0xFFU;
	uint32_t to_b = to & 0xFFU;
	uint32_t out_r = 0;
	uint32_t out_g = 0;
	uint32_t out_b = 0;

	if (denominator == 0U) {
		return from;
	}

	out_r = (from_r * (denominator - numerator) + to_r * numerator) / denominator;
	out_g = (from_g * (denominator - numerator) + to_g * numerator) / denominator;
	out_b = (from_b * (denominator - numerator) + to_b * numerator) / denominator;
	return (out_r << 16) | (out_g << 8) | out_b;
}

static void launcher_apply_background(void)
{
	if (g_root_scr == NULL) {
		return;
	}

	lv_obj_set_style_bg_color(g_root_scr, lv_color_hex(g_desktop_color), 0);
	lv_obj_set_style_bg_grad_color(g_root_scr,
				       lv_color_hex(launcher_mix_color(g_desktop_color,
								      COLOR_BG_ALT, 1U, 2U)),
				       0);
	lv_obj_set_style_bg_grad_dir(g_root_scr, LV_GRAD_DIR_VER, 0);
	lv_obj_set_style_bg_opa(g_root_scr, LV_OPA_COVER, 0);
}

static void launcher_rebuild_ui(uint64_t mono_ms)
{
	lv_obj_clean(g_root_scr);
	create_ui();
	refresh_metrics(mono_ms, 1U);
}

static void launcher_refresh_appearance(uint64_t mono_ms, uint8_t force)
{
	statemgr_get_response_s response = {0};
	uint64_t revision = 0;
	uint32_t theme_mode = g_theme_mode;
	uint32_t desktop_color = g_desktop_color;
	uint8_t theme_changed = 0U;
	uint8_t color_changed = 0U;

	if (!force && mono_ms < g_last_appearance_ms + LAUNCHER_APPEARANCE_REFRESH_INTERVAL_MS) {
		return;
	}

	g_last_appearance_ms = mono_ms;
	if (g_statemgr == NULL) {
		g_statemgr = statemgr_client_get();
	}
	if (g_statemgr == NULL || g_statemgr->ops.get_revision == NULL ||
	    g_statemgr->ops.get == NULL) {
		return;
	}

	revision = g_statemgr->ops.get_revision(g_statemgr);
	if (revision == 0U || (!force && revision == g_last_appearance_revision)) {
		return;
	}

	if (g_statemgr->ops.get(g_statemgr, "ui.theme.mode", &response) != 0U &&
	    response.found && response.entry.type == STATEMGR_VALUE_TYPE_U32) {
		theme_mode = (uint32_t)response.entry.value_u64 == LAUNCHER_THEME_DARK ?
			LAUNCHER_THEME_DARK : LAUNCHER_THEME_LIGHT;
	}
	if (g_statemgr->ops.get(g_statemgr, "ui.desktop.color", &response) != 0U &&
	    response.found && response.entry.type == STATEMGR_VALUE_TYPE_U32) {
		desktop_color = (uint32_t)response.entry.value_u64;
	}

	g_last_appearance_revision = revision;
	theme_changed = theme_mode != g_theme_mode;
	color_changed = desktop_color != g_desktop_color;
	g_theme_mode = theme_mode;
	g_desktop_color = desktop_color;
	if (theme_changed) {
		launcher_rebuild_ui(mono_ms);
	} else if (color_changed) {
		launcher_apply_background();
	}
	if (theme_changed || color_changed) {
		log_info("launcher appearance synced: theme=%u color=0x%08x rev=%llu\n",
		 (unsigned int)g_theme_mode,
		 (unsigned int)g_desktop_color,
		 (unsigned long long)revision);
	}
}

static uint64_t launcher_try_get_boot_animation_service(void)
{
	return OSIpcEndPointCall3(IPC_NAME_SERVICE_ENDPOINT_CREF,
				  IPC_NAME_SERVICE_FUNCTION_GET_SERVICE_POOL,
				  IPC_BOOT_ANIMATION_SERVICE_ID);
}

static void launcher_notify_boot_animation_exit(uint64_t mono_ms)
{
	uint64_t pool_cref = 0;

	if (g_boot_animation_exit_requested) {
		return;
	}
	if (mono_ms != 0 &&
	    mono_ms - g_last_boot_animation_notify_ms < BOOT_ANIMATION_NOTIFY_RETRY_MS) {
		return;
	}

	pool_cref = launcher_try_get_boot_animation_service();
	if (pool_cref == 0 || pool_cref == IPC_NAME_SERVICE_ENDPOINT_CREF) {
		if (mono_ms != 0) {
			g_last_boot_animation_notify_ms = mono_ms;
		}
		return;
	}
	if (!OSIpcEndPointPoolCall2(pool_cref, IPC_BOOT_ANIMATION_SERVICE_FUNCTION_EXIT)) {
		log_warn("boot animation exit request failed\n");
		if (mono_ms != 0) {
			g_last_boot_animation_notify_ms = mono_ms;
		}
		return;
	}

	g_boot_animation_exit_requested = 1;
	g_last_boot_animation_notify_ms = mono_ms;
	log_info("boot animation exit requested\n");
}

static void clear_static_flags(lv_obj_t *obj)
{
	if (obj == NULL) {
		return;
	}
	lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CLICK_FOCUSABLE);
}

static void set_label_text(lv_obj_t *obj, const char *text, uint32_t color)
{
	if (obj == NULL || text == NULL) {
		return;
	}
	lv_label_set_text(obj, text);
	lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
}

static void set_label_text_group(lv_obj_t **labels, uint32_t count, const char *text, uint32_t color)
{
	if (labels == NULL || text == NULL) {
		return;
	}

	for (uint32_t i = 0; i < count; i++) {
		if (labels[i] == NULL) {
			continue;
		}
		set_label_text(labels[i], text, color);
	}
}

static void set_chip_text(lv_obj_t *obj, const char *text, uint32_t bg, uint32_t fg)
{
	if (obj == NULL || text == NULL) {
		return;
	}
	lv_label_set_text(obj, text);
	lv_obj_set_style_bg_color(obj, lv_color_hex(bg), 0);
	lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
	lv_obj_set_style_text_color(obj, lv_color_hex(fg), 0);
}

static lv_obj_t *create_text_label(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w,
				   const lv_font_t *font, uint32_t color,
				   lv_text_align_t align, const char *text)
{
	lv_obj_t *label = lv_label_create(parent);

	lv_obj_set_pos(label, x, y);
	if (w > 0) {
		lv_obj_set_width(label, w);
		lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
	}
	lv_obj_set_style_text_font(label, font, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
	lv_obj_set_style_text_align(label, align, 0);
	lv_label_set_text(label, text);
	clear_static_flags(label);
	return label;
}

static lv_obj_t *create_mode_button(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
				    lv_coord_t w, const char *text, uint32_t bg,
				    uint32_t border, uint32_t fg, launcher_mode_e mode)
{
	lv_obj_t *btn = lv_btn_create(parent);
	lv_obj_t *label = lv_label_create(btn);

	lv_obj_set_pos(btn, x, y);
	lv_obj_set_size(btn, w, 42);
	lv_obj_set_style_radius(btn, 18, 0);
	lv_obj_set_style_bg_color(btn, lv_color_hex(bg), 0);
	lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
	lv_obj_set_style_border_color(btn, lv_color_hex(border), 0);
	lv_obj_set_style_border_width(btn, 1, 0);
	lv_obj_set_style_shadow_width(btn, 0, 0);
	lv_obj_set_style_outline_width(btn, 0, LV_STATE_FOCUSED);
	lv_obj_add_event_cb(btn, launch_mode_event_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)mode);
	lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

	lv_label_set_text(label, text);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(fg), 0);
	lv_obj_center(label);
	return btn;
}

static lv_obj_t *create_note_row(lv_obj_t *parent, lv_coord_t y,
				 const char *kicker, uint32_t kicker_color,
				 const char *body, lv_coord_t body_width,
				 lv_obj_t **body_out, uint8_t add_divider)
{
	const launcher_theme_s *theme = launcher_theme();
	lv_obj_t *kicker_lbl = lv_label_create(parent);
	lv_obj_t *body_lbl = lv_label_create(parent);

	lv_obj_set_pos(kicker_lbl, 20, y);
	lv_obj_set_style_text_font(kicker_lbl, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(kicker_lbl, lv_color_hex(kicker_color), 0);
	lv_label_set_text(kicker_lbl, kicker);

	lv_obj_set_pos(body_lbl, 20, y + 16);
	lv_obj_set_width(body_lbl, body_width);
	lv_label_set_long_mode(body_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(body_lbl, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(body_lbl, lv_color_hex(theme->text), 0);
	lv_label_set_text(body_lbl, body);

	if (body_out != NULL) {
		*body_out = body_lbl;
	}

	if (add_divider) {
		lv_obj_t *line = lv_obj_create(parent);
		lv_obj_set_pos(line, 20, y + 46);
		lv_obj_set_size(line, body_width, 1);
		lv_obj_set_style_bg_color(line, lv_color_hex(theme->line), 0);
		lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
		lv_obj_set_style_border_width(line, 0, 0);
		lv_obj_set_style_radius(line, 0, 0);
		clear_static_flags(line);
	}

	return body_lbl;
}

static char launcher_upper_char(char ch)
{
	if (ch >= 'a' && ch <= 'z') {
		return (char)(ch - 'a' + 'A');
	}
	return ch;
}

static void launcher_make_display_name(const char *app_id, char *out, uint32_t cap)
{
	uint32_t j = 0;
	uint8_t upper_next = 1;

	if (out == NULL || cap == 0U) {
		return;
	}
	out[0] = '\0';
	if (app_id == NULL || app_id[0] == '\0') {
		strncpy(out, "Unknown", cap - 1U);
		return;
	}

	for (uint32_t i = 0; app_id[i] != '\0' && j + 1U < cap; i++) {
		char ch = app_id[i];

		if (ch == '_' || ch == '-' || ch == ' ') {
			if (j > 0U && out[j - 1U] != ' ' && j + 1U < cap) {
				out[j++] = ' ';
			}
			upper_next = 1;
			continue;
		}

		out[j++] = upper_next ? launcher_upper_char(ch) : ch;
		upper_next = 0;
	}
	out[j] = '\0';
}

static uint32_t launcher_hash_text(const char *text)
{
	uint32_t hash = 2166136261U;

	if (text == NULL) {
		return hash;
	}

	for (uint32_t i = 0; text[i] != '\0'; i++) {
		hash ^= (uint8_t)text[i];
		hash *= 16777619U;
	}

	return hash;
}

static uint32_t launcher_pick_accent(const char *app_id)
{
	static const uint32_t palette[] = {
		COLOR_ACCENT,
		COLOR_ACCENT_ALT,
		COLOR_WARN,
		COLOR_SUCCESS,
	};

	return palette[launcher_hash_text(app_id) % (sizeof(palette) / sizeof(palette[0]))];
}

static void launcher_fill_app_visuals(launcher_app_entry_s *app)
{
	if (app == NULL) {
		return;
	}

	if (app->name[0] == '\0') {
		launcher_make_display_name(app->app_id, app->name, sizeof(app->name));
	}
	if (app->subtitle[0] == '\0') {
		strncpy(app->subtitle, "Open this application.", sizeof(app->subtitle) - 1U);
	}
	app->badge_text[0] = launcher_upper_char(app->name[0] == '\0' ? '?' : app->name[0]);
	app->badge_text[1] = '\0';
	app->accent_color = launcher_pick_accent(app->app_id);
}

static void launcher_set_icon_path(launcher_app_entry_s *app, const char *path)
{
	if (app == NULL || path == NULL || path[0] == '\0') {
		return;
	}

	strncpy(app->icon_path, path, sizeof(app->icon_path) - 1U);
}

static void launcher_build_page_map(void)
{
	memset(g_page_app_indices, 0xFF, sizeof(g_page_app_indices));
	g_app_page_count = 0;
	g_page_count = 1U;
	g_active_page = 0U;
	if (g_app_count == 0U) {
		return;
	}

	g_app_page_count = (g_app_count + LAUNCHER_APPS_PER_PAGE - 1U) / LAUNCHER_APPS_PER_PAGE;
	g_page_count = g_app_page_count + 1U;
	for (uint32_t i = 0; i < g_app_count; i++) {
		uint32_t page = i / LAUNCHER_APPS_PER_PAGE;
		uint32_t slot = i % LAUNCHER_APPS_PER_PAGE;

		if (page < LAUNCHER_APP_PAGE_COUNT_MAX) {
			g_page_app_indices[page][slot] = i;
		}
	}
}

static void launcher_load_apps(void)
{
	appmgr_app_list_response_s response = {0};

	memset(g_apps, 0, sizeof(g_apps));
	g_app_count = 0;
	if (g_appmgr == NULL || g_appmgr->ops.get_apps == NULL) {
		return;
	}

	if (g_appmgr->ops.get_apps(g_appmgr, &response) == 0 || response.count == 0U) {
		return;
	}

	g_app_count = response.count > APPMGR_APP_LIST_MAX ? APPMGR_APP_LIST_MAX : response.count;
	for (uint32_t i = 0; i < g_app_count; i++) {
		strncpy(g_apps[i].app_id, response.apps[i].app_id, APPMGR_APP_ID_MAX - 1U);
		strncpy(g_apps[i].name, response.apps[i].app_name, APPMGR_APP_NAME_MAX - 1U);
		strncpy(g_apps[i].subtitle, response.apps[i].subtitle, APPMGR_APP_SUBTITLE_MAX - 1U);
		strncpy(g_apps[i].process_path, response.apps[i].process_path, APPMGR_PROCESS_PATH_MAX - 1U);
		strncpy(g_apps[i].recommendation, response.apps[i].recommendation,
			APPMGR_RECOMMENDATION_MAX - 1U);
		launcher_set_icon_path(&g_apps[i], response.apps[i].icon_path);
		g_apps[i].tile = NULL;
		g_apps[i].status_label = NULL;
		g_apps[i].ready = 0;
		launcher_fill_app_visuals(&g_apps[i]);
	}
	launcher_build_page_map();
}

static lv_obj_t *create_panel(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
			      uint32_t color, uint32_t border)
{
	lv_obj_t *panel = lv_obj_create(parent);

	lv_obj_set_pos(panel, x, y);
	lv_obj_set_size(panel, w, h);
	lv_obj_set_style_bg_color(panel, lv_color_hex(color), 0);
	lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
	lv_obj_set_style_border_color(panel, lv_color_hex(border), 0);
	lv_obj_set_style_border_width(panel, 1, 0);
	lv_obj_set_style_radius(panel, 28, 0);
	lv_obj_set_style_pad_all(panel, 0, 0);
	lv_obj_set_style_shadow_width(panel, 0, 0);
	clear_static_flags(panel);
	return panel;
}

static lv_obj_t *create_metric_card(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
				    lv_coord_t w, lv_coord_t h, const char *title)
{
	const launcher_theme_s *theme = launcher_theme();
	lv_obj_t *card = create_panel(parent, x, y, w, h, theme->panel, theme->line);
	lv_obj_t *title_lbl = lv_label_create(card);

	lv_obj_set_pos(title_lbl, 24, 22);
	lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(title_lbl, lv_color_hex(theme->dim), 0);
	lv_label_set_text(title_lbl, title);

	return lv_label_create(card);
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

static lv_obj_t *create_soft_blob(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
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
	return blob;
}

static lv_obj_t *create_layer(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
			      lv_coord_t w, lv_coord_t h)
{
	lv_obj_t *layer = lv_obj_create(parent);

	lv_obj_remove_style_all(layer);
	lv_obj_set_pos(layer, x, y);
	lv_obj_set_size(layer, w, h);
	clear_static_flags(layer);
	return layer;
}

static lv_obj_t *create_circle_node(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
				    lv_coord_t size, uint32_t bg, uint32_t border,
				    lv_opa_t bg_opa)
{
	lv_obj_t *node = lv_obj_create(parent);

	lv_obj_set_pos(node, x, y);
	lv_obj_set_size(node, size, size);
	lv_obj_set_style_radius(node, 999, 0);
	lv_obj_set_style_bg_color(node, lv_color_hex(bg), 0);
	lv_obj_set_style_bg_opa(node, bg_opa, 0);
	lv_obj_set_style_border_color(node, lv_color_hex(border), 0);
	lv_obj_set_style_border_width(node, 1, 0);
	lv_obj_set_style_shadow_width(node, 0, 0);
	lv_obj_set_style_pad_all(node, 0, 0);
	clear_static_flags(node);
	return node;
}

static void launcher_anim_translate_y(void *obj, int32_t value)
{
	lv_obj_set_style_translate_y((lv_obj_t *)obj, (lv_coord_t)value, 0);
}

static void launcher_anim_translate_x(void *obj, int32_t value)
{
	lv_obj_set_style_translate_x((lv_obj_t *)obj, (lv_coord_t)value, 0);
}

static void launcher_anim_opa(void *obj, int32_t value)
{
	lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)value, 0);
}

static void launcher_start_translate_y_anim(lv_obj_t *obj, int32_t from, int32_t to,
					    uint32_t duration_ms, uint32_t delay_ms)
{
	lv_anim_t anim = {0};

	if (obj == NULL) {
		return;
	}

	lv_obj_set_style_translate_y(obj, (lv_coord_t)from, 0);
	lv_anim_init(&anim);
	lv_anim_set_var(&anim, obj);
	lv_anim_set_exec_cb(&anim, launcher_anim_translate_y);
	lv_anim_set_values(&anim, from, to);
	lv_anim_set_time(&anim, duration_ms);
	lv_anim_set_delay(&anim, delay_ms);
	lv_anim_set_playback_time(&anim, duration_ms);
	lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
	lv_anim_set_path_cb(&anim, lv_anim_path_ease_in_out);
	lv_anim_start(&anim);
}

static void launcher_start_translate_x_anim(lv_obj_t *obj, int32_t from, int32_t to,
					    uint32_t duration_ms, uint32_t delay_ms)
{
	lv_anim_t anim = {0};

	if (obj == NULL) {
		return;
	}

	lv_obj_set_style_translate_x(obj, (lv_coord_t)from, 0);
	lv_anim_init(&anim);
	lv_anim_set_var(&anim, obj);
	lv_anim_set_exec_cb(&anim, launcher_anim_translate_x);
	lv_anim_set_values(&anim, from, to);
	lv_anim_set_time(&anim, duration_ms);
	lv_anim_set_delay(&anim, delay_ms);
	lv_anim_set_playback_time(&anim, duration_ms);
	lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
	lv_anim_set_path_cb(&anim, lv_anim_path_ease_in_out);
	lv_anim_start(&anim);
}

static void launcher_start_opa_anim(lv_obj_t *obj, lv_opa_t from, lv_opa_t to,
				    uint32_t duration_ms, uint32_t delay_ms)
{
	lv_anim_t anim = {0};

	if (obj == NULL) {
		return;
	}

	lv_obj_set_style_opa(obj, from, 0);
	lv_anim_init(&anim);
	lv_anim_set_var(&anim, obj);
	lv_anim_set_exec_cb(&anim, launcher_anim_opa);
	lv_anim_set_values(&anim, from, to);
	lv_anim_set_time(&anim, duration_ms);
	lv_anim_set_delay(&anim, delay_ms);
	lv_anim_set_playback_time(&anim, duration_ms);
	lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
	lv_anim_set_path_cb(&anim, lv_anim_path_ease_in_out);
	lv_anim_start(&anim);
}

static void create_agent_visual(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
				lv_coord_t w, lv_coord_t h)
{
	uint32_t shell_bg = g_theme_mode == LAUNCHER_THEME_DARK ? 0x20283a : 0x263147;
	uint32_t shell_alt = g_theme_mode == LAUNCHER_THEME_DARK ? 0x36405a : 0x35435f;
	uint32_t shell_text = 0xf8f2ea;
	uint32_t shell_dim = 0xb9c1d2;
	uint32_t shell_soft = 0xf4eee7;
	uint32_t lens_dark = 0x243149;
	uint32_t field_line = launcher_mix_color(shell_alt, COLOR_ACCENT_ALT, 1U, 3U);
	uint32_t field_soft = launcher_mix_color(shell_bg, COLOR_ACCENT_ALT, 1U, 8U);
	lv_obj_t *panel = create_panel(parent, x, y, w, h, shell_bg, shell_alt);
	lv_obj_t *field_outer = NULL;
	lv_obj_t *field_inner = NULL;
	lv_obj_t *field_glow = NULL;
	lv_obj_t *presence = NULL;
	lv_obj_t *core = NULL;
	lv_obj_t *core_inner = NULL;
	lv_obj_t *aperture = NULL;
	lv_obj_t *iris = NULL;
	lv_obj_t *glint = NULL;
	lv_obj_t *shadow = NULL;
	lv_obj_t *signal = NULL;
	lv_obj_t *node_top = NULL;
	lv_obj_t *node_left = NULL;
	lv_obj_t *node_right = NULL;
	lv_obj_t *node_bottom = NULL;
	lv_coord_t center_x = (lv_coord_t)(w / 2);
	lv_coord_t center_y = 150;

	lv_obj_set_style_radius(panel, 34, 0);
	lv_obj_set_style_border_width(panel, 0, 0);
	lv_obj_set_style_shadow_width(panel, 12, 0);
	lv_obj_set_style_shadow_color(panel, lv_color_hex(launcher_mix_color(shell_alt, COLOR_BG, 1U, 2U)), 0);
	lv_obj_set_style_shadow_opa(panel, LV_OPA_10, 0);
	create_soft_blob(panel, (lv_coord_t)(w - 170), -20, 186, 186, COLOR_ACCENT_ALT, 14);
	create_soft_blob(panel, 18, 30, 128, 128, COLOR_ACCENT, 8);
	create_soft_blob(panel, (lv_coord_t)(center_x - 126), 54, 252, 252, COLOR_ACCENT_ALT, 9);
	create_soft_blob(panel, (lv_coord_t)(center_x - 84), 96, 170, 170, COLOR_ACCENT, 8);
	create_chip(panel, 18, 18, "Resident agent", shell_alt, shell_text);
	create_chip(panel, (lv_coord_t)(w - 102), 18, "attending", 0x1f3a34, COLOR_SUCCESS);

	field_outer = lv_obj_create(panel);
	lv_obj_set_pos(field_outer, (lv_coord_t)(center_x - 110), 44);
	lv_obj_set_size(field_outer, 220, 220);
	lv_obj_set_style_radius(field_outer, 999, 0);
	lv_obj_set_style_bg_opa(field_outer, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_color(field_outer, lv_color_hex(shell_alt), 0);
	lv_obj_set_style_border_width(field_outer, 1, 0);
	lv_obj_set_style_shadow_width(field_outer, 0, 0);
	lv_obj_set_style_pad_all(field_outer, 0, 0);
	clear_static_flags(field_outer);

	field_glow = create_soft_blob(panel, (lv_coord_t)(center_x - 86), 68, 172, 172, COLOR_ACCENT_ALT, 18);
	field_inner = lv_obj_create(panel);
	lv_obj_set_pos(field_inner, (lv_coord_t)(center_x - 82), 72);
	lv_obj_set_size(field_inner, 164, 164);
	lv_obj_set_style_radius(field_inner, 999, 0);
	lv_obj_set_style_bg_color(field_inner, lv_color_hex(field_soft), 0);
	lv_obj_set_style_bg_opa(field_inner, 46, 0);
	lv_obj_set_style_border_color(field_inner, lv_color_hex(field_line), 0);
	lv_obj_set_style_border_width(field_inner, 1, 0);
	lv_obj_set_style_shadow_width(field_inner, 0, 0);
	lv_obj_set_style_pad_all(field_inner, 0, 0);
	clear_static_flags(field_inner);

	presence = create_layer(panel, (lv_coord_t)(center_x - 86), 66, 172, 188);
	signal = lv_obj_create(panel);
	lv_obj_set_pos(signal, (lv_coord_t)(center_x - 1), 36);
	lv_obj_set_size(signal, 2, 44);
	lv_obj_set_style_bg_color(signal, lv_color_hex(COLOR_SUCCESS), 0);
	lv_obj_set_style_bg_opa(signal, LV_OPA_80, 0);
	lv_obj_set_style_border_width(signal, 0, 0);
	lv_obj_set_style_radius(signal, 999, 0);
	clear_static_flags(signal);

	core = create_panel(presence, 20, 20, 132, 132, shell_soft, shell_soft);
	lv_obj_set_style_radius(core, 999, 0);
	lv_obj_set_style_border_width(core, 0, 0);
	lv_obj_set_style_shadow_width(core, 14, 0);
	lv_obj_set_style_shadow_color(core, lv_color_hex(launcher_mix_color(shell_bg, shell_soft, 1U, 3U)), 0);
	lv_obj_set_style_shadow_opa(core, LV_OPA_30, 0);

	core_inner = create_panel(core, 16, 16, 100, 100,
				 launcher_mix_color(shell_soft, COLOR_BG, 1U, 3U), shell_soft);
	lv_obj_set_style_radius(core_inner, 999, 0);
	lv_obj_set_style_border_width(core_inner, 0, 0);
	lv_obj_set_style_shadow_width(core_inner, 0, 0);

	aperture = create_panel(core, 21, 52, 90, 28, lens_dark, lens_dark);
	lv_obj_set_style_radius(aperture, 999, 0);
	lv_obj_set_style_border_width(aperture, 0, 0);
	lv_obj_set_style_shadow_width(aperture, 0, 0);

	iris = create_circle_node(aperture, 34, 4, 20, COLOR_ACCENT_ALT, COLOR_ACCENT_ALT, LV_OPA_COVER);
	glint = create_circle_node(aperture, 44, 12, 5, shell_soft, shell_soft, LV_OPA_COVER);
	shadow = create_panel(presence, 34, 146, 104, 20,
			      launcher_mix_color(shell_bg, shell_alt, 1U, 2U),
			      launcher_mix_color(shell_bg, shell_alt, 1U, 2U));
	lv_obj_set_style_radius(shadow, 999, 0);
	lv_obj_set_style_border_width(shadow, 0, 0);
	lv_obj_set_style_shadow_width(shadow, 0, 0);

	node_top = create_circle_node(panel, (lv_coord_t)(center_x - 12), 32, 24,
				      shell_bg, COLOR_SUCCESS, LV_OPA_COVER);
	node_left = create_circle_node(panel, 102, (lv_coord_t)(center_y - 12), 24,
				       shell_bg, COLOR_ACCENT, LV_OPA_COVER);
	node_right = create_circle_node(panel, (lv_coord_t)(w - 126), (lv_coord_t)(center_y - 12),
					24, shell_bg, COLOR_WARN, LV_OPA_COVER);
	node_bottom = create_circle_node(panel, (lv_coord_t)(center_x - 12), 258, 24,
					 shell_bg, COLOR_ACCENT_ALT, LV_OPA_COVER);

	create_text_label(panel, 30, 308, (lv_coord_t)(w - 60), &lv_font_montserrat_16, shell_text,
			 LV_TEXT_ALIGN_CENTER, "Holding context and choosing the next move.");

	launcher_start_translate_y_anim(presence, 0, -8, 2800U, 120U);
	launcher_start_translate_x_anim(iris, -8, 8, 2400U, 420U);
	launcher_start_translate_x_anim(glint, -6, 6, 2400U, 420U);
	launcher_start_opa_anim(field_glow, 40, 110, 3200U, 0U);
	launcher_start_opa_anim(field_inner, 100, 180, 3200U, 260U);
	launcher_start_translate_y_anim(node_top, 0, -6, 2200U, 120U);
	launcher_start_translate_y_anim(node_left, 0, 5, 2600U, 520U);
	launcher_start_translate_y_anim(node_right, 0, -4, 2400U, 840U);
	launcher_start_translate_y_anim(node_bottom, 0, 6, 2800U, 380U);
	launcher_start_opa_anim(signal, LV_OPA_40, LV_OPA_90, 1800U, 0U);
}

static void launcher_set_intro_summary(const char *text, uint32_t color)
{
	if (text == NULL || g_hero_summary[0] == NULL) {
		return;
	}

	set_label_text(g_hero_summary[0], text, color);
}

static void launcher_set_app_page_summaries(const char *text, uint32_t color)
{
	(void)text;
	(void)color;
}

static void create_task_row(lv_obj_t *parent, uint32_t index, lv_coord_t y, uint8_t add_divider)
{
	const launcher_theme_s *theme = launcher_theme();
	lv_obj_t *row = NULL;
	(void)add_divider;

	if (index >= LAUNCHER_AGENT_TASK_COUNT) {
		return;
	}

	row = create_panel(parent, 20, y, 392, 48, theme->panel_alt, theme->panel_alt);
	lv_obj_set_style_border_width(row, 0, 0);
	lv_obj_set_style_shadow_width(row, 0, 0);
	lv_obj_set_style_radius(row, 20, 0);
	lv_obj_set_style_bg_opa(row, 150, 0);
	g_task_row_panel[index] = row;

	g_task_title[index] = create_text_label(row, 16, 7, 228,
						&lv_font_montserrat_16, theme->text,
						LV_TEXT_ALIGN_LEFT, "Waiting");
	lv_label_set_long_mode(g_task_title[index], LV_LABEL_LONG_DOT);
	g_task_body[index] = create_text_label(row, 16, 25, 244,
					       &lv_font_montserrat_14, theme->dim,
					       LV_TEXT_ALIGN_LEFT, "No agent work yet.");
	lv_label_set_long_mode(g_task_body[index], LV_LABEL_LONG_DOT);
	g_task_status[index] = create_chip(row, 284, 8, "WAITING", theme->warm_soft, theme->warn);
}

static void launcher_set_task_row(uint32_t index, const char *status_text,
				  uint32_t status_bg, uint32_t status_fg,
				  const char *title, const char *body)
{
	const launcher_theme_s *theme = launcher_theme();

	if (index >= LAUNCHER_AGENT_TASK_COUNT) {
		return;
	}
	if (g_task_row_panel[index] != NULL) {
		uint32_t row_bg = launcher_mix_color(theme->panel, status_fg, 1U, 12U);

		lv_obj_set_style_bg_color(g_task_row_panel[index], lv_color_hex(row_bg), 0);
		lv_obj_set_style_bg_opa(g_task_row_panel[index], 170, 0);
	}
	if (g_task_status[index] != NULL && status_text != NULL) {
		set_chip_text(g_task_status[index], status_text, status_bg, status_fg);
	}
	if (g_task_title[index] != NULL && title != NULL) {
		set_label_text(g_task_title[index], title, theme->text);
	}
	if (g_task_body[index] != NULL && body != NULL) {
		set_label_text(g_task_body[index], body, theme->dim);
	}
}

static lv_obj_t *create_app_tile(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
				 lv_coord_t w, lv_coord_t h, launcher_app_entry_s *app)
{
	const launcher_theme_s *theme = launcher_theme();
	lv_obj_t *tile = lv_btn_create(parent);
	lv_obj_t *badge = lv_obj_create(tile);
	lv_obj_t *title_lbl = lv_label_create(tile);
	lv_obj_t *subtitle_lbl = lv_label_create(tile);
	uint32_t tile_pressed = launcher_mix_color(theme->panel, theme->panel_alt, 1U, 1U);
	uint32_t tile_shadow = launcher_mix_color(theme->line, theme->panel, 1, 2);
	uint8_t compact = h < 144 ? 1U : 0U;
	lv_coord_t badge_size = compact ? (lv_coord_t)(h - (LAUNCHER_TILE_BADGE_MARGIN * 2)) : LAUNCHER_TILE_BADGE_SIZE;
	lv_coord_t badge_x = compact ? LAUNCHER_TILE_BADGE_MARGIN : 22;
	lv_coord_t badge_y = compact ? LAUNCHER_TILE_BADGE_MARGIN : 26;
	lv_coord_t content_x = (lv_coord_t)(badge_x + badge_size + 18);
	lv_coord_t content_w = (lv_coord_t)(w - content_x - 22);
	lv_coord_t title_y = compact ? 16 : 30;
	lv_coord_t subtitle_y = compact ? 46 : 68;
	lv_coord_t status_y = compact ? 86 : 108;

	lv_obj_remove_style_all(tile);
	lv_obj_set_pos(tile, x, y);
	lv_obj_set_size(tile, w, h);
	lv_obj_set_style_radius(tile, 30, 0);
	lv_obj_set_style_bg_color(tile, lv_color_hex(theme->panel), 0);
	lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
	lv_obj_set_style_bg_color(tile, lv_color_hex(tile_pressed), LV_STATE_PRESSED);
	lv_obj_set_style_border_width(tile, 0, 0);
	lv_obj_set_style_border_width(tile, 0, LV_STATE_FOCUSED);
	lv_obj_set_style_border_width(tile, 0, LV_STATE_PRESSED);
	lv_obj_set_style_outline_width(tile, 0, 0);
	lv_obj_set_style_outline_width(tile, 0, LV_STATE_FOCUSED);
	lv_obj_set_style_shadow_width(tile, 10, 0);
	lv_obj_set_style_shadow_color(tile, lv_color_hex(tile_shadow), 0);
	lv_obj_set_style_shadow_opa(tile, 18, 0);
	lv_obj_set_style_pad_all(tile, 0, 0);
	lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_event_cb(tile, launch_app_event_cb, LV_EVENT_CLICKED, app);

	lv_obj_set_pos(badge, badge_x, badge_y);
	lv_obj_set_size(badge, badge_size, badge_size);
	lv_obj_set_style_radius(badge, compact ? 26 : 28, 0);
	lv_obj_set_style_bg_color(badge, lv_color_hex(app->accent_color), 0);
	lv_obj_set_style_bg_opa(badge, 60, 0);
	lv_obj_set_style_border_width(badge, 0, 0);
	lv_obj_set_style_shadow_width(badge, 0, 0);
	clear_static_flags(badge);

	if (app->icon_path[0] != '\0') {
		lv_obj_t *badge_icon = lv_img_create(badge);

		lv_img_set_src(badge_icon, app->icon_path);
		lv_img_set_zoom(badge_icon, LV_IMG_ZOOM_NONE);
		lv_obj_center(badge_icon);
		clear_static_flags(badge_icon);
	} else {
		lv_obj_t *badge_text = lv_label_create(badge);

		lv_label_set_text(badge_text, app->badge_text);
		lv_obj_set_style_text_font(badge_text, &lv_font_montserrat_24, 0);
		lv_obj_set_style_text_color(badge_text, lv_color_hex(app->accent_color), 0);
		lv_obj_center(badge_text);
		clear_static_flags(badge_text);
	}

	lv_obj_set_pos(title_lbl, content_x, title_y);
	lv_obj_set_width(title_lbl, content_w);
	lv_label_set_long_mode(title_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_24, 0);
	lv_obj_set_style_text_color(title_lbl, lv_color_hex(theme->text), 0);
	lv_label_set_text(title_lbl, app->name);

	lv_obj_set_pos(subtitle_lbl, content_x, subtitle_y);
	lv_obj_set_width(subtitle_lbl, content_w);
	lv_label_set_long_mode(subtitle_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(subtitle_lbl, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(subtitle_lbl, lv_color_hex(theme->dim), 0);
	lv_label_set_text(subtitle_lbl, app->subtitle);

	app->status_label = create_chip(tile, content_x, status_y, "Syncing", theme->panel_alt, theme->dim);
	app->tile = tile;
	return tile;
}

static lv_coord_t launcher_indicator_track_width(void)
{
	if (g_page_count <= 1U) {
		return 0;
	}

	return (lv_coord_t)(g_page_count * LAUNCHER_PAGE_DOT_SLOT_W +
			    LAUNCHER_PAGE_INDICATOR_SIDE_PAD * 2);
}

static void launcher_update_page_indicator(uint32_t active_page)
{
	const launcher_theme_s *theme = launcher_theme();
	lv_coord_t track_w = launcher_indicator_track_width();

	if (g_page_count == 0U) {
		if (g_page_indicator_track != NULL) {
			lv_obj_add_flag(g_page_indicator_track, LV_OBJ_FLAG_HIDDEN);
		}
		return;
	}
	if (active_page >= g_page_count) {
		active_page = g_page_count - 1U;
	}

	g_active_page = active_page;
	if (g_page_indicator_track != NULL) {
		if (track_w == 0) {
			lv_obj_add_flag(g_page_indicator_track, LV_OBJ_FLAG_HIDDEN);
		} else {
			lv_obj_clear_flag(g_page_indicator_track, LV_OBJ_FLAG_HIDDEN);
			lv_obj_set_size(g_page_indicator_track, track_w, LAUNCHER_PAGE_INDICATOR_H);
			lv_obj_set_pos(g_page_indicator_track,
				       (lv_coord_t)((APP_DEFAULT_WIDTH - track_w) / 2),
				       LAUNCHER_PAGE_INDICATOR_Y);
		}
	}

	for (uint32_t i = 0; i < LAUNCHER_PAGE_COUNT_MAX; i++) {
		lv_obj_t *dot = g_page_dots[i];
		uint8_t active = i == g_active_page && i < g_page_count;
		lv_coord_t dot_w = active ? LAUNCHER_PAGE_DOT_ACTIVE_W : LAUNCHER_PAGE_DOT_INACTIVE_W;
		lv_coord_t slot_x = 0;

		if (dot == NULL) {
			continue;
		}

		if (i >= g_page_count || g_page_count <= 1U) {
			lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
			continue;
		}

		lv_obj_clear_flag(dot, LV_OBJ_FLAG_HIDDEN);
		slot_x = (lv_coord_t)(LAUNCHER_PAGE_INDICATOR_SIDE_PAD + i * LAUNCHER_PAGE_DOT_SLOT_W);
		lv_obj_set_pos(dot,
			       (lv_coord_t)(slot_x + (LAUNCHER_PAGE_DOT_SLOT_W - dot_w) / 2),
			       (lv_coord_t)((LAUNCHER_PAGE_INDICATOR_H - LAUNCHER_PAGE_DOT_H) / 2));
		lv_obj_set_size(dot, dot_w, LAUNCHER_PAGE_DOT_H);
		lv_obj_set_style_radius(dot, 999, 0);
		lv_obj_set_style_bg_color(dot,
					  lv_color_hex(active ? COLOR_ACCENT : theme->line),
					  0);
		lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
		lv_obj_set_style_shadow_width(dot, active ? 10 : 0, 0);
		lv_obj_set_style_shadow_color(dot, lv_color_hex(COLOR_ACCENT), 0);
		lv_obj_set_style_shadow_opa(dot, active ? LV_OPA_20 : LV_OPA_TRANSP, 0);
	}
}

static void launcher_pager_event_cb(lv_event_t *e)
{
	lv_event_code_t code = lv_event_get_code(e);
	lv_obj_t *pager = lv_event_get_target(e);
	lv_coord_t page_span = 0;
	lv_coord_t scroll_x = 0;
	uint32_t active_page = 0;

	if (code != LV_EVENT_SCROLL && code != LV_EVENT_SCROLL_END) {
		return;
	}
	if (pager == NULL || g_page_count == 0U) {
		return;
	}

	page_span = LAUNCHER_PAGE_STRIDE;
	if (page_span <= 0) {
		return;
	}

	if (code == LV_EVENT_SCROLL_END) {
		lv_point_t end = {0};
		lv_obj_get_scroll_end(pager, &end);
		scroll_x = end.x;
	} else {
		scroll_x = lv_obj_get_scroll_x(pager);
	}

	active_page = (uint32_t)((scroll_x + (page_span / 2)) / page_span);
	launcher_update_page_indicator(active_page);
}

static launcher_app_entry_s *launcher_find_recommended_app(void)
{
	for (uint32_t i = 0; i < g_app_count; i++) {
		if (g_apps[i].ready) {
			return &g_apps[i];
		}
	}

	return g_app_count == 0U ? NULL : &g_apps[0];
}

static launcher_app_entry_s *launcher_find_app_by_id(const char *app_id)
{
	if (app_id == NULL || app_id[0] == '\0') {
		return NULL;
	}

	for (uint32_t i = 0; i < g_app_count; i++) {
		if (strcmp(g_apps[i].app_id, app_id) == 0) {
			return &g_apps[i];
		}
	}

	return NULL;
}

static launcher_app_entry_s *launcher_primary_action_app(void)
{
	launcher_app_entry_s *assistant = launcher_find_app_by_id("ai");

	if (assistant != NULL && assistant->ready) {
		return assistant;
	}
	if (g_recommended_app != NULL && g_recommended_app->ready) {
		return g_recommended_app;
	}
	if (assistant != NULL) {
		return assistant;
	}
	return g_recommended_app;
}

static launcher_app_entry_s *launcher_mode_target_app(launcher_mode_e mode)
{
	launcher_app_entry_s *assistant = launcher_find_app_by_id("ai");

	(void)mode;
	if (assistant != NULL && assistant->ready) {
		return assistant;
	}
	return launcher_primary_action_app();
}

static void launcher_update_intro_panel(void)
{
	const launcher_theme_s *theme = launcher_theme();
	launcher_app_entry_s *assistant = launcher_find_app_by_id("ai");
	launcher_app_entry_s *primary = launcher_primary_action_app();
	launcher_app_entry_s *suggested = g_recommended_app != NULL ? g_recommended_app : assistant;
	uint32_t syncing_count = g_app_count > g_installed_app_count ?
		g_app_count - g_installed_app_count : 0U;
	uint32_t button_bg = theme->panel_alt;
	uint32_t button_fg = theme->dim;
	uint32_t button_pressed = theme->panel;
	uint32_t intro_color = theme->dim;
	uint32_t task_summary_color = theme->dim;
	char ready_text[32] = {0};
	char sync_text[32] = {0};
	char deck_text[32] = {0};
	char intro_text[192] = {0};
	char button_text[72] = {0};
	char task_summary[64] = {0};
	char task_title[LAUNCHER_AGENT_TASK_COUNT][64] = {{0}};
	char task_body[LAUNCHER_AGENT_TASK_COUNT][128] = {{0}};
	char task_state[LAUNCHER_AGENT_TASK_COUNT][16] = {{0}};
	uint32_t task_state_bg[LAUNCHER_AGENT_TASK_COUNT] = {0};
	uint32_t task_state_fg[LAUNCHER_AGENT_TASK_COUNT] = {0};
	uint32_t active_tasks = 0U;
	uint32_t waiting_tasks = 0U;
	char workspace_text[160] = {0};
	char live_text[160] = {0};

	snprintf(ready_text, sizeof(ready_text), "Ready %u", (unsigned int)g_installed_app_count);
	snprintf(sync_text, sizeof(sync_text), "Sync %u", (unsigned int)syncing_count);
	if (g_app_page_count == 1U) {
		strncpy(deck_text, "Deck 1", sizeof(deck_text) - 1U);
	} else {
		snprintf(deck_text, sizeof(deck_text), "Decks %u", (unsigned int)g_app_page_count);
	}

	if (g_app_count == 0U) {
		strncpy(intro_text,
			"No tools are published yet. When apps arrive, pages 2 and 3 turn into the live deck.",
			sizeof(intro_text) - 1U);
		strncpy(button_text, "No tool available", sizeof(button_text) - 1U);
		strncpy(workspace_text, "0 tools published", sizeof(workspace_text) - 1U);
		strncpy(live_text, "This page stays focused on orchestration until the app decks come online.",
			sizeof(live_text) - 1U);
		intro_color = theme->warn;
	} else {
		if (g_app_page_count > 1U) {
			snprintf(workspace_text, sizeof(workspace_text),
				 "%u tools are split across %u decks.",
				 (unsigned int)g_app_count,
				 (unsigned int)g_app_page_count);
		} else {
			snprintf(workspace_text, sizeof(workspace_text),
				 "%u tools are staged in the next deck.",
				 (unsigned int)g_app_count);
		}
		if (syncing_count > 0U) {
			snprintf(live_text, sizeof(live_text),
				 "%u ready now, %u still syncing from storage.",
				 (unsigned int)g_installed_app_count,
				 (unsigned int)syncing_count);
		} else {
			snprintf(live_text, sizeof(live_text),
				 "All %u published tools are ready to launch.",
				 (unsigned int)g_app_count);
		}

		if (assistant != NULL && assistant->ready) {
			strncpy(intro_text,
				"Assistant is live. Open it to route the next move before jumping into a tool.",
				sizeof(intro_text) - 1U);
			strncpy(button_text, "Open Assistant", sizeof(button_text) - 1U);
			button_bg = COLOR_ACCENT;
			button_fg = theme->text;
			button_pressed = launcher_mix_color(COLOR_ACCENT, theme->panel, 1U, 3U);
			intro_color = theme->text;
		} else if (primary != NULL && primary->ready) {
			snprintf(intro_text, sizeof(intro_text),
				 "%s is the cleanest ready tool right now. Open it here or swipe right for the full deck.",
				 primary->name);
			snprintf(button_text, sizeof(button_text),
				 "Open %s", primary->name);
			button_bg = primary->accent_color;
			button_fg = theme->text;
			button_pressed = launcher_mix_color(primary->accent_color, theme->panel, 1U, 3U);
			intro_color = theme->text;
		} else {
			strncpy(intro_text,
				"The desk sees your tools, but packages are still syncing into place. The router will wake up as soon as one is ready.",
				sizeof(intro_text) - 1U);
			if (assistant != NULL) {
				strncpy(button_text, "Assistant syncing", sizeof(button_text) - 1U);
			} else {
				strncpy(button_text, "Tools syncing", sizeof(button_text) - 1U);
			}
			intro_color = theme->warn;
		}
	}

	strncpy(task_title[0], "Watch workspace", sizeof(task_title[0]) - 1U);
	if (g_app_count == 0U) {
		strncpy(task_state[0], "WAITING", sizeof(task_state[0]) - 1U);
		strncpy(task_body[0], "Waiting for published tools.", sizeof(task_body[0]) - 1U);
		task_state_bg[0] = theme->warm_soft;
		task_state_fg[0] = theme->warn;
		waiting_tasks++;
	} else {
		strncpy(task_state[0], "ACTIVE", sizeof(task_state[0]) - 1U);
		if (syncing_count > 0U) {
			snprintf(task_body[0], sizeof(task_body[0]),
				 "%u published, %u still syncing.",
				 (unsigned int)g_app_count, (unsigned int)syncing_count);
		} else {
			snprintf(task_body[0], sizeof(task_body[0]),
				 "%u ready across %u deck%s.",
				 (unsigned int)g_installed_app_count,
				 (unsigned int)g_app_page_count,
				 g_app_page_count == 1U ? "" : "s");
		}
		task_state_bg[0] = launcher_mix_color(theme->panel, COLOR_ACCENT_ALT, 1U, 5U);
		task_state_fg[0] = COLOR_ACCENT_ALT;
		active_tasks++;
	}

	strncpy(task_title[1], "Hold assistant", sizeof(task_title[1]) - 1U);
	if (assistant != NULL && assistant->ready) {
		strncpy(task_state[1], "READY", sizeof(task_state[1]) - 1U);
		strncpy(task_body[1], "Listening for intent.", sizeof(task_body[1]) - 1U);
		task_state_bg[1] = theme->success_soft;
		task_state_fg[1] = COLOR_SUCCESS;
		active_tasks++;
	} else if (assistant != NULL) {
		strncpy(task_state[1], "WAITING", sizeof(task_state[1]) - 1U);
		strncpy(task_body[1], "Assistant is still syncing.", sizeof(task_body[1]) - 1U);
		task_state_bg[1] = theme->warm_soft;
		task_state_fg[1] = theme->warn;
		waiting_tasks++;
	} else {
		strncpy(task_state[1], "WAITING", sizeof(task_state[1]) - 1U);
		strncpy(task_body[1], "Assistant is not published here.", sizeof(task_body[1]) - 1U);
		task_state_bg[1] = theme->warm_soft;
		task_state_fg[1] = theme->warn;
		waiting_tasks++;
	}

	strncpy(task_title[2], "Next launch", sizeof(task_title[2]) - 1U);
	if (g_app_count == 0U) {
		strncpy(task_state[2], "WAITING", sizeof(task_state[2]) - 1U);
		strncpy(task_body[2], "No launch target yet.", sizeof(task_body[2]) - 1U);
		task_state_bg[2] = theme->warm_soft;
		task_state_fg[2] = theme->warn;
		waiting_tasks++;
	} else if (suggested != NULL && suggested->ready) {
		strncpy(task_state[2], "READY", sizeof(task_state[2]) - 1U);
		if (assistant != NULL && assistant->ready && strcmp(suggested->app_id, assistant->app_id) != 0) {
			snprintf(task_body[2], sizeof(task_body[2]),
				 "%s is queued next.", suggested->name);
		} else if (assistant != NULL && assistant->ready) {
			strncpy(task_body[2], "Assistant stays primary.", sizeof(task_body[2]) - 1U);
		} else {
			snprintf(task_body[2], sizeof(task_body[2]),
				 "%s is ready to open.", suggested->name);
		}
		task_state_bg[2] = theme->success_soft;
		task_state_fg[2] = COLOR_SUCCESS;
		active_tasks++;
	} else if (suggested != NULL) {
		strncpy(task_state[2], "WAITING", sizeof(task_state[2]) - 1U);
		snprintf(task_body[2], sizeof(task_body[2]),
			 "Waiting for %s.", suggested->name);
		task_state_bg[2] = theme->warm_soft;
		task_state_fg[2] = theme->warn;
		waiting_tasks++;
	} else {
		strncpy(task_state[2], "WAITING", sizeof(task_state[2]) - 1U);
		strncpy(task_body[2], "No ready handoff target is visible yet.", sizeof(task_body[2]) - 1U);
		task_state_bg[2] = theme->warm_soft;
		task_state_fg[2] = theme->warn;
		waiting_tasks++;
	}

	if (waiting_tasks == 0U) {
		snprintf(task_summary, sizeof(task_summary), "%u active", (unsigned int)active_tasks);
	} else {
		snprintf(task_summary, sizeof(task_summary), "%u active · %u wait",
			 (unsigned int)active_tasks, (unsigned int)waiting_tasks);
	}
	task_summary_color = active_tasks > 0U ? COLOR_ACCENT_ALT : theme->warn;

	if (g_ready_chip != NULL) {
		set_chip_text(g_ready_chip, ready_text,
			     g_installed_app_count > 0U ? theme->success_soft : theme->panel_alt,
			     g_installed_app_count > 0U ? COLOR_SUCCESS : theme->dim);
	}
	if (g_proc_chip != NULL) {
		set_chip_text(g_proc_chip, sync_text,
			     syncing_count > 0U ? theme->warm_soft : theme->panel_alt,
			     syncing_count > 0U ? theme->warn : theme->dim);
	}
	if (g_mem_chip != NULL) {
		set_chip_text(g_mem_chip, deck_text, theme->accent_soft, COLOR_ACCENT);
	}

	launcher_set_intro_summary(intro_text, intro_color);
	if (g_task_summary != NULL) {
		set_label_text(g_task_summary, task_summary, task_summary_color);
	}
	for (uint32_t i = 0; i < LAUNCHER_AGENT_TASK_COUNT; i++) {
		launcher_set_task_row(i, task_state[i], task_state_bg[i], task_state_fg[i],
				      task_title[i], task_body[i]);
	}
	if (g_memory_workspace != NULL) {
		set_label_text(g_memory_workspace, workspace_text, theme->text);
	}
	if (g_memory_live != NULL) {
		set_label_text(g_memory_live, live_text, theme->dim);
	}
	if (g_launch_btn != NULL) {
		lv_obj_set_style_bg_color(g_launch_btn, lv_color_hex(button_bg), 0);
		lv_obj_set_style_bg_color(g_launch_btn, lv_color_hex(button_pressed), LV_STATE_PRESSED);
		lv_obj_set_style_text_color(g_launch_btn, lv_color_hex(button_fg), 0);
	}
	if (g_launch_btn_label != NULL) {
		lv_label_set_text(g_launch_btn_label, button_text);
		lv_obj_set_style_text_color(g_launch_btn_label, lv_color_hex(button_fg), 0);
	}
}

static uint8_t storage_app_exists(const char *process_path)
{
	char fs_path[128] = {0};
	int64_t fd = -1;

	if (g_fs == NULL || process_path == NULL) {
		return 0;
	}
	if (!process_loader_resolve_storage_path(process_path, fs_path, sizeof(fs_path))) {
		return 0;
	}

	fd = (int64_t)g_fs->ops.open(g_fs, (char *)fs_path);
	if (fd < 0) {
		return 0;
	}
	(void)g_fs->ops.close(g_fs, (uint64_t)fd);
	return 1;
}

static void update_app_tile_state(launcher_app_entry_s *app)
{
	const launcher_theme_s *theme = launcher_theme();
	uint32_t tile_shadow = launcher_mix_color(theme->line, theme->panel, 1, 2);
	if (app == NULL || app->tile == NULL) {
		return;
	}

	if (app->ready) {
		lv_obj_set_style_bg_color(app->tile, lv_color_hex(theme->panel), 0);
		lv_obj_set_style_border_width(app->tile, 0, 0);
		lv_obj_set_style_shadow_width(app->tile, 10, 0);
		lv_obj_set_style_shadow_color(app->tile, lv_color_hex(tile_shadow), 0);
		lv_obj_set_style_shadow_opa(app->tile, 18, 0);
		if (app->status_label != NULL) {
			set_chip_text(app->status_label, "Ready now", theme->accent_soft, app->accent_color);
		}
	} else {
		lv_obj_set_style_bg_color(app->tile, lv_color_hex(theme->panel_alt), 0);
		lv_obj_set_style_border_width(app->tile, 0, 0);
		lv_obj_set_style_shadow_width(app->tile, 0, 0);
		lv_obj_set_style_shadow_opa(app->tile, LV_OPA_TRANSP, 0);
		if (app->status_label != NULL) {
			set_chip_text(app->status_label, "Syncing in", theme->panel, theme->warn);
		}
	}
}

static void refresh_metrics(uint64_t mono_ms, uint8_t rescan_storage)
{
	const launcher_theme_s *theme = launcher_theme();
	char status_text[128] = {0};

	if (g_systemd == NULL) {
		return;
	}
	if (!rescan_storage && mono_ms - g_last_metrics_ms < 1000ULL) {
		return;
	}
	g_last_metrics_ms = mono_ms;

	if (rescan_storage) {
		for (uint32_t i = 0; i < g_app_count; i++) {
			g_apps[i].ready = storage_app_exists(g_apps[i].process_path);
		}
	}

	g_installed_app_count = 0;
	for (uint32_t i = 0; i < g_app_count; i++) {
		g_installed_app_count += g_apps[i].ready ? 1U : 0U;
		update_app_tile_state(&g_apps[i]);
	}
	g_recommended_app = launcher_find_recommended_app();
	launcher_update_intro_panel();

	if (g_app_count == 0U) {
		launcher_set_app_page_summaries(
			"No apps are installed yet. Publish a few and the tool decks will wake up here.",
			theme->warn);
	} else if (g_installed_app_count == g_app_count) {
		if (g_app_page_count > 1U) {
			snprintf(status_text, sizeof(status_text),
				 "%u apps are ready across %u decks. Swipe sideways and pick your next move.",
				 (unsigned int)g_installed_app_count,
				 (unsigned int)g_app_page_count);
		} else {
			snprintf(status_text, sizeof(status_text),
				 "%u apps are ready. Swipe right and jump straight in.",
				 (unsigned int)g_app_count);
		}
		launcher_set_app_page_summaries(status_text, theme->text);
	} else if (g_installed_app_count > 0U) {
		if (g_app_page_count > 1U) {
			snprintf(status_text, sizeof(status_text),
				 "%u of %u apps are ready across %u decks. The rest are still syncing into place.",
				 (unsigned int)g_installed_app_count,
				 (unsigned int)g_app_count,
				 (unsigned int)g_app_page_count);
		} else {
			snprintf(status_text, sizeof(status_text),
				 "%u of %u apps are ready. Swipe right for the deck while the rest warm up.",
				 (unsigned int)g_installed_app_count,
				 (unsigned int)g_app_count);
		}
		launcher_set_app_page_summaries(status_text, theme->text);
	} else {
		snprintf(status_text, sizeof(status_text),
			 "Your app decks are staged. Packages are still syncing into storage.");
		launcher_set_app_page_summaries(status_text, theme->warn);
	}
}

static void launch_app(launcher_app_entry_s *app)
{
	const launcher_theme_s *theme = launcher_theme();
	uint64_t now_ms = 0;

	if (app == NULL) {
		return;
	}

	now_ms = OSSysCtrlGetMonoTime() / NSEC_PER_MSEC;
	refresh_metrics(now_ms, 1);
	if (g_appmgr == NULL) {
		set_label_text_group(g_hero_summary, LAUNCHER_PAGE_COUNT_MAX,
				      "The app service is offline right now.", theme->warn);
		return;
	}

	if (!app->ready) {
		char warn_text[128] = {0};

		snprintf(warn_text, sizeof(warn_text),
			 "%s is listed here, but its package is not available yet.",
			 app->name);
		set_label_text_group(g_hero_summary, LAUNCHER_PAGE_COUNT_MAX, warn_text, theme->warn);
		return;
	}

	{
		char request_text[128] = {0};
		snprintf(request_text, sizeof(request_text),
			 "Opening %s...",
			 app->name);
		set_label_text_group(g_hero_summary, LAUNCHER_PAGE_COUNT_MAX, request_text, COLOR_ACCENT);
	}
	lv_refr_now(NULL);
	lv_port_disp_submit();

	if (!g_appmgr->ops.launch(g_appmgr, app->app_id)) {
		char error_text[128] = {0};

		snprintf(error_text, sizeof(error_text),
			 "%s did not open. Check the serial log and try again.",
			 app->name);
		set_label_text_group(g_hero_summary, LAUNCHER_PAGE_COUNT_MAX, error_text, theme->warn);
		log_error("launcher: appmgr launch %s failed!\n", app->app_id);
		return;
	}

	{
		char success_text[160] = {0};
		snprintf(success_text, sizeof(success_text),
			 "%s is opening now. Tap it again later to bring it forward.",
			 app->name);
		set_label_text_group(g_hero_summary, LAUNCHER_PAGE_COUNT_MAX, success_text, COLOR_SUCCESS);
	}
	log_info("launcher: %s launch request submitted through appmgr\n", app->app_id);
}

static void launch_app_event_cb(lv_event_t *e)
{
	if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
		return;
	}

	launch_app((launcher_app_entry_s *)lv_event_get_user_data(e));
}

static void launch_primary_event_cb(lv_event_t *e)
{
	const launcher_theme_s *theme = launcher_theme();
	launcher_app_entry_s *primary = NULL;

	if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
		return;
	}

	primary = launcher_primary_action_app();
	if (primary != NULL) {
		launch_app(primary);
		return;
	}

	set_label_text_group(g_hero_summary, LAUNCHER_PAGE_COUNT_MAX,
			      "No applications are available yet.", theme->warn);
}

static void launch_mode_event_cb(lv_event_t *e)
{
	const launcher_theme_s *theme = launcher_theme();
	launcher_mode_e mode = (launcher_mode_e)(uintptr_t)lv_event_get_user_data(e);
	launcher_app_entry_s *target = NULL;

	if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
		return;
	}

	target = launcher_mode_target_app(mode);
	if (target != NULL) {
		launch_app(target);
		return;
	}

	set_label_text_group(g_hero_summary, LAUNCHER_PAGE_COUNT_MAX,
			      "No ready application is available yet. Try again after packages finish syncing.",
			      theme->warn);
}

static lv_obj_t *create_button(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
			       const char *text, uint32_t bg, lv_event_cb_t cb, void *user_data,
			       lv_obj_t **label_out)
{
	const launcher_theme_s *theme = launcher_theme();
	lv_obj_t *btn = lv_btn_create(parent);
	lv_obj_t *label = lv_label_create(btn);

	lv_obj_set_pos(btn, x, y);
	lv_obj_set_size(btn, w, h);
	lv_obj_set_style_radius(btn, 22, 0);
	lv_obj_set_style_bg_color(btn, lv_color_hex(bg), 0);
	lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
	lv_obj_set_style_border_width(btn, 0, 0);
	lv_obj_set_style_shadow_width(btn, 18, 0);
	lv_obj_set_style_shadow_color(btn, lv_color_hex(launcher_mix_color(bg, COLOR_BG_ALT, 1U, 2U)), 0);
	lv_obj_set_style_shadow_opa(btn, LV_OPA_20, 0);
	lv_obj_set_style_outline_width(btn, 0, LV_STATE_FOCUSED);
	lv_obj_set_style_text_color(btn, lv_color_hex(theme->text), 0);
	lv_obj_set_style_text_font(btn, &lv_font_montserrat_24, 0);
	lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);

	lv_label_set_text(label, text);
	lv_obj_center(label);
	if (label_out != NULL) {
		*label_out = label;
	}
	return btn;
}

static void create_ui(void)
{
	const launcher_theme_s *theme = launcher_theme();
	lv_obj_t *scr = lv_scr_act();
	lv_obj_t *page = NULL;
	lv_coord_t track_w = 0;
	uint32_t page_slots = g_page_count == 0U ? 1U : g_page_count;

	g_root_scr = scr;
	launcher_apply_background();
	lv_obj_set_style_border_width(scr, 0, 0);
	clear_static_flags(scr);
	memset(g_hero_summary, 0, sizeof(g_hero_summary));
	memset(g_page_dots, 0, sizeof(g_page_dots));
	g_page_indicator_track = NULL;
	g_launch_btn = NULL;
	g_launch_btn_label = NULL;
	g_ready_chip = NULL;
	g_proc_chip = NULL;
	g_mem_chip = NULL;
	g_task_summary = NULL;
	memset(g_task_row_panel, 0, sizeof(g_task_row_panel));
	memset(g_task_status, 0, sizeof(g_task_status));
	memset(g_task_title, 0, sizeof(g_task_title));
	memset(g_task_body, 0, sizeof(g_task_body));
	g_memory_workspace = NULL;
	g_memory_live = NULL;

	g_app_pager = lv_obj_create(scr);
	lv_obj_set_pos(g_app_pager, LAUNCHER_CARD_X, LAUNCHER_CARD_Y);
	lv_obj_set_size(g_app_pager, LAUNCHER_CARD_W, LAUNCHER_CARD_H);
	lv_obj_set_style_bg_opa(g_app_pager, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(g_app_pager, 0, 0);
	lv_obj_set_style_pad_all(g_app_pager, 0, 0);
	lv_obj_set_style_radius(g_app_pager, 0, 0);
	lv_obj_set_style_shadow_width(g_app_pager, 0, 0);
	lv_obj_set_scrollbar_mode(g_app_pager, LV_SCROLLBAR_MODE_OFF);
	lv_obj_set_scroll_dir(g_app_pager, g_page_count > 1U ? LV_DIR_HOR : LV_DIR_NONE);
	lv_obj_set_scroll_snap_x(g_app_pager, LV_SCROLL_SNAP_START);
	lv_obj_add_flag(g_app_pager, LV_OBJ_FLAG_SCROLL_ONE);
	lv_obj_add_event_cb(g_app_pager, launcher_pager_event_cb, LV_EVENT_SCROLL, NULL);
	lv_obj_add_event_cb(g_app_pager, launcher_pager_event_cb, LV_EVENT_SCROLL_END, NULL);

	for (uint32_t page_index = 0; page_index < page_slots; page_index++) {
		lv_obj_t *panel = NULL;

		page = lv_obj_create(g_app_pager);
		lv_obj_set_pos(page, (lv_coord_t)(page_index * LAUNCHER_PAGE_STRIDE), 0);
		lv_obj_set_size(page, LAUNCHER_CARD_W, LAUNCHER_CARD_H);
		lv_obj_set_style_bg_opa(page, LV_OPA_TRANSP, 0);
		lv_obj_set_style_border_width(page, 0, 0);
		lv_obj_set_style_pad_all(page, 0, 0);
		lv_obj_set_style_radius(page, 0, 0);
		lv_obj_set_style_shadow_width(page, 0, 0);
		clear_static_flags(page);
		lv_obj_add_flag(page, LV_OBJ_FLAG_SNAPPABLE);
		panel = create_panel(page, 0, 0, LAUNCHER_CARD_W, LAUNCHER_CARD_H, theme->panel, theme->line);
		lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, 0);
		lv_obj_set_style_border_width(panel, 0, 0);

			if (page_index == 0U) {
				char page_text[16] = {0};
				lv_obj_t *hero_panel = NULL;
				lv_obj_t *tasks_panel = NULL;
				lv_obj_t *memory_panel = NULL;

			create_soft_blob(panel, LAUNCHER_CARD_W - 288, -28, 260, 260, COLOR_ACCENT, 10);
			create_soft_blob(panel, LAUNCHER_CARD_W - 182, 96, 140, 140, COLOR_ACCENT_ALT, 12);
			create_chip(panel, 28, 24, "Agent shell", theme->warm_soft, COLOR_WARN);
			snprintf(page_text, sizeof(page_text), "1 / %u", (unsigned int)page_slots);
			create_chip(panel, LAUNCHER_CARD_W - 104, 24, page_text, theme->accent_soft, COLOR_ACCENT);

				hero_panel = create_panel(panel, LAUNCHER_PAGE_INSET, LAUNCHER_PAGE_TOP_ROW_Y,
							 LAUNCHER_PAGE_COL_W, LAUNCHER_PAGE_TOP_ROW_H,
							 theme->panel, theme->line);
				lv_obj_set_style_border_width(hero_panel, 0, 0);
				lv_obj_set_style_shadow_width(hero_panel, 12, 0);
				lv_obj_set_style_shadow_color(hero_panel, lv_color_hex(launcher_mix_color(theme->line, COLOR_BG, 1U, 2U)), 0);
				lv_obj_set_style_shadow_opa(hero_panel, LV_OPA_10, 0);
			create_soft_blob(hero_panel, 256, -22, 150, 150, COLOR_ACCENT, 10);
			create_text_label(hero_panel, 26, 26, 220, &lv_font_montserrat_14, theme->dim,
					 LV_TEXT_ALIGN_LEFT, "Kernel Agent");
			create_text_label(hero_panel, 26, 54, 340, &lv_font_montserrat_32, theme->text,
					 LV_TEXT_ALIGN_LEFT, "Ask, Plan, Launch");
			create_text_label(hero_panel, 26, 98, 260, &lv_font_montserrat_14, theme->dim,
					 LV_TEXT_ALIGN_LEFT, "Agent-first desktop");
				g_hero_summary[0] = create_text_label(hero_panel, 26, 138, 340,
								 &lv_font_montserrat_16, theme->dim,
								 LV_TEXT_ALIGN_LEFT,
								 "Assistant will route the next move when tools are ready.");
				g_ready_chip = create_chip(hero_panel, 26, 206, "Ready 0", theme->panel_alt, theme->dim);
				g_proc_chip = create_chip(hero_panel, 116, 206, "Sync 0", theme->panel_alt, theme->dim);
				g_mem_chip = create_chip(hero_panel, 202, 206, "Deck 0", theme->accent_soft, COLOR_ACCENT);
				g_launch_btn = create_button(hero_panel, 26, 258, 300, 56, "Open Assistant",
							      COLOR_ACCENT, launch_primary_event_cb,
							      NULL, &g_launch_btn_label);
				lv_obj_set_style_text_color(g_launch_btn, lv_color_hex(theme->text), 0);
				create_agent_visual(panel,
						    (lv_coord_t)(LAUNCHER_PAGE_INSET + LAUNCHER_PAGE_COL_W + LAUNCHER_PAGE_COL_GAP),
						    LAUNCHER_PAGE_TOP_ROW_Y,
						    LAUNCHER_PAGE_COL_W,
						    LAUNCHER_PAGE_TOP_ROW_H);

				tasks_panel = create_panel(panel, LAUNCHER_PAGE_INSET, LAUNCHER_PAGE_BOTTOM_ROW_Y,
							   LAUNCHER_PAGE_COL_W, LAUNCHER_PAGE_BOTTOM_ROW_H,
							   theme->panel, theme->line);
				lv_obj_set_style_border_width(tasks_panel, 0, 0);
				create_soft_blob(tasks_panel, 20, 18, 72, 6, COLOR_ACCENT, LV_OPA_70);
				create_text_label(tasks_panel, 20, 32, 220, &lv_font_montserrat_14, theme->dim,
						 LV_TEXT_ALIGN_LEFT, "Running now");
				g_task_summary = create_text_label(tasks_panel, 258, 32, 134,
								   &lv_font_montserrat_14, COLOR_ACCENT_ALT,
								   LV_TEXT_ALIGN_RIGHT, "0 active");
				create_task_row(tasks_panel, 0U, 72, 0U);
				create_task_row(tasks_panel, 1U, 126, 0U);
				create_task_row(tasks_panel, 2U, 180, 0U);

				memory_panel = create_panel(panel,
							    (lv_coord_t)(LAUNCHER_PAGE_INSET + LAUNCHER_PAGE_COL_W + LAUNCHER_PAGE_COL_GAP),
							    LAUNCHER_PAGE_BOTTOM_ROW_Y,
							    LAUNCHER_PAGE_COL_W,
							    LAUNCHER_PAGE_BOTTOM_ROW_H,
							    theme->panel, theme->line);
				lv_obj_set_style_border_width(memory_panel, 0, 0);
				create_soft_blob(memory_panel, 20, 18, 72, 6, COLOR_ACCENT_ALT, LV_OPA_70);
				create_text_label(memory_panel, 20, 32, 220, &lv_font_montserrat_14, theme->dim,
						 LV_TEXT_ALIGN_LEFT, "Workspace memory");
				create_note_row(memory_panel, 60, "Layout", COLOR_ACCENT,
					       "0 tools staged in the workspace.",
					       392, &g_memory_workspace, 1U);
				create_note_row(memory_panel, 154, "Live", COLOR_ACCENT_ALT,
					       "Nothing is ready yet. This desk is waiting for app packages.",
					       392, &g_memory_live, 0U);
			continue;
		}

		{
			char page_text[16] = {0};
			uint32_t app_page_index = page_index - 1U;

			create_soft_blob(panel, LAUNCHER_CARD_W - 236, 18, 188, 188, COLOR_ACCENT, 30);
			create_soft_blob(panel, LAUNCHER_CARD_W - 132, 84, 104, 104, COLOR_ACCENT_ALT, 40);
			create_chip(panel, LAUNCHER_HEADER_X, 24, "Tool deck", theme->warm_soft, COLOR_WARN);
			snprintf(page_text, sizeof(page_text), "%u / %u",
				 (unsigned int)(page_index + 1U), (unsigned int)page_slots);
			create_chip(panel, LAUNCHER_CARD_W - 104, 24, page_text, theme->accent_soft, COLOR_ACCENT);

				for (uint32_t slot = 0; slot < LAUNCHER_APPS_PER_PAGE; slot++) {
				uint32_t app_index = g_page_app_indices[app_page_index][slot];
				lv_coord_t col = (lv_coord_t)(slot % 2U);
				lv_coord_t row = (lv_coord_t)(slot / 2U);
				lv_coord_t tile_x = (lv_coord_t)(col * (LAUNCHER_TILE_W + LAUNCHER_TILE_COL_GAP));
				lv_coord_t tile_y = (lv_coord_t)(row * (LAUNCHER_TILE_H + LAUNCHER_TILE_ROW_GAP));

				if (app_index == LAUNCHER_APP_SLOT_EMPTY || app_index >= g_app_count) {
					continue;
				}

				create_app_tile(panel,
					       (lv_coord_t)(LAUNCHER_PAGE_VIEW_X + tile_x),
					       (lv_coord_t)(LAUNCHER_PAGE_VIEW_Y + tile_y),
					       LAUNCHER_TILE_W, LAUNCHER_TILE_H, &g_apps[app_index]);
			}
		}
	}

	track_w = launcher_indicator_track_width();
	g_page_indicator_track = lv_obj_create(scr);
	lv_obj_set_pos(g_page_indicator_track,
		       (lv_coord_t)((APP_DEFAULT_WIDTH - track_w) / 2),
		       LAUNCHER_PAGE_INDICATOR_Y);
	lv_obj_set_size(g_page_indicator_track, track_w, LAUNCHER_PAGE_INDICATOR_H);
	lv_obj_set_style_radius(g_page_indicator_track, 999, 0);
	lv_obj_set_style_bg_color(g_page_indicator_track, lv_color_hex(theme->panel_alt), 0);
	lv_obj_set_style_bg_opa(g_page_indicator_track, LV_OPA_COVER, 0);
	lv_obj_set_style_border_color(g_page_indicator_track, lv_color_hex(theme->line), 0);
	lv_obj_set_style_border_width(g_page_indicator_track, 1, 0);
	lv_obj_set_style_shadow_width(g_page_indicator_track, 0, 0);
	lv_obj_set_style_pad_all(g_page_indicator_track, 0, 0);
	clear_static_flags(g_page_indicator_track);

	for (uint32_t i = 0; i < LAUNCHER_PAGE_COUNT_MAX; i++) {
		lv_obj_t *dot = lv_obj_create(g_page_indicator_track);

		g_page_dots[i] = dot;
		lv_obj_set_pos(dot, 0, 0);
		lv_obj_set_size(dot, LAUNCHER_PAGE_DOT_INACTIVE_W, LAUNCHER_PAGE_DOT_H);
		lv_obj_set_style_radius(dot, 999, 0);
		lv_obj_set_style_bg_color(dot, lv_color_hex(theme->line), 0);
		lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
		lv_obj_set_style_border_width(dot, 0, 0);
		lv_obj_set_style_shadow_width(dot, 0, 0);
		clear_static_flags(dot);
	}
	launcher_update_page_indicator(g_active_page);
	lv_obj_scroll_to_x(g_app_pager, (lv_coord_t)(g_active_page * LAUNCHER_PAGE_STRIDE), LV_ANIM_OFF);
}

static void launcher_on_create(app_s *app)
{
	(void)app;

	g_systemd = systemd_client_get();
	g_fs = fs_client_get();
	g_appmgr = appmgr_client_get();
	g_statemgr = statemgr_client_get();
	launcher_load_apps();
	create_ui();
	launcher_refresh_appearance(0U, 1U);
	{
		uint64_t mono_ms = OSSysCtrlGetMonoTime() / NSEC_PER_MSEC;
		refresh_metrics(mono_ms, 1);
		launcher_notify_boot_animation_exit(mono_ms);
	}
	log_info("launcher ready\n");
}

static void launcher_on_update(app_s *app, uint64_t mono_ms)
{
	(void)app;
	launcher_notify_boot_animation_exit(mono_ms);
	launcher_refresh_appearance(mono_ms, 0U);
	refresh_metrics(mono_ms, 0);
}

int main(int argc, char *argv[])
{
	app_s app = {0};
	app_config_s config = {
		.name = "launcher",
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

	log_info("launcher start!\n");

	lifecycle.on_create = launcher_on_create;
	lifecycle.on_update = launcher_on_update;
	app_init(&app, &config, &lifecycle, NULL);
	return app_run(&app);
}
