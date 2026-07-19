#include "lvgl.h"
#include "app.h"
#include "log.h"
#include "libkernel/capcall.h"
#include "libsystem/ipc.h"
#include "libwindow/window.h"
#include "stdio.h"

#define WIDTH 1024
#define HEIGHT 1024
#define BOOT_WINDOW_Z 160
#define COLOR_BG             0xfff4ee
#define COLOR_BG_ALT         0xffe5d8
#define COLOR_PANEL          0xfff7f1
#define COLOR_PANEL_ALT      0xfffffb
#define COLOR_RING_BG        0xf0ddd2
#define COLOR_RING_ACCENT    0xff7d5c
#define COLOR_TEXT           0x24324a
#define COLOR_DIM            0x7e6d68
#define COLOR_LINE           0xf0ddd2
#define COLOR_LOGO_CARD      0xfffffb
#define COLOR_LOGO_DARK      0x24324a
#define COLOR_LOGO_SOFT      0xe8d8d2
#define COLOR_LOGO_ACCENT    0xff7d5c
#define COLOR_HALO_PRIMARY   0xffd7c7
#define COLOR_HALO_SECONDARY 0xffeddc
#define LOGO_TILE_SIZE       284
#define LOGO_TILE_RADIUS     70

static uint8_t g_exit_requested = 0;
static const lv_point_t g_logo_mark_points[] = {
	{42, 156},
	{98, 78},
	{138, 132},
	{200, 48},
	{236, 158},
};
static const lv_point_t g_logo_base_points[] = {
	{30, 188},
	{254, 188},
};
static const lv_point_t g_logo_accent_points[] = {
	{54, 214},
	{78, 206},
	{106, 199},
	{136, 195},
	{168, 194},
	{198, 195},
	{226, 199},
	{250, 206},
	{272, 214},
};

IPC_ENDPOINT void boot_animation_service_entry(uint64_t cref, uint64_t method,
					       uint64_t arg1, uint64_t arg2, uint64_t arg3)
{
	(void)cref;
	(void)arg1;
	(void)arg2;
	(void)arg3;

	if (method == IPC_BOOT_ANIMATION_SERVICE_FUNCTION_EXIT) {
		g_exit_requested = 1;
		OSIpcEndPointPoolReply(1);
	} else {
		OSIpcEndPointPoolReply(0);
	}

	while (1) {}
}

static void boot_animation_publish_service(void)
{
	uint64_t ret = sys_register_service_pool(IPC_BOOT_ANIMATION_SERVICE_ID,
						 &boot_animation_service_entry);

	if (ret == 0) {
		log_warn("boot animation service registration failed\n");
		return;
	}

	log_info("boot animation service published\n");
}

static void clear_static_flags(lv_obj_t *obj)
{
	if (obj == NULL) {
		return;
	}

	lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE |
			       LV_OBJ_FLAG_CLICK_FOCUSABLE);
}

static void boot_animation_anim_translate_y(void *obj, int32_t value)
{
	lv_obj_set_style_translate_y((lv_obj_t *)obj, (lv_coord_t)value, 0);
}

static void boot_animation_anim_opa(void *obj, int32_t value)
{
	lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)value, 0);
}

static void boot_animation_reveal(lv_obj_t *obj, uint32_t delay_ms, lv_coord_t start_y,
				  uint32_t duration_ms)
{
	lv_anim_t anim = {0};

	if (obj == NULL) {
		return;
	}

	lv_obj_set_style_translate_y(obj, start_y, 0);
	lv_obj_set_style_opa(obj, LV_OPA_TRANSP, 0);

	lv_anim_init(&anim);
	lv_anim_set_var(&anim, obj);
	lv_anim_set_exec_cb(&anim, boot_animation_anim_translate_y);
	lv_anim_set_values(&anim, start_y, 0);
	lv_anim_set_time(&anim, duration_ms);
	lv_anim_set_delay(&anim, delay_ms);
	lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
	lv_anim_start(&anim);

	lv_anim_init(&anim);
	lv_anim_set_var(&anim, obj);
	lv_anim_set_exec_cb(&anim, boot_animation_anim_opa);
	lv_anim_set_values(&anim, LV_OPA_TRANSP, LV_OPA_COVER);
	lv_anim_set_time(&anim, duration_ms - 80U);
	lv_anim_set_delay(&anim, delay_ms);
	lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
	lv_anim_start(&anim);
}

static lv_obj_t *create_logo_tile(lv_obj_t *parent)
{
	lv_obj_t *tile = NULL;
	lv_obj_t *mark = NULL;
	lv_obj_t *base = NULL;
	lv_obj_t *accent = NULL;

	tile = lv_obj_create(parent);
	lv_obj_set_size(tile, LOGO_TILE_SIZE, LOGO_TILE_SIZE);
	lv_obj_set_style_radius(tile, LOGO_TILE_RADIUS, 0);
	lv_obj_set_style_bg_color(tile, lv_color_hex(COLOR_LOGO_CARD), 0);
	lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
	lv_obj_set_style_border_width(tile, 0, 0);
	lv_obj_set_style_border_color(tile, lv_color_hex(COLOR_LINE), 0);
	lv_obj_set_style_shadow_width(tile, 0, 0);
	lv_obj_set_style_pad_all(tile, 0, 0);
	clear_static_flags(tile);

	mark = lv_line_create(tile);
	lv_obj_set_size(mark, LOGO_TILE_SIZE, LOGO_TILE_SIZE);
	lv_line_set_points(mark, g_logo_mark_points,
			   sizeof(g_logo_mark_points) / sizeof(g_logo_mark_points[0]));
	lv_obj_set_style_line_width(mark, 16, 0);
	lv_obj_set_style_line_color(mark, lv_color_hex(COLOR_LOGO_DARK), 0);
	lv_obj_set_style_line_rounded(mark, true, 0);
	lv_obj_set_style_bg_opa(mark, LV_OPA_TRANSP, 0);
	clear_static_flags(mark);

	base = lv_line_create(tile);
	lv_obj_set_size(base, LOGO_TILE_SIZE, LOGO_TILE_SIZE);
	lv_line_set_points(base, g_logo_base_points,
			   sizeof(g_logo_base_points) / sizeof(g_logo_base_points[0]));
	lv_obj_set_style_line_width(base, 8, 0);
	lv_obj_set_style_line_color(base, lv_color_hex(COLOR_LOGO_SOFT), 0);
	lv_obj_set_style_line_rounded(base, true, 0);
	lv_obj_set_style_bg_opa(base, LV_OPA_TRANSP, 0);
	clear_static_flags(base);

	accent = lv_line_create(tile);
	lv_obj_set_size(accent, LOGO_TILE_SIZE, LOGO_TILE_SIZE);
	lv_line_set_points(accent, g_logo_accent_points,
			   sizeof(g_logo_accent_points) / sizeof(g_logo_accent_points[0]));
	lv_obj_set_style_line_width(accent, 10, 0);
	lv_obj_set_style_line_color(accent, lv_color_hex(COLOR_LOGO_ACCENT), 0);
	lv_obj_set_style_line_rounded(accent, true, 0);
	lv_obj_set_style_bg_opa(accent, LV_OPA_TRANSP, 0);
	clear_static_flags(accent);

	return tile;
}

static void create_ui(void)
{
	lv_obj_t *scr = lv_scr_act();
	lv_obj_t *halo = NULL;
	lv_obj_t *halo_back = NULL;
	lv_obj_t *panel = NULL;
	lv_obj_t *logo_tile = NULL;
	lv_obj_t *badge = NULL;
	lv_obj_t *status_row = NULL;
	lv_obj_t *spinner = NULL;

	lv_obj_set_style_bg_color(scr, lv_color_hex(COLOR_PANEL), 0);
	lv_obj_set_style_bg_grad_color(scr, lv_color_hex(COLOR_PANEL_ALT), 0);
	lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, 0);
	lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
	lv_obj_set_style_border_width(scr, 0, 0);
	lv_obj_set_style_pad_all(scr, 0, 0);
	lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
	clear_static_flags(scr);

	halo_back = lv_obj_create(scr);
	lv_obj_set_size(halo_back, 420, 420);
	lv_obj_center(halo_back);
	lv_obj_set_y(halo_back, -138);
	lv_obj_set_style_radius(halo_back, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_bg_color(halo_back, lv_color_hex(COLOR_HALO_SECONDARY), 0);
	lv_obj_set_style_bg_opa(halo_back, LV_OPA_20, 0);
	lv_obj_set_style_border_width(halo_back, 0, 0);
	lv_obj_set_style_shadow_width(halo_back, 0, 0);
	clear_static_flags(halo_back);

	halo = lv_obj_create(scr);
	lv_obj_set_size(halo, 336, 336);
	lv_obj_center(halo);
	lv_obj_set_y(halo, -118);
	lv_obj_set_style_radius(halo, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_bg_color(halo, lv_color_hex(COLOR_HALO_PRIMARY), 0);
	lv_obj_set_style_bg_opa(halo, LV_OPA_20, 0);
	lv_obj_set_style_border_width(halo, 0, 0);
	lv_obj_set_style_shadow_width(halo, 0, 0);
	clear_static_flags(halo);

	panel = lv_obj_create(scr);
	lv_obj_set_size(panel, 600, 620);
	lv_obj_center(panel);
	lv_obj_set_style_radius(panel, 42, 0);
	lv_obj_set_style_bg_color(panel, lv_color_hex(COLOR_PANEL), 0);
	lv_obj_set_style_bg_grad_color(panel, lv_color_hex(COLOR_PANEL_ALT), 0);
	lv_obj_set_style_bg_grad_dir(panel, LV_GRAD_DIR_VER, 0);
	lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
	lv_obj_set_style_border_color(panel, lv_color_hex(COLOR_LINE), 0);
	lv_obj_set_style_border_width(panel, 0, 0);
	lv_obj_set_style_pad_all(panel, 0, 0);
	lv_obj_set_style_shadow_width(panel, 0, 0);
	clear_static_flags(panel);

	logo_tile = create_logo_tile(panel);
	lv_obj_align(logo_tile, LV_ALIGN_TOP_MID, 0, 26);

	badge = lv_label_create(panel);
	lv_obj_set_width(badge, 320);
	lv_obj_align(badge, LV_ALIGN_TOP_MID, 0, 480);
	lv_obj_set_style_text_font(badge, &lv_font_montserrat_24, 0);
	lv_obj_set_style_text_align(badge, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_style_text_color(badge, lv_color_hex(COLOR_RING_ACCENT), 0);
	lv_label_set_text(badge, "TranquilOS");

	status_row = lv_obj_create(panel);
	lv_obj_set_size(status_row, 120, 104);
	lv_obj_align(status_row, LV_ALIGN_TOP_MID, 0, 500);
	lv_obj_set_style_bg_opa(status_row, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(status_row, 0, 0);
	lv_obj_set_style_shadow_width(status_row, 0, 0);
	lv_obj_set_style_pad_all(status_row, 0, 0);
	lv_obj_set_layout(status_row, LV_LAYOUT_FLEX);
	lv_obj_set_flex_flow(status_row, LV_FLEX_FLOW_ROW);
	lv_obj_set_flex_align(status_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
			      LV_FLEX_ALIGN_CENTER);
	clear_static_flags(status_row);

	spinner = lv_spinner_create(status_row, 1100, 80);
	lv_obj_set_size(spinner, 88, 88);
	lv_obj_set_style_arc_width(spinner, 8, LV_PART_MAIN);
	lv_obj_set_style_arc_width(spinner, 8, LV_PART_INDICATOR);
	lv_obj_set_style_arc_color(spinner, lv_color_hex(COLOR_RING_BG), LV_PART_MAIN);
	lv_obj_set_style_arc_color(spinner, lv_color_hex(COLOR_RING_ACCENT), LV_PART_INDICATOR);
	lv_obj_set_style_bg_opa(spinner, LV_OPA_TRANSP, 0);
	lv_obj_set_style_translate_y(spinner, 10, 0);
	clear_static_flags(spinner);

	boot_animation_reveal(halo_back, 0U, 14, 520U);
	boot_animation_reveal(halo, 40U, 18, 520U);
	boot_animation_reveal(panel, 80U, 26, 560U);
	boot_animation_reveal(logo_tile, 150U, 18, 420U);
	boot_animation_reveal(badge, 210U, 18, 380U);
	boot_animation_reveal(status_row, 380U, 12, 360U);
}

static void boot_animation_on_create(app_s *app)
{
	(void)app;
	create_ui();
	boot_animation_publish_service();
	log_info("boot animation ready\n");
}

static void boot_animation_on_update(app_s *app, uint64_t mono_ms)
{
	if (app == NULL) {
		return;
	}

	if (!g_exit_requested) {
		return;
	}

	log_info("launcher ready signal received, exiting boot animation\n");
	app_request_exit(app, 0);
}

int main(int argc, char *argv[])
{
	app_s app = {0};
	app_config_s config = {
		.name = "boot_animation",
		.x = 0,
		.y = 0,
		.z = BOOT_WINDOW_Z,
		.width = WIDTH,
		.height = HEIGHT,
		.window_flags = WINDOW_FLAG_FOCUSABLE,
		.enable_input = 0,
	};
	app_lifecycle_ops_s lifecycle = {0};

	(void)argc;
	(void)argv;

	log_info("boot animation start\n");
	lifecycle.on_create = boot_animation_on_create;
	lifecycle.on_update = boot_animation_on_update;
	app_init(&app, &config, &lifecycle, NULL);
	return app_run(&app);
}
