#include "microui.h"
#include "screen.h"
#include "stdio.h"
#include "syscall.h"

char *buf = "hello,microgui\n";
static float bg[3] = {90, 95, 100};
#define write_log printf

static int text_width(mu_Font font, const char *text, int len) {
  if (len == -1) {
    len = strlen(text);
  }
  return r_get_text_width(text, len);
}

static int text_height(mu_Font font) { return r_get_text_height(); }

static int uint8_slider(mu_Context *ctx, unsigned char *value, int low,
                        int high) {
  static float tmp;
  mu_push_id(ctx, &value, sizeof(value));
  tmp = *value;
  int res = mu_slider_ex(ctx, &tmp, low, high, 0, "%.0f", MU_OPT_ALIGNCENTER);
  *value = tmp;
  mu_pop_id(ctx);
  return res;
}
static void style_window(mu_Context *ctx) {
  static struct {
    const char *label;
    int idx;
  } colors[] = {{"text:", MU_COLOR_TEXT},
                {"border:", MU_COLOR_BORDER},
                {"windowbg:", MU_COLOR_WINDOWBG},
                {"titlebg:", MU_COLOR_TITLEBG},
                {"titletext:", MU_COLOR_TITLETEXT},
                {"panelbg:", MU_COLOR_PANELBG},
                {"button:", MU_COLOR_BUTTON},
                {"buttonhover:", MU_COLOR_BUTTONHOVER},
                {"buttonfocus:", MU_COLOR_BUTTONFOCUS},
                {"base:", MU_COLOR_BASE},
                {"basehover:", MU_COLOR_BASEHOVER},
                {"basefocus:", MU_COLOR_BASEFOCUS},
                {"scrollbase:", MU_COLOR_SCROLLBASE},
                {"scrollthumb:", MU_COLOR_SCROLLTHUMB},
                {NULL}};

  if (mu_begin_window(ctx, "Style Editor", mu_rect(350, 250, 300, 240))) {
    int sw = mu_get_current_container(ctx)->body.w * 0.14;
    mu_layout_row(ctx, 6, (int[]){80, sw, sw, sw, sw, -1}, 0);
    for (int i = 0; colors[i].label; i++) {
      mu_label(ctx, colors[i].label);
      uint8_slider(ctx, &ctx->style->colors[i].r, 0, 255);
      uint8_slider(ctx, &ctx->style->colors[i].g, 0, 255);
      uint8_slider(ctx, &ctx->style->colors[i].b, 0, 255);
      uint8_slider(ctx, &ctx->style->colors[i].a, 0, 255);
      mu_draw_rect(ctx, mu_layout_next(ctx), ctx->style->colors[i]);
    }
    mu_end_window(ctx);
  }
}

static void test_window(mu_Context *ctx) {
  /* do window */
  if (mu_begin_window(ctx, "Demo Window", mu_rect(40, 40, 300, 450))) {
    mu_Container *win = mu_get_current_container(ctx);
    win->rect.w = mu_max(win->rect.w, 240);
    win->rect.h = mu_max(win->rect.h, 300);

    /* window info */
    if (mu_header(ctx, "Window Info")) {
      mu_Container *win = mu_get_current_container(ctx);
      char buf[64];
      mu_layout_row(ctx, 2, (int[]){54, -1}, 0);
      mu_label(ctx, "Position:");
      sprintf(buf, "%d, %d", win->rect.x, win->rect.y);
      mu_label(ctx, buf);
      mu_label(ctx, "Size:");
      sprintf(buf, "%d, %d", win->rect.w, win->rect.h);
      mu_label(ctx, buf);
    }

    /* labels + buttons */
    if (mu_header_ex(ctx, "Test Buttons", MU_OPT_EXPANDED)) {
      mu_layout_row(ctx, 3, (int[]){86, -110, -1}, 0);
      mu_label(ctx, "Test buttons 1:");
      if (mu_button(ctx, "Button 1")) {
        write_log("Pressed button 1");
      }
      if (mu_button(ctx, "Button 2")) {
        write_log("Pressed button 2");
      }
      mu_label(ctx, "Test buttons 2:");
      if (mu_button(ctx, "Button 3")) {
        write_log("Pressed button 3");
      }
      if (mu_button(ctx, "Popup")) {
        mu_open_popup(ctx, "Test Popup");
      }
      if (mu_begin_popup(ctx, "Test Popup")) {
        mu_button(ctx, "Hello");
        mu_button(ctx, "World");
        mu_end_popup(ctx);
      }
    }

    /* tree */
    if (mu_header_ex(ctx, "Tree and Text", MU_OPT_EXPANDED)) {
      mu_layout_row(ctx, 2, (int[]){140, -1}, 0);
      mu_layout_begin_column(ctx);
      if (mu_begin_treenode(ctx, "Test 1")) {
        if (mu_begin_treenode(ctx, "Test 1a")) {
          mu_label(ctx, "Hello");
          mu_label(ctx, "world");
          mu_end_treenode(ctx);
        }
        if (mu_begin_treenode(ctx, "Test 1b")) {
          if (mu_button(ctx, "Button 1")) {
            write_log("Pressed button 1");
          }
          if (mu_button(ctx, "Button 2")) {
            write_log("Pressed button 2");
          }
          mu_end_treenode(ctx);
        }
        mu_end_treenode(ctx);
      }
      if (mu_begin_treenode(ctx, "Test 2")) {
        mu_layout_row(ctx, 2, (int[]){54, 54}, 0);
        if (mu_button(ctx, "Button 3")) {
          write_log("Pressed button 3");
        }
        if (mu_button(ctx, "Button 4")) {
          write_log("Pressed button 4");
        }
        if (mu_button(ctx, "Button 5")) {
          write_log("Pressed button 5");
        }
        if (mu_button(ctx, "Button 6")) {
          write_log("Pressed button 6");
        }
        mu_end_treenode(ctx);
      }
      if (mu_begin_treenode(ctx, "Test 3")) {
        static int checks[3] = {1, 0, 1};
        mu_checkbox(ctx, "Checkbox 1", &checks[0]);
        mu_checkbox(ctx, "Checkbox 2", &checks[1]);
        mu_checkbox(ctx, "Checkbox 3", &checks[2]);
        mu_end_treenode(ctx);
      }
      mu_layout_end_column(ctx);

      mu_layout_begin_column(ctx);
      mu_layout_row(ctx, 1, (int[]){-1}, 0);
      mu_text(
          ctx,
          "Lorem ipsum dolor sit amet, consectetur adipiscing "
          "elit. Maecenas lacinia, sem eu lacinia molestie, mi risus faucibus "
          "ipsum, eu varius magna felis a nulla.");
      mu_layout_end_column(ctx);
    }

    /* background color sliders */
    if (mu_header_ex(ctx, "Background Color", MU_OPT_EXPANDED)) {
      mu_layout_row(ctx, 2, (int[]){-78, -1}, 74);
      /* sliders */
      mu_layout_begin_column(ctx);
      mu_layout_row(ctx, 2, (int[]){46, -1}, 0);
      mu_label(ctx, "Red:");
      mu_slider(ctx, &bg[0], 0, 255);
      mu_label(ctx, "Green:");
      mu_slider(ctx, &bg[1], 0, 255);
      mu_label(ctx, "Blue:");
      mu_slider(ctx, &bg[2], 0, 255);
      mu_layout_end_column(ctx);
      /* color preview */
      mu_Rect r = mu_layout_next(ctx);
      mu_draw_rect(ctx, r, mu_color(bg[0], bg[1], bg[2], 255));
      char buf[32];
      sprintf(buf, "#%02X%02X%02X", (int)bg[0], (int)bg[1], (int)bg[2]);
      mu_draw_control_text(ctx, buf, r, MU_COLOR_TEXT, MU_OPT_ALIGNCENTER);
    }

    mu_end_window(ctx);
  }
}
static void process_frame(mu_Context *ctx) {
  mu_begin(ctx);
  style_window(ctx);
  test_window(ctx);
  mu_end(ctx);
}
// static const char button_map[256] = {
//     [SDL_BUTTON_LEFT & 0xff] = MU_MOUSE_LEFT,
//     [SDL_BUTTON_RIGHT & 0xff] = MU_MOUSE_RIGHT,
//     [SDL_BUTTON_MIDDLE & 0xff] = MU_MOUSE_MIDDLE,
// };

// static const char key_map[256] = {
//     // [ SDLK_LSHIFT       & 0xff ] = MU_KEY_SHIFT,
//     // [ SDLK_RSHIFT       & 0xff ] = MU_KEY_SHIFT,
//     // [ SDLK_LCTRL        & 0xff ] = MU_KEY_CTRL,
//     // [ SDLK_RCTRL        & 0xff ] = MU_KEY_CTRL,
//     // [ SDLK_LALT         & 0xff ] = MU_KEY_ALT,
//     // [ SDLK_RALT         & 0xff ] = MU_KEY_ALT,
//     // [ SDLK_RETURN       & 0xff ] = MU_KEY_RETURN,
//     // [ SDLK_BACKSPACE    & 0xff ] = MU_KEY_BACKSPACE,
// };

int main(int argc, char *argv[]) {
  printf(buf);
  r_init();
  mu_Context *ctx = malloc(sizeof(mu_Context));
  mu_init(ctx);
  ctx->text_width = text_width;
  ctx->text_height = text_height;

  mouse_data_t mouse;
  screen_info_t *screen;
  screen = screen_info();
  int i = 0;
  for (;;) {
    screen_printf(200, 10, "hello,YiYiYa OS");
    screen_printf(0, 600, "%d", i++);

    event_read_mouse(&mouse);
    if (mouse.sate) {
      mu_input_mousemove(ctx, mouse.x, screen->height - mouse.y);
    }
    u8 button = mouse.sate & BUTTON_MASK;
    
    if (button == BUTTON_LEFT) {
      screen_printf(0, 620, "%x", button);
      mu_input_mousedown(ctx, mouse.x, screen->height - mouse.y, MU_MOUSE_LEFT);
    } else if (button == BUTTON_RIGHT) {
      mu_input_mousedown(ctx, mouse.x, screen->height - mouse.y, MU_MOUSE_RIGHT);
    } else if (button == BUTTON_MIDDLE) {
      mu_input_mousedown(ctx, mouse.x, screen->height - mouse.y, MU_MOUSE_MIDDLE);
    }else if(button==0){
      mu_input_mouseup(ctx, mouse.x, screen->height - mouse.y, MU_MOUSE_LEFT);
      mu_input_mouseup(ctx, mouse.x, screen->height - mouse.y, MU_MOUSE_RIGHT);
      mu_input_mouseup(ctx, mouse.x, screen->height - mouse.y, MU_MOUSE_MIDDLE);
    }

   process_frame(ctx);
    /* render */
    r_clear(mu_color(bg[0], bg[1], bg[2], 100));
    mu_Command *cmd = NULL;
    while (mu_next_command(ctx, &cmd)) {
      switch (cmd->type) {
        case MU_COMMAND_TEXT:
          r_draw_text(cmd->text.str, cmd->text.pos, cmd->text.color);
          break;
        case MU_COMMAND_RECT:
          r_draw_rect(cmd->rect.rect, cmd->rect.color);
          break;
        case MU_COMMAND_ICON:
          r_draw_icon(cmd->icon.id, cmd->icon.rect, cmd->icon.color);
          break;
        case MU_COMMAND_CLIP:
          r_set_clip_rect(cmd->clip.rect);
          break;
      }
    }

    screen_fill_rect(mouse.x, screen->height - mouse.y, 4, 4,
                       0x00ff00);
    r_present();
  }
  return 0;
}