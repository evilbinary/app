#ifndef YIYIYA_FONT_H
#define YIYIYA_FONT_H

#include <cairo.h>

/*
 * 获取指定字体：family 为空/通用名 -> 共享默认字体；family 是
 * /font/xxx.ttf 路径 -> 该字体；"monospace" -> 共享等宽字体。
 * 返回的是新的 cairo font face 引用，调用方负责 cairo_font_face_destroy()。
 */
cairo_font_face_t *yiyiya_font_face(const char *family);

/* 把字体设置到 cairo 上下文；加载失败时保持 cairo 当前字体不变。 */
void yiyiya_font_apply(cairo_t *cr, const char *family);

/* 销毁 FT_Face 缓存和 FT 库（atexit 调用）。cairo font face 不再缓存，
 * 由各自的 cairo context 销毁。 */
void yiyiya_font_fini(void);

#endif
