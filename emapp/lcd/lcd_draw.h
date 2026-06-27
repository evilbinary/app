#ifndef LCD_DRAW_H
#define LCD_DRAW_H

#include "main.h"

int lcd_open(void);
void lcd_fill_rect(int x, int y, int w, int h, u16 color);
void lcd_draw_pixel(int x, int y, u16 color);
void lcd_draw_line(int x1, int y1, int x2, int y2, u16 color);
void lcd_draw_rect(int x, int y, int w, int h, u16 color);
void lcd_draw_text(int x, int y, u16 color, int size, const char* text);
void lcd_draw_text_bg(int x, int y, u16 color, u16 bg_color, int size,
                      const char* text);

#endif
