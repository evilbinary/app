/*
 * 共享字体 + 客户端字体覆盖。
 *
 * cairo 的 toy 字体 API 在没有 fontconfig 时会退回内置 twin 字体。
 * 各客户端在自己的 cairo 上下文里显式调用 yiyiya_font_apply()：
 *   - family 为空 / "sans-serif" / "serif" 等通用名 -> 共用默认字体
 *   - family 是 /font/xxx.ttf 这样的路径 -> 用调用方指定的字体
 *   - "monospace" -> 共用等宽字体
 *
 * 默认字体可以用环境变量覆盖：
 *   YIYIYA_FONT=/font/Roboto-Light.ttf
 *   YIYIYA_FONT_MONO=/font/RobotoMono-Regular.ttf
 */

#include "yiyiya-font.h"

#include <cairo.h>
#include <cairo-ft.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define YIYIYA_DEFAULT_FONT      "/font/font.ttf"
#define YIYIYA_DEFAULT_MONO_FONT "/font/RobotoMono-Regular.ttf"

struct font_entry {
	char *path;
	FT_Face ft_face;
	struct font_entry *next;
};

static FT_Library font_library;
static int font_library_state;
static struct font_entry *font_cache;
static int font_atexit_registered;

/* 退出时只销毁 FT_Face/FT_Library。cairo font face 不再跨帧缓存：
 * 每次 yiyiya_font_apply() 创建一个 face，交给 cairo context 持有，
 * context 销毁时 face 会一起销毁，unscaled font 也会从 cairo 的全局
 * font map 里摘掉。这样 toytoolkit 的 cleanup_after_cairo() 不会在
 * cairo_debug_reset_static_data() 时命中 from_face 字体的断言。 */
void
yiyiya_font_fini(void)
{
	struct font_entry *entry;

	entry = font_cache;
	while (entry != NULL) {
		struct font_entry *next = entry->next;

		if (entry->ft_face != NULL)
			FT_Done_Face(entry->ft_face);
		free(entry->path);
		free(entry);
		entry = next;
	}
	font_cache = NULL;

	if (font_library_state > 0) {
		FT_Done_FreeType(font_library);
		font_library_state = 0;
	}
}

static const char *
font_env_or(const char *name, const char *fallback)
{
	const char *value = getenv(name);

	if (value != NULL && *value != '\0' && access(value, R_OK) == 0)
		return value;
	return fallback;
}

static const char *
font_default_path(void)
{
	return font_env_or("YIYIYA_FONT", YIYIYA_DEFAULT_FONT);
}

static const char *
font_mono_path(void)
{
	return font_env_or("YIYIYA_FONT_MONO", YIYIYA_DEFAULT_MONO_FONT);
}

static const char *
font_resolve(const char *family)
{
	if (family != NULL && *family != '\0') {
		if ((strchr(family, '/') != NULL ||
		     strstr(family, ".ttf") != NULL) &&
		    access(family, R_OK) == 0)
			return family;
		if (strcmp(family, "monospace") == 0)
			return font_mono_path();
	}

	return font_default_path();
}

static FT_Face
font_load(const char *path)
{
	struct font_entry *entry;
	FT_Face ft_face;

	for (entry = font_cache; entry != NULL; entry = entry->next) {
		if (strcmp(entry->path, path) == 0)
			return entry->ft_face;
	}

	if (!font_atexit_registered) {
		atexit(yiyiya_font_fini);
		font_atexit_registered = 1;
	}

	if (font_library_state == 0) {
		font_library_state = FT_Init_FreeType(&font_library) == 0 ? 1 : -1;
	}
	if (font_library_state < 0)
		return NULL;

	if (FT_New_Face(font_library, path, 0, &ft_face) != 0)
		return NULL;

	entry = calloc(1, sizeof(*entry));
	if (entry == NULL) {
		FT_Done_Face(ft_face);
		return NULL;
	}
	entry->path = strdup(path);
	if (entry->path == NULL) {
		free(entry);
		FT_Done_Face(ft_face);
		return NULL;
	}
	entry->ft_face = ft_face;
	entry->next = font_cache;
	font_cache = entry;

	return ft_face;
}

cairo_font_face_t *
yiyiya_font_face(const char *family)
{
	FT_Face ft_face = font_load(font_resolve(family));
	cairo_font_face_t *face;
	cairo_status_t status;

	if (ft_face == NULL)
		return NULL;

	face = cairo_ft_font_face_create_for_ft_face(ft_face, 0);
	status = cairo_font_face_status(face);
	if (status != CAIRO_STATUS_SUCCESS) {
		cairo_font_face_destroy(face);
		return NULL;
	}
	return face;
}

void
yiyiya_font_apply(cairo_t *cr, const char *family)
{
	cairo_font_face_t *face = yiyiya_font_face(family);

	if (face != NULL) {
		cairo_set_font_face(cr, face);
		/* context 已持有引用；这里松开我们的引用，等 context 销毁时
		 * cairo font face / unscaled font 一并摘掉。 */
		cairo_font_face_destroy(face);
	}
}
