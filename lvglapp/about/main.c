#include "lvgl.h"
#include "app.h"
#include "libsystem/statemgr_client.h"
#include "libwindow/window.h"
#include "log.h"
#include "stdint.h"

#define ABOUT_SYSTEM_NAME "TranquilOS"
#define ABOUT_SYSTEM_VERSION "v1.4.11(NeoYang)"
#define ABOUT_SYSTEM_TAGLINE "Lightweight AArch64 microkernel operating system"
#define ABOUT_SYSTEM_SUMMARY "A lightweight AArch64 system that keeps the kernel focused on isolation, scheduling, and IPC while higher-level services and apps stay in userspace."

#define COLOR_BG             0xfff4ee
#define COLOR_BG_ALT         0xeaf4ff
#define COLOR_PANEL          0xfffffb
#define COLOR_PANEL_ALT      0xfff1eb
#define COLOR_PANEL_SOFT     0xe8f8f0
#define COLOR_LINE           0xf0ddd2
#define COLOR_TEXT           0x24324a
#define COLOR_DIM            0x7d8198
#define COLOR_ACCENT         0xff7d5c
#define COLOR_ACCENT_SOFT    0xffe6dd
#define COLOR_WARM           0xffb85e
#define COLOR_WARM_SOFT      0xfff0d2
#define COLOR_DARK           0x24324a
#define COLOR_DARK_SOFT      0xe3ebf3
#define COLOR_HALO_PRIMARY   0xc6f2df
#define COLOR_HALO_SECONDARY 0xffdfd6

#define ABOUT_CONTENT_X 40
#define ABOUT_CONTENT_Y 28
#define ABOUT_HERO_W    320
#define ABOUT_PANEL_GAP 16
#define ABOUT_SIDE_W    (APP_DEFAULT_WIDTH - ABOUT_CONTENT_X * 2 - ABOUT_HERO_W - ABOUT_PANEL_GAP)
#define ABOUT_PANEL_H   760
#define ABOUT_SIDE_X    (ABOUT_CONTENT_X + ABOUT_HERO_W + ABOUT_PANEL_GAP)
#define THEME_REFRESH_INTERVAL_MS 100ULL

#define LOGO_TILE_SIZE   248
#define LOGO_TILE_RADIUS 64

static const lv_point_t g_logo_mark_points[] = {
	{36, 138},
	{86, 70},
	{122, 116},
	{176, 44},
	{210, 140},
};

static const lv_point_t g_logo_base_points[] = {
	{26, 168},
	{222, 168},
};

static const lv_point_t g_logo_accent_points[] = {
	{44, 190},
	{66, 183},
	{92, 177},
	{120, 173},
	{148, 172},
	{174, 173},
	{198, 177},
	{220, 183},
	{238, 190},
};

typedef struct about_card_spec {
	const char *title;
	const char *body;
	const char *chip;
	uint8_t warm_chip;
} about_card_spec_s;

typedef enum about_theme_mode {
	ABOUT_THEME_LIGHT = 0,
	ABOUT_THEME_DARK = 1,
} about_theme_mode_e;

typedef struct about_theme {
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
	uint32_t logo_mark;
	uint32_t logo_base;
	uint32_t halo_primary;
	uint32_t halo_secondary;
} about_theme_s;

static const about_card_spec_s g_about_cards[] = {
	{
		.title = "Kernel",
		.body = "Privilege-aware microkernel services cover virtual memory, timers, scheduling, capabilities, interrupts, IPC, and upcalls on AArch64.",
		.chip = "AArch64 core",
		.warm_chip = 0U,
	},
	{
		.title = "Userspace",
		.body = "Device, memory, process, app, state, input, and window services are composed in userspace so desktop features can evolve without growing the kernel.",
		.chip = "Service-oriented",
		.warm_chip = 1U,
	},
};

static statemgr_client_s *g_statemgr = NULL;
static uint32_t g_theme_mode = ABOUT_THEME_LIGHT;
static uint64_t g_last_theme_ms = 0;
static uint64_t g_last_theme_revision = 0;

static const about_theme_s g_about_theme_light = {
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
	.logo_mark = COLOR_DARK,
	.logo_base = COLOR_DARK_SOFT,
	.halo_primary = COLOR_HALO_PRIMARY,
	.halo_secondary = COLOR_HALO_SECONDARY,
};

static const about_theme_s g_about_theme_dark = {
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
	.logo_mark = 0xfff6ef,
	.logo_base = 0x8189a4,
	.halo_primary = 0x2b564c,
	.halo_secondary = 0x47303a,
};

static const about_theme_s *about_theme(void)
{
	return g_theme_mode == ABOUT_THEME_DARK ?
		&g_about_theme_dark : &g_about_theme_light;
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
	lv_obj_set_style_radius(panel, 30, 0);
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

static lv_obj_t *create_logo_tile(lv_obj_t *parent, lv_coord_t x, lv_coord_t y)
{
	const about_theme_s *theme = about_theme();
	lv_obj_t *tile = lv_obj_create(parent);
	lv_obj_t *mark = NULL;
	lv_obj_t *base = NULL;
	lv_obj_t *accent = NULL;

	lv_obj_set_pos(tile, x, y);
	lv_obj_set_size(tile, LOGO_TILE_SIZE, LOGO_TILE_SIZE);
	lv_obj_set_style_radius(tile, LOGO_TILE_RADIUS, 0);
	lv_obj_set_style_bg_color(tile, lv_color_hex(theme->panel_alt), 0);
	lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
	lv_obj_set_style_border_width(tile, 0, 0);
	lv_obj_set_style_shadow_width(tile, 0, 0);
	lv_obj_set_style_pad_all(tile, 0, 0);
	clear_static_flags(tile);

	mark = lv_line_create(tile);
	lv_obj_set_size(mark, LOGO_TILE_SIZE, LOGO_TILE_SIZE);
	lv_line_set_points(mark, g_logo_mark_points,
			   sizeof(g_logo_mark_points) / sizeof(g_logo_mark_points[0]));
	lv_obj_set_style_line_width(mark, 15, 0);
	lv_obj_set_style_line_color(mark, lv_color_hex(theme->logo_mark), 0);
	lv_obj_set_style_line_rounded(mark, true, 0);
	lv_obj_set_style_bg_opa(mark, LV_OPA_TRANSP, 0);
	clear_static_flags(mark);

	base = lv_line_create(tile);
	lv_obj_set_size(base, LOGO_TILE_SIZE, LOGO_TILE_SIZE);
	lv_line_set_points(base, g_logo_base_points,
			   sizeof(g_logo_base_points) / sizeof(g_logo_base_points[0]));
	lv_obj_set_style_line_width(base, 8, 0);
	lv_obj_set_style_line_color(base, lv_color_hex(theme->logo_base), 0);
	lv_obj_set_style_line_rounded(base, true, 0);
	lv_obj_set_style_bg_opa(base, LV_OPA_TRANSP, 0);
	clear_static_flags(base);

	accent = lv_line_create(tile);
	lv_obj_set_size(accent, LOGO_TILE_SIZE, LOGO_TILE_SIZE);
	lv_line_set_points(accent, g_logo_accent_points,
			   sizeof(g_logo_accent_points) / sizeof(g_logo_accent_points[0]));
	lv_obj_set_style_line_width(accent, 10, 0);
	lv_obj_set_style_line_color(accent, lv_color_hex(theme->accent), 0);
	lv_obj_set_style_line_rounded(accent, true, 0);
	lv_obj_set_style_bg_opa(accent, LV_OPA_TRANSP, 0);
	clear_static_flags(accent);

	return tile;
}

static void create_info_card(lv_obj_t *parent, lv_coord_t y, const about_card_spec_s *spec)
{
	const about_theme_s *theme = about_theme();
	lv_obj_t *card = NULL;
	lv_obj_t *title = NULL;
	lv_obj_t *body = NULL;

	if (parent == NULL || spec == NULL) {
		return;
	}

	card = create_panel(parent, 24, y, ABOUT_SIDE_W - 48, 154, theme->panel_alt, theme->line);
	title = lv_label_create(card);
	lv_obj_set_pos(title, 22, 22);
	lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
	lv_obj_set_style_text_color(title, lv_color_hex(theme->text), 0);
	lv_label_set_text(title, spec->title);
	clear_static_flags(title);

	body = lv_label_create(card);
	lv_obj_set_pos(body, 22, 58);
	lv_obj_set_width(body, ABOUT_SIDE_W - 92);
	lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(body, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(body, lv_color_hex(theme->dim), 0);
	lv_label_set_text(body, spec->body);
	clear_static_flags(body);

	(void)create_chip(card, 22, 114, spec->chip,
			  spec->warm_chip ? theme->warm_soft : theme->accent_soft,
			  spec->warm_chip ? theme->warm : theme->accent);
}

static uint8_t about_get_theme_revision(uint64_t *revision_out)
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

static uint8_t about_read_theme_mode(uint32_t *theme_mode_out)
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

	*theme_mode_out = (uint32_t)response.entry.value_u64 == ABOUT_THEME_DARK ?
		ABOUT_THEME_DARK : ABOUT_THEME_LIGHT;
	return 1U;
}

static void create_ui(void)
{
	const about_theme_s *theme = about_theme();
	lv_obj_t *scr = lv_scr_act();
	lv_obj_t *halo_back = NULL;
	lv_obj_t *halo = NULL;
	lv_obj_t *hero = NULL;
	lv_obj_t *side = NULL;
	lv_obj_t *label = NULL;
	lv_obj_t *summary = NULL;
	lv_obj_t *meta_panel = NULL;
	lv_obj_t *meta_value = NULL;

	lv_obj_set_style_bg_color(scr, lv_color_hex(theme->bg), 0);
	lv_obj_set_style_bg_grad_color(scr, lv_color_hex(theme->bg_alt), 0);
	lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, 0);
	lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
	lv_obj_set_style_border_width(scr, 0, 0);
	lv_obj_set_style_pad_all(scr, 0, 0);
	lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
	clear_static_flags(scr);

	halo_back = lv_obj_create(scr);
	lv_obj_set_pos(halo_back, 84, 68);
	lv_obj_set_size(halo_back, 280, 280);
	lv_obj_set_style_radius(halo_back, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_bg_color(halo_back, lv_color_hex(theme->halo_secondary), 0);
	lv_obj_set_style_bg_opa(halo_back, LV_OPA_30, 0);
	lv_obj_set_style_border_width(halo_back, 0, 0);
	lv_obj_set_style_shadow_width(halo_back, 0, 0);
	clear_static_flags(halo_back);

	halo = lv_obj_create(scr);
	lv_obj_set_pos(halo, 112, 96);
	lv_obj_set_size(halo, 224, 224);
	lv_obj_set_style_radius(halo, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_bg_color(halo, lv_color_hex(theme->halo_primary), 0);
	lv_obj_set_style_bg_opa(halo, LV_OPA_30, 0);
	lv_obj_set_style_border_width(halo, 0, 0);
	lv_obj_set_style_shadow_width(halo, 0, 0);
	clear_static_flags(halo);

	hero = create_panel(scr, ABOUT_CONTENT_X, ABOUT_CONTENT_Y, ABOUT_HERO_W, ABOUT_PANEL_H,
			    theme->panel, theme->line);
	side = create_panel(scr, ABOUT_SIDE_X, ABOUT_CONTENT_Y, ABOUT_SIDE_W, ABOUT_PANEL_H,
			    theme->panel, theme->line);

	(void)create_chip(hero, 24, 24, "SYSTEM", theme->accent_soft, theme->accent);
	(void)create_logo_tile(hero, 36, 82);

	label = lv_label_create(hero);
	lv_obj_set_pos(label, 24, 356);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_32, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->text), 0);
	lv_label_set_text(label, ABOUT_SYSTEM_NAME);
	clear_static_flags(label);

	label = lv_label_create(hero);
	lv_obj_set_pos(label, 24, 406);
	lv_obj_set_width(label, ABOUT_HERO_W - 48);
	lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->dim), 0);
	lv_label_set_text(label, ABOUT_SYSTEM_TAGLINE);
	clear_static_flags(label);

	meta_panel = create_panel(hero, 24, 494, ABOUT_HERO_W - 48, 92, theme->panel_alt, theme->line);
	label = lv_label_create(meta_panel);
	lv_obj_set_pos(label, 18, 16);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->dim), 0);
	lv_label_set_text(label, "Version");
	clear_static_flags(label);

	meta_value = lv_label_create(meta_panel);
	lv_obj_set_pos(meta_value, 18, 38);
	lv_obj_set_style_text_font(meta_value, &lv_font_montserrat_24, 0);
	lv_obj_set_style_text_color(meta_value, lv_color_hex(theme->text), 0);
	lv_label_set_text(meta_value, ABOUT_SYSTEM_VERSION);
	clear_static_flags(meta_value);

	(void)create_chip(side, 24, 24, "ABOUT", theme->warm_soft, theme->warm);

	label = lv_label_create(side);
	lv_obj_set_pos(label, 24, 72);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_32, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(theme->text), 0);
	lv_label_set_text(label, "System overview");
	clear_static_flags(label);

	summary = lv_label_create(side);
	lv_obj_set_pos(summary, 24, 126);
	lv_obj_set_width(summary, ABOUT_SIDE_W - 48);
	lv_label_set_long_mode(summary, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(summary, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(summary, lv_color_hex(theme->dim), 0);
	lv_label_set_text(summary, ABOUT_SYSTEM_SUMMARY);
	clear_static_flags(summary);

	(void)create_chip(side, 24, 222, "Microkernel", theme->accent_soft, theme->accent);
	(void)create_chip(side, 144, 222, "Userspace services", theme->panel_soft, theme->accent);

	for (uint32_t i = 0; i < sizeof(g_about_cards) / sizeof(g_about_cards[0]); i++) {
		create_info_card(side, 272 + (lv_coord_t)i * 166, &g_about_cards[i]);
	}
}

static void about_rebuild_ui(void)
{
	lv_obj_clean(lv_scr_act());
	create_ui();
}

static void about_refresh_theme(uint64_t mono_ms)
{
	uint32_t theme_mode = g_theme_mode;
	uint64_t revision = 0;

	if (mono_ms < g_last_theme_ms + THEME_REFRESH_INTERVAL_MS) {
		return;
	}
	g_last_theme_ms = mono_ms;

	if (!about_get_theme_revision(&revision) || !about_read_theme_mode(&theme_mode)) {
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
	about_rebuild_ui();
	log_info("about theme synced: mode=%u rev=%llu\n",
		 (unsigned int)g_theme_mode,
		 (unsigned long long)revision);
}

static void about_on_create(app_s *app)
{
	(void)app;

	g_statemgr = statemgr_client_get();
	(void)about_read_theme_mode(&g_theme_mode);
	(void)about_get_theme_revision(&g_last_theme_revision);
	create_ui();
	log_info("about app ready\n");
}

static void about_on_update(app_s *app, uint64_t mono_ms)
{
	(void)app;
	about_refresh_theme(mono_ms);
}

int main(int argc, char *argv[])
{
	app_s app = {0};
	app_config_s config = {
		.name = "about",
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

	log_info("about start!\n");
	lifecycle.on_create = about_on_create;
	lifecycle.on_update = about_on_update;
	app_init(&app, &config, &lifecycle, NULL);
	return app_run(&app);
}
