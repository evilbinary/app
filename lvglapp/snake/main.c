#include "lvgl.h"
#include "app.h"
#include "libwindow/window.h"
#include "log.h"
#include "stdio.h"
#include "string.h"
#include "stdint.h"

#define SYSTEM_OVERLAY_GAP 24
#define CONTENT_X 40
#define CONTENT_Y SYSTEM_OVERLAY_GAP
#define CONTENT_W 944
#define CONTENT_H (APP_DEFAULT_HEIGHT - CONTENT_Y - SYSTEM_OVERLAY_GAP)

#define BOARD_COLS 18
#define BOARD_ROWS 18
#define BOARD_CELL_SIZE 23
#define BOARD_GAP 2
#define BOARD_CAPACITY (BOARD_COLS * BOARD_ROWS)
#define BOARD_PIXEL_SIZE ((BOARD_COLS * BOARD_CELL_SIZE) + ((BOARD_COLS - 1) * BOARD_GAP))

#define BOARD_X CONTENT_X
#define BOARD_Y (CONTENT_Y + 140)
#define BOARD_W 580
#define BOARD_H 520
#define SIDE_X (BOARD_X + BOARD_W + 20)
#define SIDE_Y BOARD_Y
#define SIDE_W (CONTENT_X + CONTENT_W - SIDE_X)
#define SIDE_H BOARD_H
#define FOOTER_Y (BOARD_Y + BOARD_H + 20)
#define FOOTER_H 132

#define COLOR_BG          0xf4f8fc
#define COLOR_BG_ALT      0xe6eff8
#define COLOR_PANEL       0xffffff
#define COLOR_PANEL_ALT   0xf1f6fb
#define COLOR_LINE        0xd2e0eb
#define COLOR_TEXT        0x163149
#define COLOR_DIM         0x6e8195
#define COLOR_ACCENT      0x4fa391
#define COLOR_ACCENT_ALT  0xc2a56d
#define COLOR_SNAKE_HEAD  0x3dbd67
#define COLOR_SNAKE_BODY  0x7cda90
#define COLOR_FOOD        0xef7a61
#define COLOR_EMPTY       0xe8eff5
#define COLOR_GAME_OVER   0xdf7465
#define COLOR_READY       0x43a46d

#define SNAKE_STEP_MS 180ULL
#define SNAKE_MIN_STEP_MS 92ULL
#define SNAKE_SPEEDUP_MS 6ULL

typedef struct snake_state {
	lv_obj_t *cells[BOARD_ROWS][BOARD_COLS];
	lv_obj_t *score_value;
	lv_obj_t *best_value;
	lv_obj_t *speed_value;
	lv_obj_t *status_value;
	lv_obj_t *hint_value;
	lv_obj_t *key_anchor;
	int16_t body_x[BOARD_CAPACITY];
	int16_t body_y[BOARD_CAPACITY];
	uint16_t body_len;
	int8_t dir_x;
	int8_t dir_y;
	int8_t next_dir_x;
	int8_t next_dir_y;
	int16_t food_x;
	int16_t food_y;
	uint16_t score;
	uint16_t best_score;
	uint32_t rng;
	uint64_t next_step_ms;
	uint8_t alive;
	uint8_t dirty;
} snake_state_s;

enum {
	SNAKE_CMD_UP = 1,
	SNAKE_CMD_DOWN,
	SNAKE_CMD_LEFT,
	SNAKE_CMD_RIGHT,
	SNAKE_CMD_RESTART,
};

static snake_state_s g_snake = {0};

static void clear_static_flags(lv_obj_t *obj)
{
	if (obj == NULL) {
		return;
	}
	lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CLICK_FOCUSABLE);
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
	lv_obj_set_style_shadow_width(panel, 0, 0);
	clear_static_flags(panel);
	return panel;
}

static lv_obj_t *create_info_card(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
				  lv_coord_t w, lv_coord_t h, const char *title)
{
	lv_obj_t *card = create_panel(parent, x, y, w, h, COLOR_PANEL_ALT, COLOR_LINE);
	lv_obj_t *title_lbl = lv_label_create(card);

	lv_obj_set_pos(title_lbl, 18, 16);
	lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(title_lbl, lv_color_hex(COLOR_DIM), 0);
	lv_label_set_text(title_lbl, title);

	return lv_label_create(card);
}

static void set_label_text(lv_obj_t *obj, const char *text, uint32_t color)
{
	if (obj == NULL || text == NULL) {
		return;
	}
	lv_label_set_text(obj, text);
	lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
}

static uint32_t snake_next_random(void)
{
	g_snake.rng = (g_snake.rng * 1664525U) + 1013904223U;
	return g_snake.rng;
}

static uint8_t snake_is_body_cell(int16_t x, int16_t y, uint16_t limit)
{
	for (uint16_t i = 0; i < limit; i++) {
		if (g_snake.body_x[i] == x && g_snake.body_y[i] == y) {
			return 1;
		}
	}
	return 0;
}

static void snake_place_food(void)
{
	uint16_t free_count = 0;
	uint32_t pick = 0;
	uint16_t seen = 0;

	free_count = (uint16_t)(BOARD_CAPACITY - g_snake.body_len);
	if (free_count == 0U) {
		g_snake.food_x = -1;
		g_snake.food_y = -1;
		return;
	}

	pick = snake_next_random() % free_count;
	for (int16_t y = 0; y < BOARD_ROWS; y++) {
		for (int16_t x = 0; x < BOARD_COLS; x++) {
			if (snake_is_body_cell(x, y, g_snake.body_len)) {
				continue;
			}
			if (seen == (uint16_t)pick) {
				g_snake.food_x = x;
				g_snake.food_y = y;
				return;
			}
			seen++;
		}
	}
}

static uint64_t snake_step_interval_ms(void)
{
	uint64_t speedup = (uint64_t)g_snake.score * SNAKE_SPEEDUP_MS;
	if (speedup >= (SNAKE_STEP_MS - SNAKE_MIN_STEP_MS)) {
		return SNAKE_MIN_STEP_MS;
	}
	return SNAKE_STEP_MS - speedup;
}

static void snake_refresh_hud(void)
{
	char text[96] = {0};
	uint64_t step_ms = snake_step_interval_ms();

	snprintf(text, sizeof(text), "%u", (unsigned int)g_snake.score);
	set_label_text(g_snake.score_value, text, COLOR_TEXT);

	snprintf(text, sizeof(text), "%u", (unsigned int)g_snake.best_score);
	set_label_text(g_snake.best_value, text, COLOR_ACCENT);

	snprintf(text, sizeof(text), "%llums step", (unsigned long long)step_ms);
	set_label_text(g_snake.speed_value, text, COLOR_ACCENT_ALT);

	if (g_snake.alive) {
		set_label_text(g_snake.status_value, "Run is live. Use arrows or touch controls to steer.", COLOR_READY);
	} else {
		set_label_text(g_snake.status_value, "Run ended. Tap Restart to begin a fresh round.", COLOR_GAME_OVER);
	}

	snprintf(text, sizeof(text),
		 "Food at %d:%d\nLength %u / %u",
		 (int)g_snake.food_x,
		 (int)g_snake.food_y,
		 (unsigned int)g_snake.body_len,
		 (unsigned int)BOARD_CAPACITY);
	set_label_text(g_snake.hint_value, text, COLOR_DIM);
}

static void snake_render_board(void)
{
	for (int16_t y = 0; y < BOARD_ROWS; y++) {
		for (int16_t x = 0; x < BOARD_COLS; x++) {
			uint32_t color = COLOR_EMPTY;
			uint32_t border = COLOR_LINE;
			uint8_t opa = LV_OPA_80;

			if (x == g_snake.food_x && y == g_snake.food_y) {
				color = COLOR_FOOD;
				border = COLOR_FOOD;
				opa = LV_OPA_COVER;
			}

			for (uint16_t i = 0; i < g_snake.body_len; i++) {
				if (g_snake.body_x[i] == x && g_snake.body_y[i] == y) {
					color = (i == 0U) ? COLOR_SNAKE_HEAD : COLOR_SNAKE_BODY;
					border = (i == 0U) ? COLOR_ACCENT : COLOR_SNAKE_BODY;
					opa = LV_OPA_COVER;
					break;
				}
			}

			lv_obj_set_style_bg_color(g_snake.cells[y][x], lv_color_hex(color), 0);
			lv_obj_set_style_bg_opa(g_snake.cells[y][x], opa, 0);
			lv_obj_set_style_border_color(g_snake.cells[y][x], lv_color_hex(border), 0);
		}
	}

	snake_refresh_hud();
	g_snake.dirty = 0;
}

static void snake_restart(uint64_t mono_ms)
{
	int16_t start_x = BOARD_COLS / 2;
	int16_t start_y = BOARD_ROWS / 2;

	memset(g_snake.body_x, 0, sizeof(g_snake.body_x));
	memset(g_snake.body_y, 0, sizeof(g_snake.body_y));
	g_snake.body_len = 4;
	for (uint16_t i = 0; i < g_snake.body_len; i++) {
		g_snake.body_x[i] = (int16_t)(start_x - i);
		g_snake.body_y[i] = start_y;
	}

	g_snake.dir_x = 1;
	g_snake.dir_y = 0;
	g_snake.next_dir_x = 1;
	g_snake.next_dir_y = 0;
	g_snake.score = 0;
	g_snake.alive = 1;
	g_snake.next_step_ms = mono_ms + snake_step_interval_ms();
	snake_place_food();
	g_snake.dirty = 1;
	log_info("snake: new game started\n");
}

static void snake_queue_direction(int8_t dx, int8_t dy)
{
	if ((dx == 0 && dy == 0) ||
	    (dx == -g_snake.dir_x && dy == -g_snake.dir_y) ||
	    (dx == -g_snake.next_dir_x && dy == -g_snake.next_dir_y)) {
		return;
	}

	g_snake.next_dir_x = dx;
	g_snake.next_dir_y = dy;
}

static void snake_apply_command(uintptr_t command)
{
	switch (command) {
		case SNAKE_CMD_UP:
			snake_queue_direction(0, -1);
			break;
		case SNAKE_CMD_DOWN:
			snake_queue_direction(0, 1);
			break;
		case SNAKE_CMD_LEFT:
			snake_queue_direction(-1, 0);
			break;
		case SNAKE_CMD_RIGHT:
			snake_queue_direction(1, 0);
			break;
		default:
			break;
	}
}

static void snake_control_event_cb(lv_event_t *e)
{
	uintptr_t command = 0;
	lv_event_code_t code = lv_event_get_code(e);

	if (code != LV_EVENT_PRESSED &&
	    code != LV_EVENT_CLICKED &&
	    code != LV_EVENT_LONG_PRESSED_REPEAT) {
		return;
	}

	command = (uintptr_t)lv_event_get_user_data(e);
	if (command == SNAKE_CMD_RESTART) {
		snake_restart(OSSysCtrlGetMonoTime() / NSEC_PER_MSEC);
	} else {
		snake_apply_command(command);
	}

	if (g_snake.key_anchor != NULL) {
		lv_group_focus_obj(g_snake.key_anchor);
	}
}

static void snake_key_event_cb(lv_event_t *e)
{
	uint32_t key = 0;

	if (lv_event_get_code(e) != LV_EVENT_KEY) {
		return;
	}

	key = lv_indev_get_key(lv_indev_get_act());
	switch (key) {
		case LV_KEY_UP:
			snake_apply_command(SNAKE_CMD_UP);
			break;
		case LV_KEY_DOWN:
			snake_apply_command(SNAKE_CMD_DOWN);
			break;
		case LV_KEY_LEFT:
			snake_apply_command(SNAKE_CMD_LEFT);
			break;
		case LV_KEY_RIGHT:
			snake_apply_command(SNAKE_CMD_RIGHT);
			break;
		case 'r':
		case 'R':
			snake_restart(OSSysCtrlGetMonoTime() / NSEC_PER_MSEC);
			break;
		default:
			break;
	}
}

static lv_obj_t *create_button(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
			       const char *text, uint32_t bg, uintptr_t command)
{
	lv_obj_t *btn = lv_btn_create(parent);
	lv_obj_t *label = lv_label_create(btn);

	lv_obj_set_pos(btn, x, y);
	lv_obj_set_size(btn, w, h);
	lv_obj_set_style_radius(btn, 24, 0);
	lv_obj_set_style_bg_color(btn, lv_color_hex(bg), 0);
	lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
	lv_obj_set_style_border_width(btn, 0, 0);
	lv_obj_set_style_shadow_width(btn, 0, 0);
	lv_obj_set_style_outline_width(btn, 0, LV_STATE_FOCUSED);
	lv_obj_set_style_text_color(btn, lv_color_hex(COLOR_TEXT), 0);
	lv_obj_set_style_text_font(btn, &lv_font_montserrat_24, 0);
	lv_obj_add_event_cb(btn, snake_control_event_cb, LV_EVENT_PRESSED, (void *)command);
	lv_obj_add_event_cb(btn, snake_control_event_cb, LV_EVENT_CLICKED, (void *)command);
	lv_obj_add_event_cb(btn, snake_control_event_cb, LV_EVENT_LONG_PRESSED_REPEAT, (void *)command);

	lv_label_set_text(label, text);
	lv_obj_center(label);
	return btn;
}

static void snake_step(void)
{
	int16_t new_x = 0;
	int16_t new_y = 0;
	uint8_t grow = 0;
	uint16_t collision_limit = 0;

	if (!g_snake.alive) {
		return;
	}

	g_snake.dir_x = g_snake.next_dir_x;
	g_snake.dir_y = g_snake.next_dir_y;
	new_x = (int16_t)(g_snake.body_x[0] + g_snake.dir_x);
	new_y = (int16_t)(g_snake.body_y[0] + g_snake.dir_y);

	if (new_x < 0 || new_x >= BOARD_COLS || new_y < 0 || new_y >= BOARD_ROWS) {
		g_snake.alive = 0;
		g_snake.dirty = 1;
		return;
	}

	grow = (uint8_t)(new_x == g_snake.food_x && new_y == g_snake.food_y);
	collision_limit = grow ? g_snake.body_len : (uint16_t)(g_snake.body_len - 1U);
	if (snake_is_body_cell(new_x, new_y, collision_limit)) {
		g_snake.alive = 0;
		g_snake.dirty = 1;
		return;
	}

	if (grow && g_snake.body_len < BOARD_CAPACITY) {
		g_snake.body_len++;
		g_snake.score++;
		if (g_snake.score > g_snake.best_score) {
			g_snake.best_score = g_snake.score;
		}
	}

	for (uint16_t i = g_snake.body_len - 1U; i > 0U; i--) {
		g_snake.body_x[i] = g_snake.body_x[i - 1U];
		g_snake.body_y[i] = g_snake.body_y[i - 1U];
	}
	g_snake.body_x[0] = new_x;
	g_snake.body_y[0] = new_y;

	if (grow) {
		if (g_snake.body_len == BOARD_CAPACITY) {
			g_snake.alive = 0;
		}
		snake_place_food();
	}

	g_snake.dirty = 1;
}

static void create_ui(void)
{
	lv_obj_t *scr = lv_scr_act();
	lv_obj_t *glow_left = NULL;
	lv_obj_t *glow_right = NULL;
	lv_obj_t *header = NULL;
	lv_obj_t *board_panel = NULL;
	lv_obj_t *board_surface = NULL;
	lv_obj_t *side_panel = NULL;
	lv_obj_t *footer_panel = NULL;
	lv_obj_t *title = NULL;
	lv_obj_t *subtitle = NULL;
	lv_obj_t *section = NULL;
	lv_coord_t board_origin_x = 0;
	lv_coord_t board_origin_y = 0;

	lv_obj_set_style_bg_color(scr, lv_color_hex(COLOR_BG), 0);
	lv_obj_set_style_bg_grad_color(scr, lv_color_hex(COLOR_BG_ALT), 0);
	lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, 0);
	lv_obj_set_style_border_width(scr, 0, 0);
	clear_static_flags(scr);

	glow_left = lv_obj_create(scr);
	lv_obj_set_size(glow_left, 260, 260);
	lv_obj_set_pos(glow_left, -70, 110);
	lv_obj_set_style_radius(glow_left, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_bg_color(glow_left, lv_color_hex(COLOR_ACCENT), 0);
	lv_obj_set_style_bg_opa(glow_left, LV_OPA_20, 0);
	lv_obj_set_style_border_width(glow_left, 0, 0);
	clear_static_flags(glow_left);

	glow_right = lv_obj_create(scr);
	lv_obj_set_size(glow_right, 300, 300);
	lv_obj_set_pos(glow_right, 780, 160);
	lv_obj_set_style_radius(glow_right, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_bg_color(glow_right, lv_color_hex(COLOR_ACCENT_ALT), 0);
	lv_obj_set_style_bg_opa(glow_right, LV_OPA_20, 0);
	lv_obj_set_style_border_width(glow_right, 0, 0);
	clear_static_flags(glow_right);

	header = create_panel(scr, CONTENT_X, CONTENT_Y, CONTENT_W, 120, COLOR_PANEL_ALT, COLOR_LINE);

	section = lv_label_create(header);
	lv_obj_set_pos(section, 24, 18);
	lv_obj_set_style_text_font(section, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(section, lv_color_hex(COLOR_ACCENT), 0);
	lv_label_set_text(section, "BREAK MODE");

	title = lv_label_create(header);
	lv_obj_set_pos(title, 24, 42);
	lv_obj_set_style_text_font(title, &lv_font_montserrat_48, 0);
	lv_obj_set_style_text_color(title, lv_color_hex(COLOR_TEXT), 0);
	lv_label_set_text(title, "Snake");

	subtitle = lv_label_create(header);
	lv_obj_set_pos(subtitle, 312, 46);
	lv_obj_set_width(subtitle, 604);
	lv_label_set_long_mode(subtitle, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(subtitle, lv_color_hex(COLOR_DIM), 0);
	lv_label_set_text(subtitle,
			  "Take a short break. Use touch or arrow keys to steer, then restart anytime from the control bar.");

	board_panel = create_panel(scr, BOARD_X, BOARD_Y, BOARD_W, BOARD_H, COLOR_PANEL, COLOR_LINE);
	board_surface = create_panel(board_panel, 18, 18, BOARD_W - 36, BOARD_H - 36, COLOR_BG_ALT, COLOR_LINE);

	board_origin_x = (lv_coord_t)((BOARD_W - 36 - BOARD_PIXEL_SIZE) / 2);
	board_origin_y = (lv_coord_t)((BOARD_H - 36 - BOARD_PIXEL_SIZE) / 2);
	for (int16_t y = 0; y < BOARD_ROWS; y++) {
		for (int16_t x = 0; x < BOARD_COLS; x++) {
			lv_obj_t *cell = lv_obj_create(board_surface);

			lv_obj_set_pos(cell,
				       (lv_coord_t)(board_origin_x + (x * (BOARD_CELL_SIZE + BOARD_GAP))),
				       (lv_coord_t)(board_origin_y + (y * (BOARD_CELL_SIZE + BOARD_GAP))));
			lv_obj_set_size(cell, BOARD_CELL_SIZE, BOARD_CELL_SIZE);
			lv_obj_set_style_radius(cell, 10, 0);
			lv_obj_set_style_bg_color(cell, lv_color_hex(COLOR_EMPTY), 0);
			lv_obj_set_style_bg_opa(cell, LV_OPA_80, 0);
			lv_obj_set_style_border_color(cell, lv_color_hex(COLOR_LINE), 0);
			lv_obj_set_style_border_width(cell, 1, 0);
			lv_obj_set_style_shadow_width(cell, 0, 0);
			clear_static_flags(cell);
			g_snake.cells[y][x] = cell;
		}
	}

	side_panel = create_panel(scr, SIDE_X, SIDE_Y, SIDE_W, SIDE_H, COLOR_PANEL, COLOR_LINE);

	g_snake.score_value = create_info_card(side_panel, 18, 18, SIDE_W - 36, 96, "Score");
	lv_obj_set_pos(g_snake.score_value, 18, 28);
	lv_obj_set_style_text_font(g_snake.score_value, &lv_font_montserrat_48, 0);
	lv_obj_set_width(g_snake.score_value, SIDE_W - 72);

	g_snake.best_value = create_info_card(side_panel, 18, 126, SIDE_W - 36, 76, "Best");
	lv_obj_set_pos(g_snake.best_value, 18, 28);
	lv_obj_set_style_text_font(g_snake.best_value, &lv_font_montserrat_32, 0);
	lv_obj_set_width(g_snake.best_value, SIDE_W - 72);

	g_snake.speed_value = create_info_card(side_panel, 18, 214, SIDE_W - 36, 76, "Speed");
	lv_obj_set_pos(g_snake.speed_value, 18, 28);
	lv_obj_set_style_text_font(g_snake.speed_value, &lv_font_montserrat_24, 0);
	lv_obj_set_width(g_snake.speed_value, SIDE_W - 72);

	g_snake.status_value = create_info_card(side_panel, 18, 302, SIDE_W - 36, 96, "Status");
	lv_obj_set_pos(g_snake.status_value, 18, 30);
	lv_obj_set_width(g_snake.status_value, SIDE_W - 72);
	lv_label_set_long_mode(g_snake.status_value, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(g_snake.status_value, &lv_font_montserrat_16, 0);

	g_snake.hint_value = create_info_card(side_panel, 18, 410, SIDE_W - 36, 92, "Run data");
	lv_obj_set_pos(g_snake.hint_value, 18, 28);
	lv_obj_set_width(g_snake.hint_value, SIDE_W - 72);
	lv_label_set_long_mode(g_snake.hint_value, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(g_snake.hint_value, &lv_font_montserrat_16, 0);

	footer_panel = create_panel(scr, CONTENT_X, FOOTER_Y, CONTENT_W, FOOTER_H, COLOR_PANEL_ALT, COLOR_LINE);

	section = lv_label_create(footer_panel);
	lv_obj_set_pos(section, 24, 18);
	lv_obj_set_style_text_font(section, &lv_font_montserrat_24, 0);
	lv_obj_set_style_text_color(section, lv_color_hex(COLOR_TEXT), 0);
	lv_label_set_text(section, "Controls");

	subtitle = lv_label_create(footer_panel);
	lv_obj_set_pos(subtitle, 24, 46);
	lv_obj_set_width(subtitle, 446);
	lv_label_set_long_mode(subtitle, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(subtitle, lv_color_hex(COLOR_DIM), 0);
	lv_label_set_text(subtitle,
			  "Tap a direction to steer. Arrow keys also work. Press R or Restart to begin a fresh round.");

	create_button(footer_panel, 586, 16, 88, 42, "Up", COLOR_ACCENT, SNAKE_CMD_UP);
	create_button(footer_panel, 492, 70, 88, 42, "Left", COLOR_ACCENT_ALT, SNAKE_CMD_LEFT);
	create_button(footer_panel, 586, 70, 88, 42, "Down", COLOR_ACCENT, SNAKE_CMD_DOWN);
	create_button(footer_panel, 680, 70, 88, 42, "Right", COLOR_ACCENT_ALT, SNAKE_CMD_RIGHT);
	create_button(footer_panel, 796, 34, 118, 56, "Restart", COLOR_GAME_OVER, SNAKE_CMD_RESTART);

	g_snake.key_anchor = lv_btn_create(scr);
	lv_obj_set_pos(g_snake.key_anchor, 0, 0);
	lv_obj_set_size(g_snake.key_anchor, 1, 1);
	lv_obj_set_style_bg_opa(g_snake.key_anchor, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(g_snake.key_anchor, 0, 0);
	lv_obj_set_style_shadow_width(g_snake.key_anchor, 0, 0);
	lv_obj_set_style_outline_width(g_snake.key_anchor, 0, 0);
	lv_obj_add_event_cb(g_snake.key_anchor, snake_key_event_cb, LV_EVENT_KEY, NULL);
	lv_obj_add_flag(g_snake.key_anchor, LV_OBJ_FLAG_CLICK_FOCUSABLE);
	lv_group_focus_obj(g_snake.key_anchor);
}

static void snake_on_create(app_s *app)
{
	uint64_t mono_ms = 0;

	(void)app;
	memset(&g_snake, 0, sizeof(g_snake));
	g_snake.rng = (uint32_t)(OSSysCtrlGetMonoTime() & 0xffffffffU);
	create_ui();
	mono_ms = OSSysCtrlGetMonoTime() / NSEC_PER_MSEC;
	snake_restart(mono_ms);
	snake_render_board();
	log_info("snake ready\n");
}

static void snake_on_foreground(app_s *app)
{
	(void)app;
	if (g_snake.alive) {
		set_label_text(g_snake.status_value, "Run is live. Use arrows or touch controls to steer.", COLOR_READY);
	}
	if (g_snake.key_anchor != NULL) {
		lv_group_focus_obj(g_snake.key_anchor);
	}
}

static void snake_on_background(app_s *app)
{
	(void)app;
	if (g_snake.alive) {
		set_label_text(g_snake.status_value, "Paused in the background. Return to keep the run going.", COLOR_DIM);
	}
}

static void snake_on_update(app_s *app, uint64_t mono_ms)
{
	(void)app;

	if (g_snake.alive && mono_ms >= g_snake.next_step_ms) {
		uint32_t steps = 0;
		do {
			snake_step();
			steps++;
			g_snake.next_step_ms += snake_step_interval_ms();
		} while (g_snake.alive && mono_ms >= g_snake.next_step_ms && steps < 4U);
	}

	if (g_snake.dirty) {
		snake_render_board();
	}
}

int main(int argc, char *argv[])
{
	app_s app = {0};
	app_config_s config = {
		.name = "snake",
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

	log_info("snake start!\n");

	lifecycle.on_create = snake_on_create;
	lifecycle.on_foreground = snake_on_foreground;
	lifecycle.on_background = snake_on_background;
	lifecycle.on_update = snake_on_update;
	app_init(&app, &config, &lifecycle, NULL);
	return app_run(&app);
}
