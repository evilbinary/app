#include "lvgl.h"
#include "app.h"
#include "libwindow/window.h"
#include "log.h"
#include "stdio.h"
#include "string.h"
#include "stdint.h"
#include "stddef.h"

#define CONTENT_MARGIN 40
#define CONTENT_TOP_GAP 28
#define CONTENT_BOTTOM_GAP 28
#define CONTENT_X CONTENT_MARGIN
#define CONTENT_Y CONTENT_TOP_GAP
#define CONTENT_W (APP_DEFAULT_WIDTH - (CONTENT_MARGIN * 2))
#define CONTENT_H (APP_DEFAULT_HEIGHT - CONTENT_TOP_GAP - CONTENT_BOTTOM_GAP)

#define HUD_X CONTENT_X
#define HUD_Y CONTENT_Y
#define HUD_W CONTENT_W
#define HUD_H 96
#define PLAYFIELD_X CONTENT_X
#define PLAYFIELD_Y (HUD_Y + HUD_H + 18)
#define PLAYFIELD_W CONTENT_W
#define PLAYFIELD_H (CONTENT_H - HUD_H - 18)
#define PLAYFIELD_PAD 24
#define PLAYFIELD_INNER_X PLAYFIELD_PAD
#define PLAYFIELD_INNER_Y 76
#define PLAYFIELD_INNER_BOTTOM_PAD 56
#define PLAYFIELD_INNER_W (PLAYFIELD_W - (PLAYFIELD_PAD * 2))
#define PLAYFIELD_INNER_H (PLAYFIELD_H - PLAYFIELD_INNER_Y - PLAYFIELD_INNER_BOTTOM_PAD)

#define BALL_SIZE 54
#define BALL_SHADOW_OFFSET_X 10
#define BALL_SHADOW_OFFSET_Y 12
#define BALL_FP_SHIFT 10
#define BALL_FP_ONE (1 << BALL_FP_SHIFT)
#define BOUNCE_INITIAL_VX 372
#define BOUNCE_INITIAL_VY 284
#define BOUNCE_MAX_DT_MS 40U

#define COLOR_BG 0xf2f7fb
#define COLOR_BG_ALT 0xe3edf8
#define COLOR_PANEL 0xffffff
#define COLOR_PANEL_ALT 0xf4f8fc
#define COLOR_FIELD 0x12263a
#define COLOR_FIELD_ALT 0x18344b
#define COLOR_TEXT 0x163149
#define COLOR_DIM 0x708297
#define COLOR_LINE 0xd3e0eb
#define COLOR_ACCENT 0x4fa391
#define COLOR_ACCENT_ALT 0xf3b562
#define COLOR_TRACK 0x29465f
#define COLOR_SHADOW 0x0f1f30

typedef struct bounce_state {
	lv_obj_t *root;
	lv_obj_t *playfield;
	lv_obj_t *trail;
	lv_obj_t *shadow;
	lv_obj_t *ball;
	lv_obj_t *fps_label;
	lv_obj_t *speed_label;
	lv_obj_t *bounce_label;
	lv_obj_t *hint_label;
	int32_t pos_x_fp;
	int32_t pos_y_fp;
	int32_t vel_x_fp;
	int32_t vel_y_fp;
	uint32_t fps_x10;
	uint32_t rebound_count;
	uint32_t fps_window_frames;
	uint64_t last_tick_ms;
	uint64_t last_fps_sample_ms;
	uint8_t dirty;
} bounce_state_s;

static bounce_state_s g_bounce = {0};

static void clear_static_flags(lv_obj_t *obj)
{
	if (obj == NULL) {
		return;
	}

	lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CLICK_FOCUSABLE);
}

static lv_obj_t *create_panel(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
			      lv_coord_t w, lv_coord_t h, uint32_t bg, uint32_t border)
{
	lv_obj_t *panel = lv_obj_create(parent);

	lv_obj_set_pos(panel, x, y);
	lv_obj_set_size(panel, w, h);
	lv_obj_set_style_bg_color(panel, lv_color_hex(bg), 0);
	lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
	lv_obj_set_style_border_color(panel, lv_color_hex(border), 0);
	lv_obj_set_style_border_width(panel, 0, 0);
	lv_obj_set_style_radius(panel, 28, 0);
	lv_obj_set_style_pad_all(panel, 0, 0);
	lv_obj_set_style_shadow_width(panel, 0, 0);
	clear_static_flags(panel);
	return panel;
}

static void set_label_text(lv_obj_t *obj, const char *text, uint32_t color)
{
	if (obj == NULL || text == NULL) {
		return;
	}

	lv_label_set_text(obj, text);
	lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
}

static uint32_t bounce_ball_left_px(void)
{
	return (uint32_t)(g_bounce.pos_x_fp >> BALL_FP_SHIFT);
}

static uint32_t bounce_ball_top_px(void)
{
	return (uint32_t)(g_bounce.pos_y_fp >> BALL_FP_SHIFT);
}

static uint32_t bounce_speed_pps(void)
{
	uint32_t vx = (uint32_t)(g_bounce.vel_x_fp < 0 ? -g_bounce.vel_x_fp : g_bounce.vel_x_fp);
	uint32_t vy = (uint32_t)(g_bounce.vel_y_fp < 0 ? -g_bounce.vel_y_fp : g_bounce.vel_y_fp);

	return (vx > vy ? vx : vy) >> BALL_FP_SHIFT;
}

static void bounce_refresh_hud(void)
{
	char text[64] = {0};

	if (g_bounce.fps_x10 == 0U) {
		snprintf(text, sizeof(text), "FPS --");
	} else {
		snprintf(text, sizeof(text), "FPS %u.%u",
			 (unsigned int)(g_bounce.fps_x10 / 10U),
			 (unsigned int)(g_bounce.fps_x10 % 10U));
	}
	set_label_text(g_bounce.fps_label, text, COLOR_TEXT);

	snprintf(text, sizeof(text), "Speed %u px/s", (unsigned int)bounce_speed_pps());
	set_label_text(g_bounce.speed_label, text, COLOR_DIM);

	snprintf(text, sizeof(text), "Rebounds %u", (unsigned int)g_bounce.rebound_count);
	set_label_text(g_bounce.bounce_label, text, COLOR_ACCENT);
}

static void bounce_render_ball(void)
{
	lv_coord_t x = (lv_coord_t)(PLAYFIELD_INNER_X + bounce_ball_left_px());
	lv_coord_t y = (lv_coord_t)(PLAYFIELD_INNER_Y + bounce_ball_top_px());

	if (g_bounce.trail != NULL) {
		lv_obj_set_pos(g_bounce.trail, x - 8, y - 8);
	}
	if (g_bounce.shadow != NULL) {
		lv_obj_set_pos(g_bounce.shadow, x + BALL_SHADOW_OFFSET_X, y + BALL_SHADOW_OFFSET_Y);
	}
	if (g_bounce.ball != NULL) {
		lv_obj_set_pos(g_bounce.ball, x, y);
	}
}

static void bounce_reset_motion(uint64_t mono_ms)
{
	int32_t inner_w = (int32_t)PLAYFIELD_INNER_W - BALL_SIZE;
	int32_t inner_h = (int32_t)PLAYFIELD_INNER_H - BALL_SIZE;

	memset(&g_bounce.pos_x_fp, 0, sizeof(g_bounce) - offsetof(bounce_state_s, pos_x_fp));
	g_bounce.pos_x_fp = (inner_w / 5) * BALL_FP_ONE;
	g_bounce.pos_y_fp = (inner_h / 4) * BALL_FP_ONE;
	g_bounce.vel_x_fp = BOUNCE_INITIAL_VX * BALL_FP_ONE;
	g_bounce.vel_y_fp = BOUNCE_INITIAL_VY * BALL_FP_ONE;
	g_bounce.fps_x10 = 0U;
	g_bounce.last_tick_ms = mono_ms;
	g_bounce.last_fps_sample_ms = 0U;
	g_bounce.dirty = 1U;
}

static void bounce_step(uint32_t dt_ms)
{
	int32_t min_x_fp = 0;
	int32_t min_y_fp = 0;
	int32_t max_x_fp = 0;
	int32_t max_y_fp = 0;

	if (dt_ms == 0U) {
		return;
	}

	max_x_fp = ((int32_t)PLAYFIELD_INNER_W - BALL_SIZE) * BALL_FP_ONE;
	max_y_fp = ((int32_t)PLAYFIELD_INNER_H - BALL_SIZE) * BALL_FP_ONE;

	g_bounce.pos_x_fp += (int32_t)(((int64_t)g_bounce.vel_x_fp * dt_ms) / 1000LL);
	g_bounce.pos_y_fp += (int32_t)(((int64_t)g_bounce.vel_y_fp * dt_ms) / 1000LL);

	if (g_bounce.pos_x_fp <= min_x_fp) {
		g_bounce.pos_x_fp = min_x_fp;
		g_bounce.vel_x_fp = -g_bounce.vel_x_fp;
		g_bounce.rebound_count++;
	}
	if (g_bounce.pos_x_fp >= max_x_fp) {
		g_bounce.pos_x_fp = max_x_fp;
		g_bounce.vel_x_fp = -g_bounce.vel_x_fp;
		g_bounce.rebound_count++;
	}
	if (g_bounce.pos_y_fp <= min_y_fp) {
		g_bounce.pos_y_fp = min_y_fp;
		g_bounce.vel_y_fp = -g_bounce.vel_y_fp;
		g_bounce.rebound_count++;
	}
	if (g_bounce.pos_y_fp >= max_y_fp) {
		g_bounce.pos_y_fp = max_y_fp;
		g_bounce.vel_y_fp = -g_bounce.vel_y_fp;
		g_bounce.rebound_count++;
	}

	g_bounce.dirty = 1U;
}

static void bounce_create_ui(void)
{
	lv_obj_t *scr = lv_scr_act();
	lv_obj_t *hud = NULL;
	lv_obj_t *title = NULL;
	lv_obj_t *subtitle = NULL;
	lv_obj_t *field_title = NULL;
	lv_obj_t *field_line = NULL;

	g_bounce.root = scr;
	lv_obj_set_style_bg_color(scr, lv_color_hex(COLOR_BG), 0);
	lv_obj_set_style_bg_grad_color(scr, lv_color_hex(COLOR_BG_ALT), 0);
	lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, 0);
	lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

	hud = create_panel(scr, HUD_X, HUD_Y, HUD_W, HUD_H, COLOR_PANEL, COLOR_LINE);

	title = lv_label_create(hud);
	lv_obj_set_pos(title, 26, 18);
	lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
	lv_obj_set_style_text_color(title, lv_color_hex(COLOR_TEXT), 0);
	lv_label_set_text(title, "Bounce Lab");

	subtitle = lv_label_create(hud);
	lv_obj_set_pos(subtitle, 26, 56);
	lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(subtitle, lv_color_hex(COLOR_DIM), 0);
	lv_label_set_text(subtitle, "A single ball rides the frame pacing. Watch it rebound and track live FPS.");

	g_bounce.fps_label = lv_label_create(hud);
	lv_obj_set_pos(g_bounce.fps_label, HUD_W - 170, 20);
	lv_obj_set_style_text_font(g_bounce.fps_label, &lv_font_montserrat_24, 0);
	lv_obj_set_style_text_color(g_bounce.fps_label, lv_color_hex(COLOR_TEXT), 0);

	g_bounce.speed_label = lv_label_create(hud);
	lv_obj_set_pos(g_bounce.speed_label, HUD_W - 170, 52);
	lv_obj_set_style_text_font(g_bounce.speed_label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(g_bounce.speed_label, lv_color_hex(COLOR_DIM), 0);

	g_bounce.playfield = create_panel(scr, PLAYFIELD_X, PLAYFIELD_Y, PLAYFIELD_W, PLAYFIELD_H,
					  COLOR_FIELD, COLOR_TRACK);
	lv_obj_set_style_bg_grad_color(g_bounce.playfield, lv_color_hex(COLOR_FIELD_ALT), 0);
	lv_obj_set_style_bg_grad_dir(g_bounce.playfield, LV_GRAD_DIR_VER, 0);
	lv_obj_set_style_radius(g_bounce.playfield, 36, 0);

	field_title = lv_label_create(g_bounce.playfield);
	lv_obj_set_pos(field_title, 24, 18);
	lv_obj_set_style_text_font(field_title, &lv_font_montserrat_24, 0);
	lv_obj_set_style_text_color(field_title, lv_color_hex(0xf5f9fd), 0);
	lv_label_set_text(field_title, "Motion Field");

	g_bounce.bounce_label = lv_label_create(g_bounce.playfield);
	lv_obj_set_pos(g_bounce.bounce_label, PLAYFIELD_W - 180, 20);
	lv_obj_set_style_text_font(g_bounce.bounce_label, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(g_bounce.bounce_label, lv_color_hex(COLOR_ACCENT_ALT), 0);

	field_line = lv_obj_create(g_bounce.playfield);
	lv_obj_set_pos(field_line, 24, 54);
	lv_obj_set_size(field_line, PLAYFIELD_W - 48, 1);
	lv_obj_set_style_bg_color(field_line, lv_color_hex(COLOR_TRACK), 0);
	lv_obj_set_style_bg_opa(field_line, LV_OPA_COVER, 0);
	lv_obj_set_style_border_width(field_line, 0, 0);
	lv_obj_set_style_radius(field_line, 0, 0);
	clear_static_flags(field_line);

	g_bounce.hint_label = lv_label_create(g_bounce.playfield);
	lv_obj_set_pos(g_bounce.hint_label, 24, PLAYFIELD_H - 44);
	lv_obj_set_style_text_font(g_bounce.hint_label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(g_bounce.hint_label, lv_color_hex(0xa4bed4), 0);
	lv_label_set_text(g_bounce.hint_label, "Fixed at 60 FPS target. The readout shows the actual delivered frame rate.");

	g_bounce.trail = lv_obj_create(g_bounce.playfield);
	lv_obj_set_size(g_bounce.trail, BALL_SIZE + 16, BALL_SIZE + 16);
	lv_obj_set_style_radius(g_bounce.trail, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_bg_color(g_bounce.trail, lv_color_hex(COLOR_ACCENT_ALT), 0);
	lv_obj_set_style_bg_opa(g_bounce.trail, LV_OPA_20, 0);
	lv_obj_set_style_border_width(g_bounce.trail, 0, 0);
	lv_obj_set_style_shadow_width(g_bounce.trail, 0, 0);
	clear_static_flags(g_bounce.trail);

	g_bounce.shadow = lv_obj_create(g_bounce.playfield);
	lv_obj_set_size(g_bounce.shadow, BALL_SIZE, BALL_SIZE);
	lv_obj_set_style_radius(g_bounce.shadow, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_bg_color(g_bounce.shadow, lv_color_hex(COLOR_SHADOW), 0);
	lv_obj_set_style_bg_opa(g_bounce.shadow, LV_OPA_40, 0);
	lv_obj_set_style_border_width(g_bounce.shadow, 0, 0);
	lv_obj_set_style_shadow_width(g_bounce.shadow, 0, 0);
	clear_static_flags(g_bounce.shadow);

	g_bounce.ball = lv_obj_create(g_bounce.playfield);
	lv_obj_set_size(g_bounce.ball, BALL_SIZE, BALL_SIZE);
	lv_obj_set_style_radius(g_bounce.ball, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_bg_color(g_bounce.ball, lv_color_hex(COLOR_ACCENT), 0);
	lv_obj_set_style_bg_grad_color(g_bounce.ball, lv_color_hex(COLOR_ACCENT_ALT), 0);
	lv_obj_set_style_bg_grad_dir(g_bounce.ball, LV_GRAD_DIR_VER, 0);
	lv_obj_set_style_border_width(g_bounce.ball, 0, 0);
	lv_obj_set_style_shadow_width(g_bounce.ball, 26, 0);
	lv_obj_set_style_shadow_color(g_bounce.ball, lv_color_hex(COLOR_ACCENT), 0);
	lv_obj_set_style_shadow_opa(g_bounce.ball, LV_OPA_40, 0);
	clear_static_flags(g_bounce.ball);
}

static void bounce_on_create(app_s *app)
{
	uint64_t mono_ms = 0U;

	(void)app;
	memset(&g_bounce, 0, sizeof(g_bounce));
	bounce_create_ui();
	mono_ms = OSSysCtrlGetMonoTime() / NSEC_PER_MSEC;
	bounce_reset_motion(mono_ms);
	bounce_refresh_hud();
	bounce_render_ball();
	log_info("bounce ready\n");
}

static void bounce_on_update(app_s *app, uint64_t mono_ms)
{
	uint32_t dt_ms = 0U;
	uint64_t fps_elapsed_ms = 0U;

	(void)app;

	if (g_bounce.last_tick_ms == 0U) {
		g_bounce.last_tick_ms = mono_ms;
	}
	if (g_bounce.last_fps_sample_ms == 0U) {
		g_bounce.last_fps_sample_ms = mono_ms;
		g_bounce.fps_window_frames = 0U;
	}
	dt_ms = (uint32_t)(mono_ms - g_bounce.last_tick_ms);
	if (dt_ms > BOUNCE_MAX_DT_MS) {
		dt_ms = BOUNCE_MAX_DT_MS;
	}
	g_bounce.last_tick_ms = mono_ms;
	bounce_step(dt_ms);

	g_bounce.fps_window_frames++;
	fps_elapsed_ms = mono_ms - g_bounce.last_fps_sample_ms;
	if (fps_elapsed_ms >= 250U) {
		g_bounce.fps_x10 = (uint32_t)((g_bounce.fps_window_frames * 10000ULL) / fps_elapsed_ms);
		g_bounce.fps_window_frames = 0U;
		g_bounce.last_fps_sample_ms = mono_ms;
		g_bounce.dirty = 1U;
	}

	if (!g_bounce.dirty) {
		return;
	}

	bounce_refresh_hud();
	bounce_render_ball();
	g_bounce.dirty = 0U;
}

int main(int argc, char *argv[])
{
	app_s app = {0};
	app_config_s config = {
		.name = "bounce",
		.x = APP_DEFAULT_X,
		.y = APP_DEFAULT_Y,
		.z = 0,
		.width = APP_DEFAULT_WIDTH,
		.height = APP_DEFAULT_HEIGHT,
		.window_flags = WINDOW_FLAG_FOCUSABLE,
		.enable_input = 0,
	};
	app_lifecycle_ops_s lifecycle = {0};

	(void)argc;
	(void)argv;

	log_info("bounce start!\n");

	lifecycle.on_create = bounce_on_create;
	lifecycle.on_update = bounce_on_update;
	app_init(&app, &config, &lifecycle, NULL);
	return app_run(&app);
}
