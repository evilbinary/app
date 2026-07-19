#include "lvgl.h"
#include "widgets/lv_canvas.h"
#include "app.h"
#include "liblvgl/lv_port_disp.h"
#include "libwindow/window.h"
#include "log.h"
#include "systemui_shared.h"

#define NAV_DOCK_WIDTH 328
#define NAV_DOCK_HEIGHT 72
#define NAV_ITEM_WIDTH 86
#define NAV_ITEM_HEIGHT NAV_DOCK_HEIGHT
#define NAV_ICON_SIZE 40
#define NAV_ICON_CIRCLE_SIZE 26
#define NAV_ICON_SQUARE_SIZE 26

typedef enum nav_icon_shape {
	NAV_ICON_TRIANGLE_LEFT = 0,
	NAV_ICON_CIRCLE = 1,
	NAV_ICON_SQUARE = 2,
} nav_icon_shape_e;

typedef enum nav_item_id {
	NAV_ITEM_BACK = 0,
	NAV_ITEM_HOME = 1,
	NAV_ITEM_RECENT = 2,
	NAV_ITEM_COUNT = 3,
} nav_item_id_e;

typedef struct nav_item {
	lv_obj_t *item;
	lv_obj_t *icon;
	nav_icon_shape_e shape;
	uint8_t id;
} nav_item_s;

static systemui_runtime_state_s g_state = {0};
static lv_obj_t *g_root = NULL;
static lv_obj_t *g_bar = NULL;
static lv_obj_t *g_line = NULL;
static lv_obj_t *g_dock = NULL;
static nav_item_s g_nav_items[NAV_ITEM_COUNT] = {0};
static uint8_t g_nav_triangle_buf[LV_IMG_BUF_SIZE_TRUE_COLOR_ALPHA(NAV_ICON_SIZE, NAV_ICON_SIZE)];

static void clear_static_flags(lv_obj_t *obj)
{
	if (obj == NULL) {
		return;
	}

	lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE |
			       LV_OBJ_FLAG_CLICK_FOCUSABLE);
}

static void navui_draw_triangle(lv_obj_t *canvas, uint32_t color)
{
	lv_draw_rect_dsc_t triangle_dsc;
	static const lv_point_t triangle_points[3] = {
		{9, NAV_ICON_SIZE / 2},
		{31, 8},
		{31, NAV_ICON_SIZE - 8},
	};

	if (canvas == NULL) {
		return;
	}

	lv_canvas_fill_bg(canvas, lv_color_hex(0x000000), LV_OPA_TRANSP);
	lv_draw_rect_dsc_init(&triangle_dsc);
	triangle_dsc.bg_color = lv_color_hex(color);
	triangle_dsc.bg_opa = LV_OPA_COVER;
	lv_canvas_draw_polygon(canvas, triangle_points, 3, &triangle_dsc);
}

static void navui_set_icon_color(nav_item_s *item, uint32_t color)
{
	if (item == NULL || item->icon == NULL) {
		return;
	}

	switch (item->shape) {
		case NAV_ICON_TRIANGLE_LEFT:
			navui_draw_triangle(item->icon, color);
			break;
		case NAV_ICON_CIRCLE:
			lv_obj_set_style_bg_color(item->icon, lv_color_hex(color), 0);
			break;
		case NAV_ICON_SQUARE:
			lv_obj_set_style_bg_color(item->icon, lv_color_hex(color), 0);
			break;
		default:
			break;
	}
}

static void navui_update_item_style(nav_item_s *item, uint8_t pressed)
{
	uint32_t bg_color = 0;
	uint32_t icon_color = 0;

	if (item == NULL || item->item == NULL) {
		return;
	}

	if (pressed) {
		bg_color = g_state.palette.bg;
		icon_color = g_state.palette.panel;
		lv_obj_set_style_bg_opa(item->item, LV_OPA_COVER, 0);
	} else {
		bg_color = g_state.palette.panel;
		icon_color = g_state.palette.bg;
		lv_obj_set_style_bg_opa(item->item, LV_OPA_COVER, 0);
	}

	lv_obj_set_style_shadow_width(item->item, 0, 0);
	lv_obj_set_style_shadow_opa(item->item, LV_OPA_TRANSP, 0);
	lv_obj_set_style_translate_y(item->item, 0, 0);
	lv_obj_set_style_bg_color(item->item, lv_color_hex(bg_color), 0);
	navui_set_icon_color(item, icon_color);
}

static void navui_handle_action(uint8_t id)
{
	if (id == NAV_ITEM_BACK) {
		(void)systemui_request_overlay_action(SYSTEMUI_OVERLAY_PANEL_ALL,
							    SYSTEMUI_OVERLAY_ACTION_CLOSE);
		return;
	}
	if (id == NAV_ITEM_HOME) {
		uint64_t launcher_pid = systemui_lookup_process_pid(SYSTEMUI_LAUNCHER_PROCESS_PATH);
		(void)systemui_request_overlay_action(SYSTEMUI_OVERLAY_PANEL_ALL,
							    SYSTEMUI_OVERLAY_ACTION_CLOSE);
		if (launcher_pid != 0U && !window_activate_owner(launcher_pid)) {
			log_warn("navui: activate launcher failed pid=%d\n", launcher_pid);
		}
		return;
	}
	if (id == NAV_ITEM_RECENT) {
		(void)systemui_request_overlay_action(SYSTEMUI_OVERLAY_PANEL_RECENT,
							    SYSTEMUI_OVERLAY_ACTION_TOGGLE);
	}
}

static void navui_item_event_cb(lv_event_t *e)
{
	lv_event_code_t code = lv_event_get_code(e);
	nav_item_s *item = (nav_item_s *)lv_event_get_user_data(e);

	if (item == NULL) {
		return;
	}

	if (code == LV_EVENT_PRESSED) {
		navui_update_item_style(item, 1U);
		return;
	}
	if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
		navui_update_item_style(item, 0U);
		return;
	}
	if (code == LV_EVENT_CLICKED) {
		navui_update_item_style(item, 0U);
		navui_handle_action(item->id);
	}
}

static void navui_create_item(lv_obj_t *parent, uint8_t id, lv_coord_t x, nav_icon_shape_e shape)
{
	nav_item_s *item = &g_nav_items[id];
	lv_obj_t *shape_obj = NULL;

	memset(item, 0, sizeof(*item));
	item->id = id;
	item->shape = shape;

	item->item = lv_obj_create(parent);
	lv_obj_set_size(item->item, NAV_ITEM_WIDTH, NAV_ITEM_HEIGHT);
	lv_obj_set_pos(item->item, x, 0);
	lv_obj_set_style_radius(item->item, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_pad_all(item->item, 0, 0);
	lv_obj_set_style_border_width(item->item, 0, 0);
	lv_obj_set_style_shadow_width(item->item, 0, 0);
	lv_obj_set_style_outline_width(item->item, 0, 0);
	lv_obj_add_flag(item->item, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_clear_flag(item->item, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICK_FOCUSABLE);
	lv_obj_add_event_cb(item->item, navui_item_event_cb, LV_EVENT_PRESSED, item);
	lv_obj_add_event_cb(item->item, navui_item_event_cb, LV_EVENT_RELEASED, item);
	lv_obj_add_event_cb(item->item, navui_item_event_cb, LV_EVENT_PRESS_LOST, item);
	lv_obj_add_event_cb(item->item, navui_item_event_cb, LV_EVENT_CLICKED, item);

	if (shape == NAV_ICON_TRIANGLE_LEFT) {
		shape_obj = lv_canvas_create(item->item);
		lv_canvas_set_buffer(shape_obj, g_nav_triangle_buf, NAV_ICON_SIZE, NAV_ICON_SIZE,
				     LV_IMG_CF_TRUE_COLOR_ALPHA);
		lv_obj_set_style_bg_opa(shape_obj, LV_OPA_TRANSP, 0);
		lv_obj_set_style_border_width(shape_obj, 0, 0);
		lv_obj_set_style_pad_all(shape_obj, 0, 0);
		lv_obj_center(shape_obj);
	} else {
		shape_obj = lv_obj_create(item->item);
		lv_obj_set_style_radius(shape_obj,
					shape == NAV_ICON_CIRCLE ? LV_RADIUS_CIRCLE : 6, 0);
		lv_obj_set_style_border_width(shape_obj, 0, 0);
		lv_obj_set_style_shadow_width(shape_obj, 0, 0);
		lv_obj_set_style_pad_all(shape_obj, 0, 0);
		lv_obj_set_size(shape_obj,
				shape == NAV_ICON_CIRCLE ? NAV_ICON_CIRCLE_SIZE : NAV_ICON_SQUARE_SIZE,
				shape == NAV_ICON_CIRCLE ? NAV_ICON_CIRCLE_SIZE : NAV_ICON_SQUARE_SIZE);
		lv_obj_center(shape_obj);
	}

	item->icon = shape_obj;
	clear_static_flags(shape_obj);
	navui_update_item_style(item, 0U);
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
	if (g_dock != NULL) {
		lv_obj_set_style_bg_color(g_dock, lv_color_hex(g_state.palette.panel), 0);
		lv_obj_set_style_border_color(g_dock, lv_color_hex(g_state.palette.line), 0);
		lv_obj_set_style_shadow_color(g_dock, lv_color_hex(g_state.palette.accent), 0);
		lv_obj_set_style_shadow_opa(g_dock, 22, 0);
	}
	for (uint32_t i = 0; i < NAV_ITEM_COUNT; i++) {
		navui_update_item_style(&g_nav_items[i], 0U);
	}
}

static void create_ui(void)
{
	lv_obj_t *scr = lv_scr_act();
	window_s *window = NULL;
	window_rect_s input_region = {
		.x = 0,
		.y = 0,
		.width = SYSTEMUI_WIDTH,
		.height = SYSTEMUI_NAV_BAR_HEIGHT,
	};

	g_root = scr;
	lv_obj_set_style_bg_color(scr, lv_color_hex(g_state.palette.bg), 0);
	lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
	lv_obj_set_style_border_width(scr, 0, 0);
	lv_obj_set_style_pad_all(scr, 0, 0);
	clear_static_flags(scr);

	g_bar = lv_obj_create(scr);
	lv_obj_set_size(g_bar, SYSTEMUI_WIDTH, SYSTEMUI_NAV_BAR_HEIGHT);
	lv_obj_set_pos(g_bar, 0, 0);
	lv_obj_set_style_bg_color(g_bar, lv_color_hex(g_state.palette.bg), 0);
	lv_obj_set_style_bg_opa(g_bar, LV_OPA_COVER, 0);
	lv_obj_set_style_border_width(g_bar, 0, 0);
	lv_obj_set_style_radius(g_bar, 0, 0);
	lv_obj_set_style_pad_all(g_bar, 0, 0);
	lv_obj_set_style_shadow_width(g_bar, 0, 0);
	clear_static_flags(g_bar);

	g_line = lv_obj_create(g_bar);
	lv_obj_set_size(g_line, SYSTEMUI_WIDTH - 48, 1);
	lv_obj_align(g_line, LV_ALIGN_TOP_MID, 0, 2);
	lv_obj_set_style_bg_color(g_line, lv_color_hex(g_state.palette.line), 0);
	lv_obj_set_style_bg_opa(g_line, LV_OPA_40, 0);
	lv_obj_set_style_border_width(g_line, 0, 0);
	lv_obj_set_style_radius(g_line, LV_RADIUS_CIRCLE, 0);
	clear_static_flags(g_line);

	g_dock = lv_obj_create(g_bar);
	lv_obj_set_size(g_dock, NAV_DOCK_WIDTH, NAV_DOCK_HEIGHT);
	lv_obj_align(g_dock, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_style_bg_color(g_dock, lv_color_hex(g_state.palette.panel), 0);
	lv_obj_set_style_bg_opa(g_dock, LV_OPA_COVER, 0);
	lv_obj_set_style_border_color(g_dock, lv_color_hex(g_state.palette.line), 0);
	lv_obj_set_style_border_width(g_dock, 1, 0);
	lv_obj_set_style_radius(g_dock, 36, 0);
	lv_obj_set_style_pad_all(g_dock, 0, 0);
	lv_obj_set_style_shadow_width(g_dock, 22, 0);
	clear_static_flags(g_dock);

	navui_create_item(g_dock, NAV_ITEM_BACK, 0, NAV_ICON_TRIANGLE_LEFT);
	navui_create_item(g_dock, NAV_ITEM_HOME,
			  (NAV_DOCK_WIDTH - NAV_ITEM_WIDTH) / 2, NAV_ICON_CIRCLE);
	navui_create_item(g_dock, NAV_ITEM_RECENT,
			  NAV_DOCK_WIDTH - NAV_ITEM_WIDTH, NAV_ICON_SQUARE);

	window = lv_port_disp_get_window();
	if (window != NULL && !window_set_input_region(window, &input_region)) {
		log_warn("navui: set input region failed\n");
	}
}

static void nav_ui_on_create(app_s *app)
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

static void nav_ui_on_update(app_s *app, uint64_t mono_ms)
{
	(void)app;
	if (systemui_sync_appearance(&g_state, mono_ms, 0U) || g_state.appearance_dirty) {
		g_state.appearance_dirty = 0U;
		apply_appearance();
	}
}

int main(int argc, char *argv[])
{
	app_s app = {0};
	app_config_s config = {
		.name = "navui",
		.x = 0,
		.y = SYSTEMUI_HEIGHT - SYSTEMUI_NAV_BAR_HEIGHT,
		.z = SYSTEMUI_NAV_WINDOW_Z,
		.width = SYSTEMUI_WIDTH,
		.height = SYSTEMUI_NAV_BAR_HEIGHT,
		.window_flags = 0U,
		.enable_input = 1U,
	};
	app_lifecycle_ops_s lifecycle = {
		.on_create = nav_ui_on_create,
		.on_update = nav_ui_on_update,
	};

	(void)argc;
	(void)argv;
	log_info("navui start\n");
	app_init(&app, &config, &lifecycle, NULL);
	return app_run(&app);
}
