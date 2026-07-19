#ifndef __YIYIYA_LIBWINDOW_WINDOW_H__
#define __YIYIYA_LIBWINDOW_WINDOW_H__

#include "window_defs.h"

// YiYiYa 适配：使用屏幕尺寸
#define APP_DEFAULT_X 0
#define APP_DEFAULT_Y 0
#define APP_DEFAULT_WIDTH 900U
#define APP_DEFAULT_HEIGHT 700U

typedef struct window window_s;

typedef void (*window_visibility_callback_fn)(window_s *window, void *opaque);

struct window {
	window_surface_s surface;
	window_surface_s back_surface;
	int32_t x;
	int32_t y;
	int32_t z;
	uint32_t width;
	uint32_t height;
	uint32_t stride;
	uint32_t flags;
	uint8_t visible;
	uint8_t has_input_region;
	window_rect_s input_region;
	window_visibility_callback_fn on_foreground;
	window_visibility_callback_fn on_background;
	void *callback_opaque;
	window_event_s pending_events[WINDOW_LOCAL_EVENT_QUEUE_SIZE];
	uint32_t pending_head;
	uint32_t pending_tail;
	uint8_t vsync_pending;
};

// YiYiYa 适配：简化实现
static inline uint64_t window_create(window_s *window, int32_t x, int32_t y, int32_t z,
		       uint32_t width, uint32_t height, uint32_t flags) {
	(void)window; (void)x; (void)y; (void)z; (void)width; (void)height; (void)flags;
	return 0;
}

static inline uint64_t window_destroy(window_s *window) {
	(void)window;
	return 0;
}

static inline uint64_t window_present(window_s *window) {
	(void)window;
	return 0;
}

static inline uint64_t window_poll_event(window_s *window, window_event_s *event) {
	(void)window; (void)event;
	return 0;
}

static inline uint64_t window_get_pointer_state(window_s *window, window_event_s *event) {
	(void)window; (void)event;
	return 0;
}

static inline void window_set_visibility_callbacks(window_s *window,
				     window_visibility_callback_fn on_foreground,
				     window_visibility_callback_fn on_background,
				     void *opaque) {
	(void)window; (void)on_foreground; (void)on_background; (void)opaque;
}

static inline void window_sync(window_s *window) {
	(void)window;
}

static inline uint64_t window_consume_vsync(window_s *window) {
	(void)window;
	return 1;  // YiYiYa 适配：始终返回有效，允许渲染
}

static inline uint64_t window_is_visible(const window_s *window) {
	(void)window;
	return 1;
}

static inline uint64_t window_activate_owner(uint64_t pid) {
	(void)pid;
	return 0;
}

static inline uint64_t window_set_position(window_s *window, int32_t x, int32_t y) {
	(void)window; (void)x; (void)y;
	return 0;
}

static inline uint64_t window_set_visible(window_s *window, uint8_t visible) {
	(void)window; (void)visible;
	return 0;
}

static inline uint64_t window_set_input_region(window_s *window, const window_rect_s *region) {
	(void)window; (void)region;
	return 0;
}

static inline uint64_t window_get_input_revision(void) {
	return 0;
}

static inline uint64_t window_get_last_input_mono_ms(void) {
	return 0;
}

#endif /* __YIYIYA_LIBWINDOW_WINDOW_H__ */
