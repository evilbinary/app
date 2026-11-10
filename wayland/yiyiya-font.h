#ifndef YIYIYA_FONT_H
#define YIYIYA_FONT_H

#include <cairo.h>

/*
 * 获取共享字体/指定字体：
 *   family 为空或通用 family -> 共享默认字体
 *   family 是 /font/xxx.ttf 路径 -> 该字体
 *   family 是 "monospace" -> 共享等宽字体
 * 返回的 font face 由模块缓存，调用方不要销毁。
 */
cairo_font_face_t *yiyiya_font_face(const char *family);

/* 把字体设置到 cairo 上下文；加载失败时保持 cairo 当前字体不变。 */
void yiyiya_font_apply(cairo_t *cr, const char *family);

#endif
