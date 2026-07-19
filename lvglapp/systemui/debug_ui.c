#include "lvgl.h"
#include "app.h"
#include "liblvgl/lv_port_disp.h"
#include "libwindow/window.h"
#include "log.h"
#include "systemui_shared.h"

#define POINTER_WINDOW_SIZE 48
#define POINTER_CURSOR_SIZE 32
#define POINTER_CURSOR_MARGIN ((POINTER_WINDOW_SIZE - POINTER_CURSOR_SIZE) / 2)
#define POINTER_CURSOR_LIGHT_COLOR 0x7d8594U
#define POINTER_CURSOR_DARK_COLOR 0xc3cad5U
#define POINTER_CURSOR_LIGHT_PRESSED_COLOR 0x5a6170U
#define POINTER_CURSOR_DARK_PRESSED_COLOR 0x98a1adU

static systemui_runtime_state_s g_state = {0};
static lv_obj_t *g_root = NULL;
static lv_obj_t *g_pointer_cursor = NULL;
static uint64_t g_last_input_revision = 0;
static uint64_t g_last_input_ms = 0;
static uint64_t g_last_lock_request_ms = 0;
static uint8_t g_idle_lock_requested = 0U;

static void clear_static_flags(lv_obj_t *obj)
{
	if (obj == NULL) {
		return;
	}

	lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE |
			       LV_OBJ_FLAG_CLICK_FOCUSABLE);
}

static void pointer_cursor_apply_style(uint8_t pressed)
{
	uint32_t color = 0U;
	lv_opa_t opa = LV_OPA_50;

	if (g_pointer_cursor == NULL) {
		return;
	}

	if (g_state.appearance.theme_mode == SYSTEMUI_THEME_MODE_DARK) {
		color = pressed ? POINTER_CURSOR_DARK_PRESSED_COLOR : POINTER_CURSOR_DARK_COLOR;
	} else {
		color = pressed ? POINTER_CURSOR_LIGHT_PRESSED_COLOR : POINTER_CURSOR_LIGHT_COLOR;
	}
	if (pressed) {
		opa = LV_OPA_70;
	}

	lv_obj_set_style_bg_color(g_pointer_cursor, lv_color_hex(color), 0);
	lv_obj_set_style_bg_opa(g_pointer_cursor, opa, 0);
}

static void apply_appearance(void)
{
	if (g_root != NULL) {
		lv_obj_set_style_bg_opa(g_root, LV_OPA_TRANSP, 0);
	}
	if (g_pointer_cursor != NULL) {
		pointer_cursor_apply_style(0U);
	}
}

static void create_pointer_cursor(lv_obj_t *scr)
{
	g_pointer_cursor = lv_obj_create(scr);
	lv_obj_set_size(g_pointer_cursor, POINTER_CURSOR_SIZE, POINTER_CURSOR_SIZE);
	lv_obj_set_pos(g_pointer_cursor, POINTER_CURSOR_MARGIN, POINTER_CURSOR_MARGIN);
	lv_obj_set_style_radius(g_pointer_cursor, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_border_width(g_pointer_cursor, 0, 0);
	lv_obj_set_style_shadow_width(g_pointer_cursor, 0, 0);
	lv_obj_set_style_pad_all(g_pointer_cursor, 0, 0);
	clear_static_flags(g_pointer_cursor);
	lv_obj_add_flag(g_pointer_cursor, LV_OBJ_FLAG_HIDDEN);
	pointer_cursor_apply_style(0U);
}

static void update_pointer_cursor(void)
{
	static int32_t last_cursor_x = -1;
	static int32_t last_cursor_y = -1;
	static lv_indev_state_t last_cursor_state = (lv_indev_state_t)-1;
	static uint8_t cursor_bound_to_pointer = 0U;
	window_event_s pointer_state = {0};
	lv_indev_state_t touch_state = LV_INDEV_STATE_RELEASED;
	window_s *window = lv_port_disp_get_window();

	if (g_pointer_cursor == NULL) {
		return;
	}
	if (window == NULL || !window_get_pointer_state(window, &pointer_state) ||
	    pointer_state.type != WINDOW_EVENT_POINTER) {
		if (!lv_obj_has_flag(g_pointer_cursor, LV_OBJ_FLAG_HIDDEN)) {
			lv_obj_add_flag(g_pointer_cursor, LV_OBJ_FLAG_HIDDEN);
		}
		return;
	}

	if (!cursor_bound_to_pointer && (pointer_state.x != 0 || pointer_state.y != 0)) {
		cursor_bound_to_pointer = 1U;
	}
	touch_state = pointer_state.value == WINDOW_POINTER_PRESS ?
		      LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;

	if (!cursor_bound_to_pointer) {
		if (!lv_obj_has_flag(g_pointer_cursor, LV_OBJ_FLAG_HIDDEN)) {
			lv_obj_add_flag(g_pointer_cursor, LV_OBJ_FLAG_HIDDEN);
		}
		return;
	}

	if (lv_obj_has_flag(g_pointer_cursor, LV_OBJ_FLAG_HIDDEN)) {
		lv_obj_clear_flag(g_pointer_cursor, LV_OBJ_FLAG_HIDDEN);
	}

	if (touch_state != last_cursor_state) {
		if (touch_state == LV_INDEV_STATE_PRESSED) {
			pointer_cursor_apply_style(1U);
		} else {
			pointer_cursor_apply_style(0U);
		}
	}

	if (pointer_state.x != last_cursor_x || pointer_state.y != last_cursor_y) {
		(void)window_set_position(window,
					  pointer_state.x - (POINTER_WINDOW_SIZE / 2),
					  pointer_state.y - (POINTER_WINDOW_SIZE / 2));
		last_cursor_x = pointer_state.x;
		last_cursor_y = pointer_state.y;
	}

	last_cursor_state = touch_state;
}

static void debug_ui_update_idle_lock(uint64_t mono_ms)
{
	uint64_t input_revision = window_get_input_revision();
	uint64_t last_input_ms = window_get_last_input_mono_ms();
	uint64_t idle_timeout_ms = (uint64_t)g_state.idle_lock_timeout_sec * 1000ULL;

	if (input_revision != g_last_input_revision || last_input_ms != g_last_input_ms) {
		g_last_input_revision = input_revision;
		g_last_input_ms = last_input_ms;
		g_idle_lock_requested = 0U;
	}

	if (g_idle_lock_requested) {
		return;
	}
	if (mono_ms < g_last_input_ms + idle_timeout_ms) {
		return;
	}
	if (mono_ms < g_last_lock_request_ms + SYSTEMUI_IDLE_LOCK_RETRY_MS) {
		return;
	}

	g_last_lock_request_ms = mono_ms;
	if (systemui_request_lockscreen_action(SYSTEMUI_OVERLAY_ACTION_OPEN) != 0U) {
		g_idle_lock_requested = 1U;
	}
}

static void debug_ui_on_create(app_s *app)
{
	(void)app;
	systemui_runtime_init(&g_state);
	(void)systemui_sync_appearance(&g_state, 0U, 1U);
	g_root = lv_scr_act();
	lv_obj_set_style_bg_color(g_root, lv_color_hex(0x000000), 0);
	lv_obj_set_style_bg_opa(g_root, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(g_root, 0, 0);
	lv_obj_set_style_pad_all(g_root, 0, 0);
	clear_static_flags(g_root);
	create_pointer_cursor(g_root);
	apply_appearance();
	g_last_input_revision = window_get_input_revision();
	g_last_input_ms = window_get_last_input_mono_ms();
	g_last_lock_request_ms = 0U;
	g_idle_lock_requested = 0U;
	lv_obj_invalidate(lv_scr_act());
	lv_refr_now(NULL);
	lv_port_disp_submit();
}

static void debug_ui_on_update(app_s *app, uint64_t mono_ms)
{
	(void)app;
	if (systemui_sync_appearance(&g_state, mono_ms, 0U) || g_state.appearance_dirty) {
		g_state.appearance_dirty = 0U;
		apply_appearance();
	}
	debug_ui_update_idle_lock(mono_ms);
	update_pointer_cursor();
}

int main(int argc, char *argv[])
{
	app_s app = {0};
	app_config_s config = {
		.name = "debugui",
		.x = (SYSTEMUI_WIDTH / 2) - (POINTER_WINDOW_SIZE / 2),
		.y = (SYSTEMUI_HEIGHT / 2) - (POINTER_WINDOW_SIZE / 2),
		.z = SYSTEMUI_DEBUG_WINDOW_Z,
		.width = POINTER_WINDOW_SIZE,
		.height = POINTER_WINDOW_SIZE,
		.window_flags = WINDOW_FLAG_ALPHA_BLEND,
		.enable_input = 0U,
	};
	app_lifecycle_ops_s lifecycle = {
		.on_create = debug_ui_on_create,
		.on_update = debug_ui_on_update,
	};

	(void)argc;
	(void)argv;
	log_info("debugui start\n");
	app_init(&app, &config, &lifecycle, NULL);
	return app_run(&app);
}
