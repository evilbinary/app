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

#define HEADER_H 136
#define BOARD_PANEL_X CONTENT_X
#define BOARD_PANEL_Y (CONTENT_Y + HEADER_H + 16)
#define BOARD_PANEL_W 612
#define BOARD_PANEL_H 644
#define SIDE_PANEL_X (BOARD_PANEL_X + BOARD_PANEL_W + 20)
#define SIDE_PANEL_Y BOARD_PANEL_Y
#define SIDE_PANEL_W (CONTENT_X + CONTENT_W - SIDE_PANEL_X)
#define SIDE_PANEL_H BOARD_PANEL_H

#define BOARD_COLS 10
#define BOARD_ROWS 10
#define BOARD_MINES 16
#define BOARD_CAPACITY (BOARD_COLS * BOARD_ROWS)
#define BOARD_SAFE_CELLS (BOARD_CAPACITY - BOARD_MINES)
#define BOARD_CELL_SIZE 48
#define BOARD_GAP 4
#define BOARD_PIXEL_SIZE ((BOARD_COLS * BOARD_CELL_SIZE) + ((BOARD_COLS - 1) * BOARD_GAP))

#define COLOR_BG            0xf6f8fc
#define COLOR_BG_ALT        0xe8eef7
#define COLOR_PANEL         0xffffff
#define COLOR_PANEL_ALT     0xf2f6fb
#define COLOR_LINE          0xd5e0ea
#define COLOR_TEXT          0x163149
#define COLOR_DIM           0x6f8193
#define COLOR_ACCENT        0x4d9888
#define COLOR_ACCENT_SOFT   0xe4f1ed
#define COLOR_WARM          0xc9a364
#define COLOR_WARM_SOFT     0xf5eddf
#define COLOR_ALERT         0xdd6d63
#define COLOR_ALERT_SOFT    0xf8e8e6
#define COLOR_CELL_COVER    0xe3ecf5
#define COLOR_CELL_OPEN     0xffffff
#define COLOR_READY         0x3e8e69

typedef struct minesweeper_cell {
	lv_obj_t *button;
	lv_obj_t *label;
	uint8_t mine;
	uint8_t revealed;
	uint8_t flagged;
	uint8_t adjacent;
} minesweeper_cell_s;

typedef struct minesweeper_state {
	minesweeper_cell_s cells[BOARD_ROWS][BOARD_COLS];
	lv_obj_t *mine_value;
	lv_obj_t *flag_value;
	lv_obj_t *clear_value;
	lv_obj_t *mode_value;
	lv_obj_t *status_value;
	lv_obj_t *hint_value;
	lv_obj_t *mode_btn;
	lv_obj_t *mode_btn_label;
	lv_obj_t *key_anchor;
	uint32_t rng;
	uint16_t flags_used;
	uint16_t revealed_safe;
	uint8_t started;
	uint8_t game_over;
	uint8_t won;
	uint8_t flag_mode;
} minesweeper_state_s;

static minesweeper_state_s g_game = {0};

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

static lv_obj_t *create_stat_card(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
				  lv_coord_t w, lv_coord_t h, const char *title,
				  uint32_t value_color, const lv_font_t *font)
{
	lv_obj_t *card = create_panel(parent, x, y, w, h, COLOR_PANEL_ALT, COLOR_LINE);
	lv_obj_t *title_lbl = lv_label_create(card);
	lv_obj_t *value_lbl = lv_label_create(card);

	lv_obj_set_pos(title_lbl, 16, 14);
	lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(title_lbl, lv_color_hex(COLOR_DIM), 0);
	lv_label_set_text(title_lbl, title);

	lv_obj_set_pos(value_lbl, 16, 40);
	lv_obj_set_width(value_lbl, w - 32);
	lv_label_set_long_mode(value_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(value_lbl, font, 0);
	lv_obj_set_style_text_color(value_lbl, lv_color_hex(value_color), 0);
	lv_label_set_text(value_lbl, "--");
	return value_lbl;
}

static lv_obj_t *create_action_button(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
				      lv_coord_t w, lv_coord_t h, uint32_t bg,
				      lv_event_cb_t cb, void *user_data, lv_obj_t **label_out)
{
	lv_obj_t *btn = lv_btn_create(parent);
	lv_obj_t *label = lv_label_create(btn);

	lv_obj_set_pos(btn, x, y);
	lv_obj_set_size(btn, w, h);
	lv_obj_set_style_radius(btn, 20, 0);
	lv_obj_set_style_bg_color(btn, lv_color_hex(bg), 0);
	lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
	lv_obj_set_style_border_width(btn, 0, 0);
	lv_obj_set_style_shadow_width(btn, 0, 0);
	lv_obj_set_style_outline_width(btn, 0, LV_STATE_FOCUSED);
	lv_obj_set_style_pad_all(btn, 0, 0);
	lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);

	lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(COLOR_TEXT), 0);
	lv_label_set_text(label, "--");
	lv_obj_center(label);

	if (label_out != NULL) {
		*label_out = label;
	}
	return btn;
}

static void minesweeper_focus_keys(void)
{
	if (g_game.key_anchor != NULL) {
		lv_group_focus_obj(g_game.key_anchor);
	}
}

static uint32_t minesweeper_next_random(void)
{
	g_game.rng = (g_game.rng * 1664525U) + 1013904223U;
	return g_game.rng;
}

static uint8_t minesweeper_in_bounds(int x, int y)
{
	return x >= 0 && x < BOARD_COLS && y >= 0 && y < BOARD_ROWS;
}

static uint8_t minesweeper_in_safe_zone(int x, int y, int safe_x, int safe_y)
{
	return x >= (safe_x - 1) && x <= (safe_x + 1) &&
	       y >= (safe_y - 1) && y <= (safe_y + 1);
}

static uint32_t minesweeper_number_color(uint8_t adjacent)
{
	switch (adjacent) {
		case 1:
			return 0x3d76d4;
		case 2:
			return 0x3f9865;
		case 3:
			return 0xc7664b;
		case 4:
			return 0x8b63d1;
		case 5:
			return 0xc74d64;
		case 6:
			return 0x3d98a0;
		case 7:
			return 0x4a5d72;
		default:
			return COLOR_TEXT;
	}
}

static void minesweeper_refresh_hud(void);
static void minesweeper_render_board(void);

static void minesweeper_reset_board_state(uint64_t mono_ms)
{
	for (int y = 0; y < BOARD_ROWS; y++) {
		for (int x = 0; x < BOARD_COLS; x++) {
			g_game.cells[y][x].mine = 0;
			g_game.cells[y][x].revealed = 0;
			g_game.cells[y][x].flagged = 0;
			g_game.cells[y][x].adjacent = 0;
		}
	}

	g_game.flags_used = 0;
	g_game.revealed_safe = 0;
	g_game.started = 0;
	g_game.game_over = 0;
	g_game.won = 0;
	g_game.flag_mode = 0;
	g_game.rng = (uint32_t)(mono_ms ^ (mono_ms >> 17) ^ 0x4d494e45U);
	if (g_game.rng == 0U) {
		g_game.rng = 0x6d696e65U;
	}
}

static uint8_t minesweeper_count_adjacent_mines(int x, int y)
{
	uint8_t count = 0;

	for (int ny = y - 1; ny <= y + 1; ny++) {
		for (int nx = x - 1; nx <= x + 1; nx++) {
			if ((nx == x && ny == y) || !minesweeper_in_bounds(nx, ny)) {
				continue;
			}
			count += g_game.cells[ny][nx].mine ? 1U : 0U;
		}
	}
	return count;
}

static void minesweeper_place_mines(int safe_x, int safe_y)
{
	uint16_t choices[BOARD_CAPACITY] = {0};
	uint16_t choice_count = 0;

	for (int y = 0; y < BOARD_ROWS; y++) {
		for (int x = 0; x < BOARD_COLS; x++) {
			if (minesweeper_in_safe_zone(x, y, safe_x, safe_y)) {
				continue;
			}
			choices[choice_count++] = (uint16_t)(y * BOARD_COLS + x);
		}
	}

	for (uint16_t i = 0; i < BOARD_MINES && i < choice_count; i++) {
		uint16_t remain = (uint16_t)(choice_count - i);
		uint16_t swap_index = (uint16_t)(i + (minesweeper_next_random() % remain));
		uint16_t temp = choices[i];
		uint16_t picked = 0;
		int mine_x = 0;
		int mine_y = 0;

		choices[i] = choices[swap_index];
		choices[swap_index] = temp;
		picked = choices[i];
		mine_x = (int)(picked % BOARD_COLS);
		mine_y = (int)(picked / BOARD_COLS);
		g_game.cells[mine_y][mine_x].mine = 1;
	}

	for (int y = 0; y < BOARD_ROWS; y++) {
		for (int x = 0; x < BOARD_COLS; x++) {
			if (g_game.cells[y][x].mine) {
				continue;
			}
			g_game.cells[y][x].adjacent = minesweeper_count_adjacent_mines(x, y);
		}
	}

	g_game.started = 1;
}

static void minesweeper_finish_loss(void)
{
	g_game.game_over = 1;
	g_game.won = 0;
	log_info("minesweeper: mine triggered\n");
}

static void minesweeper_finish_win(void)
{
	for (int y = 0; y < BOARD_ROWS; y++) {
		for (int x = 0; x < BOARD_COLS; x++) {
			if (g_game.cells[y][x].mine && !g_game.cells[y][x].flagged) {
				g_game.cells[y][x].flagged = 1;
			}
		}
	}
	g_game.flags_used = BOARD_MINES;
	g_game.game_over = 1;
	g_game.won = 1;
	log_info("minesweeper: board cleared\n");
}

static void minesweeper_reveal_region(int start_x, int start_y)
{
	int queue_x[BOARD_CAPACITY] = {0};
	int queue_y[BOARD_CAPACITY] = {0};
	uint8_t queued[BOARD_ROWS][BOARD_COLS] = {{0}};
	uint16_t head = 0;
	uint16_t tail = 0;

	queue_x[tail] = start_x;
	queue_y[tail] = start_y;
	queued[start_y][start_x] = 1;
	tail++;

	while (head < tail) {
		int x = queue_x[head];
		int y = queue_y[head];
		minesweeper_cell_s *cell = &g_game.cells[y][x];
		head++;

		if (cell->revealed || cell->flagged || cell->mine) {
			continue;
		}

		cell->revealed = 1;
		g_game.revealed_safe++;
		if (cell->adjacent != 0U) {
			continue;
		}

		for (int ny = y - 1; ny <= y + 1; ny++) {
			for (int nx = x - 1; nx <= x + 1; nx++) {
				minesweeper_cell_s *next = NULL;

				if ((nx == x && ny == y) || !minesweeper_in_bounds(nx, ny) || queued[ny][nx]) {
					continue;
				}
				next = &g_game.cells[ny][nx];
				if (next->mine || next->flagged || next->revealed) {
					continue;
				}

				queue_x[tail] = nx;
				queue_y[tail] = ny;
				queued[ny][nx] = 1;
				tail++;
			}
		}
	}
}

static void minesweeper_reveal_cell(int x, int y)
{
	minesweeper_cell_s *cell = NULL;

	if (!minesweeper_in_bounds(x, y) || g_game.game_over) {
		return;
	}

	cell = &g_game.cells[y][x];
	if (cell->revealed || cell->flagged) {
		return;
	}

	if (!g_game.started) {
		minesweeper_place_mines(x, y);
	}

	if (cell->mine) {
		cell->revealed = 1;
		minesweeper_finish_loss();
		minesweeper_render_board();
		return;
	}

	minesweeper_reveal_region(x, y);
	if (g_game.revealed_safe >= BOARD_SAFE_CELLS) {
		minesweeper_finish_win();
	}
	minesweeper_render_board();
}

static void minesweeper_toggle_flag(int x, int y)
{
	minesweeper_cell_s *cell = NULL;

	if (!minesweeper_in_bounds(x, y) || g_game.game_over) {
		return;
	}

	cell = &g_game.cells[y][x];
	if (cell->revealed) {
		return;
	}

	if (cell->flagged) {
		cell->flagged = 0;
		if (g_game.flags_used > 0U) {
			g_game.flags_used--;
		}
	} else {
		if (g_game.flags_used >= BOARD_MINES) {
			minesweeper_refresh_hud();
			return;
		}
		cell->flagged = 1;
		g_game.flags_used++;
	}

	minesweeper_render_board();
}

static void minesweeper_set_mode(uint8_t flag_mode)
{
	g_game.flag_mode = flag_mode ? 1U : 0U;
	minesweeper_refresh_hud();
}

static void minesweeper_toggle_mode(void)
{
	minesweeper_set_mode(!g_game.flag_mode);
}

static void minesweeper_render_cell(int x, int y)
{
	minesweeper_cell_s *cell = &g_game.cells[y][x];
	const char *text = "";
	uint32_t bg = COLOR_CELL_COVER;
	uint32_t border = COLOR_LINE;
	uint32_t text_color = COLOR_TEXT;
	uint8_t bg_opa = LV_OPA_COVER;
	char digit[2] = {0};

	if (cell->revealed) {
		if (cell->mine) {
			bg = COLOR_ALERT;
			border = COLOR_ALERT;
			text = "*";
			text_color = COLOR_PANEL;
		} else {
			bg = COLOR_CELL_OPEN;
			border = COLOR_LINE;
			bg_opa = LV_OPA_90;
			if (cell->adjacent > 0U) {
				digit[0] = (char)('0' + cell->adjacent);
				digit[1] = '\0';
				text = digit;
				text_color = minesweeper_number_color(cell->adjacent);
			}
		}
	} else if (cell->flagged) {
		if (g_game.game_over && !cell->mine) {
			bg = COLOR_ALERT_SOFT;
			border = COLOR_ALERT;
			text = "x";
			text_color = COLOR_ALERT;
		} else {
			bg = COLOR_WARM_SOFT;
			border = COLOR_WARM;
			text = "F";
			text_color = COLOR_WARM;
		}
	} else if (g_game.game_over && cell->mine) {
		bg = COLOR_ALERT;
		border = COLOR_ALERT;
		text = "*";
		text_color = COLOR_PANEL;
	} else {
		bg = COLOR_CELL_COVER;
		border = COLOR_LINE;
		text = "";
		text_color = COLOR_TEXT;
	}

	lv_obj_set_style_bg_color(cell->button, lv_color_hex(bg), 0);
	lv_obj_set_style_bg_opa(cell->button, bg_opa, 0);
	lv_obj_set_style_border_color(cell->button, lv_color_hex(border), 0);
	lv_label_set_text(cell->label, text);
	lv_obj_set_style_text_color(cell->label, lv_color_hex(text_color), 0);
	lv_obj_center(cell->label);
}

static void minesweeper_refresh_hud(void)
{
	char text[64] = {0};
	uint16_t flags_left = g_game.flags_used >= BOARD_MINES ? 0U : (uint16_t)(BOARD_MINES - g_game.flags_used);

	snprintf(text, sizeof(text), "%u", (unsigned int)BOARD_MINES);
	set_label_text(g_game.mine_value, text, COLOR_ALERT);

	snprintf(text, sizeof(text), "%u", (unsigned int)flags_left);
	set_label_text(g_game.flag_value, text, COLOR_WARM);

	snprintf(text, sizeof(text), "%u / %u",
		 (unsigned int)g_game.revealed_safe,
		 (unsigned int)BOARD_SAFE_CELLS);
	set_label_text(g_game.clear_value, text, COLOR_ACCENT);

	set_label_text(g_game.mode_value,
		       g_game.flag_mode ? "Flag mode" : "Reveal mode",
		       g_game.flag_mode ? COLOR_WARM : COLOR_ACCENT);

	if (g_game.mode_btn_label != NULL) {
		lv_label_set_text(g_game.mode_btn_label,
				  g_game.flag_mode ? "Switch To Reveal" : "Switch To Flag");
		lv_obj_center(g_game.mode_btn_label);
	}
	if (g_game.mode_btn != NULL) {
		lv_obj_set_style_bg_color(g_game.mode_btn,
					  lv_color_hex(g_game.flag_mode ? COLOR_WARM_SOFT : COLOR_ACCENT_SOFT), 0);
	}

	if (g_game.won) {
		set_label_text(g_game.status_value,
			       "Field cleared.",
			       COLOR_READY);
		set_label_text(g_game.hint_value,
			       "Restart for a fresh board.",
			       COLOR_DIM);
		return;
	}
	if (g_game.game_over) {
		set_label_text(g_game.status_value,
			       "Mine hit.",
			       COLOR_ALERT);
		set_label_text(g_game.hint_value,
			       "Restart to reshuffle the field.",
			       COLOR_DIM);
		return;
	}
	if (!g_game.started) {
		set_label_text(g_game.status_value,
			       "First tap opens a safe zone.",
			       COLOR_READY);
	} else if (g_game.flag_mode) {
		set_label_text(g_game.status_value,
			       "Flag mode. Tap to mark tiles.",
			       COLOR_WARM);
	} else {
		set_label_text(g_game.status_value,
			       "Reveal mode. Tap to clear tiles.",
			       COLOR_ACCENT);
	}

	set_label_text(g_game.hint_value,
		       "Tap reveals. Long press flags.\nF toggles mode. R restarts.",
		       COLOR_DIM);
}

static void minesweeper_render_board(void)
{
	for (int y = 0; y < BOARD_ROWS; y++) {
		for (int x = 0; x < BOARD_COLS; x++) {
			minesweeper_render_cell(x, y);
		}
	}
	minesweeper_refresh_hud();
}

static void minesweeper_new_game(uint64_t mono_ms)
{
	minesweeper_reset_board_state(mono_ms);
	minesweeper_render_board();
	log_info("minesweeper: new board ready\n");
}

static void minesweeper_cell_event_cb(lv_event_t *e)
{
	uintptr_t packed = (uintptr_t)lv_event_get_user_data(e);
	int x = (int)(packed % BOARD_COLS);
	int y = (int)(packed / BOARD_COLS);
	lv_event_code_t code = lv_event_get_code(e);

	if (code == LV_EVENT_SHORT_CLICKED) {
		if (g_game.flag_mode) {
			minesweeper_toggle_flag(x, y);
		} else {
			minesweeper_reveal_cell(x, y);
		}
	} else if (code == LV_EVENT_LONG_PRESSED) {
		minesweeper_toggle_flag(x, y);
	}

	minesweeper_focus_keys();
}

static void minesweeper_mode_event_cb(lv_event_t *e)
{
	if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
		return;
	}

	minesweeper_toggle_mode();
	minesweeper_focus_keys();
}

static void minesweeper_restart_event_cb(lv_event_t *e)
{
	if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
		return;
	}

	minesweeper_new_game(OSSysCtrlGetMonoTime() / 1000000ULL);
	minesweeper_focus_keys();
}

static void minesweeper_key_event_cb(lv_event_t *e)
{
	uint32_t key = 0;

	if (lv_event_get_code(e) != LV_EVENT_KEY) {
		return;
	}

	key = lv_indev_get_key(lv_indev_get_act());
	switch (key) {
		case 'f':
		case 'F':
			minesweeper_toggle_mode();
			break;
		case 'r':
		case 'R':
			minesweeper_new_game(OSSysCtrlGetMonoTime() / 1000000ULL);
			break;
		default:
			break;
	}
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
	lv_obj_t *label = NULL;
	lv_coord_t origin_x = 0;
	lv_coord_t origin_y = 0;

	lv_obj_set_style_bg_color(scr, lv_color_hex(COLOR_BG), 0);
	lv_obj_set_style_bg_grad_color(scr, lv_color_hex(COLOR_BG_ALT), 0);
	lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, 0);
	lv_obj_set_style_border_width(scr, 0, 0);
	clear_static_flags(scr);

	glow_left = lv_obj_create(scr);
	lv_obj_set_size(glow_left, 260, 260);
	lv_obj_set_pos(glow_left, -40, 150);
	lv_obj_set_style_radius(glow_left, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_bg_color(glow_left, lv_color_hex(COLOR_ACCENT), 0);
	lv_obj_set_style_bg_opa(glow_left, LV_OPA_20, 0);
	lv_obj_set_style_border_width(glow_left, 0, 0);
	clear_static_flags(glow_left);

	glow_right = lv_obj_create(scr);
	lv_obj_set_size(glow_right, 320, 320);
	lv_obj_set_pos(glow_right, 770, 120);
	lv_obj_set_style_radius(glow_right, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_bg_color(glow_right, lv_color_hex(COLOR_WARM), 0);
	lv_obj_set_style_bg_opa(glow_right, LV_OPA_20, 0);
	lv_obj_set_style_border_width(glow_right, 0, 0);
	clear_static_flags(glow_right);

	header = create_panel(scr, CONTENT_X, CONTENT_Y, CONTENT_W, HEADER_H, COLOR_PANEL, COLOR_LINE);

	label = lv_label_create(header);
	lv_obj_set_pos(label, 24, 18);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(COLOR_ACCENT), 0);
	lv_label_set_text(label, "BREAK MODE");

	label = lv_label_create(header);
	lv_obj_set_pos(label, 24, 40);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_48, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(COLOR_TEXT), 0);
	lv_label_set_text(label, "Minesweeper");

	label = lv_label_create(header);
	lv_obj_set_pos(label, 24, 92);
	lv_obj_set_width(label, CONTENT_W - 48);
	lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(COLOR_DIM), 0);
	lv_label_set_text(label, "Sweep the board clean. Tap to reveal, long press to flag, and use the mode switch when you want explicit marking control.");

	board_panel = create_panel(scr, BOARD_PANEL_X, BOARD_PANEL_Y, BOARD_PANEL_W, BOARD_PANEL_H, COLOR_PANEL, COLOR_LINE);

	label = lv_label_create(board_panel);
	lv_obj_set_pos(label, 24, 18);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(COLOR_DIM), 0);
	lv_label_set_text(label, "FIELD");

	label = lv_label_create(board_panel);
	lv_obj_set_pos(label, 24, 40);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(COLOR_TEXT), 0);
	lv_label_set_text(label, "10 x 10 grid with 16 hidden mines");

	board_surface = create_panel(board_panel, 24, 68, 564, 552, COLOR_PANEL_ALT, COLOR_LINE);
	origin_x = (lv_coord_t)((564 - BOARD_PIXEL_SIZE) / 2);
	origin_y = (lv_coord_t)((552 - BOARD_PIXEL_SIZE) / 2);

	for (int y = 0; y < BOARD_ROWS; y++) {
		for (int x = 0; x < BOARD_COLS; x++) {
			lv_obj_t *cell_btn = lv_btn_create(board_surface);
			lv_obj_t *cell_label = lv_label_create(cell_btn);
			uintptr_t index = (uintptr_t)(y * BOARD_COLS + x);

			lv_obj_set_pos(cell_btn,
				       origin_x + (x * (BOARD_CELL_SIZE + BOARD_GAP)),
				       origin_y + (y * (BOARD_CELL_SIZE + BOARD_GAP)));
			lv_obj_set_size(cell_btn, BOARD_CELL_SIZE, BOARD_CELL_SIZE);
			lv_obj_set_style_radius(cell_btn, 16, 0);
			lv_obj_set_style_bg_color(cell_btn, lv_color_hex(COLOR_CELL_COVER), 0);
			lv_obj_set_style_bg_opa(cell_btn, LV_OPA_COVER, 0);
			lv_obj_set_style_border_color(cell_btn, lv_color_hex(COLOR_LINE), 0);
			lv_obj_set_style_border_width(cell_btn, 1, 0);
			lv_obj_set_style_shadow_width(cell_btn, 0, 0);
			lv_obj_set_style_outline_width(cell_btn, 0, LV_STATE_FOCUSED);
			lv_obj_set_style_pad_all(cell_btn, 0, 0);
			lv_obj_add_event_cb(cell_btn, minesweeper_cell_event_cb, LV_EVENT_SHORT_CLICKED, (void *)index);
			lv_obj_add_event_cb(cell_btn, minesweeper_cell_event_cb, LV_EVENT_LONG_PRESSED, (void *)index);

			lv_obj_set_style_text_font(cell_label, &lv_font_montserrat_24, 0);
			lv_obj_set_style_text_color(cell_label, lv_color_hex(COLOR_TEXT), 0);
			lv_label_set_text(cell_label, "");
			lv_obj_center(cell_label);

			g_game.cells[y][x].button = cell_btn;
			g_game.cells[y][x].label = cell_label;
		}
	}

	side_panel = create_panel(scr, SIDE_PANEL_X, SIDE_PANEL_Y, SIDE_PANEL_W, SIDE_PANEL_H, COLOR_PANEL, COLOR_LINE);

	label = lv_label_create(side_panel);
	lv_obj_set_pos(label, 24, 18);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(COLOR_DIM), 0);
	lv_label_set_text(label, "TACTIC");

	label = lv_label_create(side_panel);
	lv_obj_set_pos(label, 24, 40);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_32, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(COLOR_TEXT), 0);
	lv_label_set_text(label, "Sweep");

	g_game.mine_value = create_stat_card(side_panel, 24, 104, 120, 86, "Mines", COLOR_ALERT, &lv_font_montserrat_24);
	g_game.flag_value = create_stat_card(side_panel, 168, 104, 120, 86, "Flags left", COLOR_WARM, &lv_font_montserrat_24);
	g_game.clear_value = create_stat_card(side_panel, 24, 198, 264, 86, "Cleared", COLOR_ACCENT, &lv_font_montserrat_24);
	g_game.mode_value = create_stat_card(side_panel, 24, 300, 264, 72, "Mode", COLOR_ACCENT, &lv_font_montserrat_24);

	label = lv_label_create(side_panel);
	lv_obj_set_pos(label, 24, 388);
	lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(label, lv_color_hex(COLOR_DIM), 0);
	lv_label_set_text(label, "Controls");

	g_game.mode_btn = create_action_button(side_panel, 24, 412, 264, 48,
					       COLOR_ACCENT_SOFT, minesweeper_mode_event_cb,
					       NULL, &g_game.mode_btn_label);
	g_game.key_anchor = lv_btn_create(side_panel);
	lv_obj_set_pos(g_game.key_anchor, 0, 0);
	lv_obj_set_size(g_game.key_anchor, 1, 1);
	lv_obj_set_style_bg_opa(g_game.key_anchor, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(g_game.key_anchor, 0, 0);
	lv_obj_set_style_outline_width(g_game.key_anchor, 0, LV_STATE_FOCUSED);
	lv_obj_set_style_shadow_width(g_game.key_anchor, 0, 0);
	lv_obj_set_style_pad_all(g_game.key_anchor, 0, 0);
	lv_obj_clear_flag(g_game.key_anchor, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_event_cb(g_game.key_anchor, minesweeper_key_event_cb, LV_EVENT_KEY, NULL);

	(void)create_action_button(side_panel, 24, 472, 264, 48,
				   COLOR_PANEL_ALT, minesweeper_restart_event_cb,
				   NULL, &label);
	lv_label_set_text(label, "Restart");
	lv_obj_center(label);

	g_game.status_value = lv_label_create(side_panel);
	lv_obj_set_pos(g_game.status_value, 24, 548);
	lv_obj_set_width(g_game.status_value, 264);
	lv_label_set_long_mode(g_game.status_value, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(g_game.status_value, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(g_game.status_value, lv_color_hex(COLOR_TEXT), 0);
	lv_label_set_text(g_game.status_value, "--");

	g_game.hint_value = lv_label_create(side_panel);
	lv_obj_set_pos(g_game.hint_value, 24, 584);
	lv_obj_set_width(g_game.hint_value, 264);
	lv_label_set_long_mode(g_game.hint_value, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_font(g_game.hint_value, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(g_game.hint_value, lv_color_hex(COLOR_DIM), 0);
	lv_label_set_text(g_game.hint_value, "--");

	minesweeper_focus_keys();
}

static void minesweeper_on_create(app_s *app)
{
	(void)app;

	create_ui();
	minesweeper_new_game(OSSysCtrlGetMonoTime() / 1000000ULL);
}

static void minesweeper_on_update(app_s *app, uint64_t mono_ms)
{
	(void)app;
	(void)mono_ms;
}

int main(int argc, char *argv[])
{
	app_s app = {0};
	app_config_s config = {
		.name = "minesweeper",
		.x = APP_DEFAULT_X,
		.y = APP_DEFAULT_Y,
		.z = 0,
		.width = APP_DEFAULT_WIDTH,
		.height = APP_DEFAULT_HEIGHT,
		.window_flags = WINDOW_FLAG_FOCUSABLE,
		.enable_input = 1,
	};
	app_lifecycle_ops_s lifecycle = {
		.on_create = minesweeper_on_create,
		.on_update = minesweeper_on_update,
	};

	(void)argc;
	(void)argv;

	log_info("minesweeper start!\n");
	app_init(&app, &config, &lifecycle, NULL);
	return app_run(&app);
}
