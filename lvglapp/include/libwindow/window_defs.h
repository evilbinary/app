#ifndef __YIYIYA_LIBWINDOW_WINDOW_DEFS_H__
#define __YIYIYA_LIBWINDOW_WINDOW_DEFS_H__

#include <stddef.h>
#include <stdint.h>

#define WINDOW_PIXEL_BYTES (sizeof(uint32_t))
#define WINDOW_FLAG_FOCUSABLE (0x1U)
#define WINDOW_FLAG_ALPHA_BLEND (0x2U)
#define WINDOW_LOCAL_EVENT_QUEUE_SIZE (32U)
#define WINDOW_EVENT_LIST_MAX (WINDOW_LOCAL_EVENT_QUEUE_SIZE)

typedef enum window_event_type {
	WINDOW_EVENT_NONE = 0,
	WINDOW_EVENT_KEY = 0x1,
	WINDOW_EVENT_POINTER = 0x2,
	WINDOW_EVENT_FOREGROUND = 0x3,
	WINDOW_EVENT_BACKGROUND = 0x4,
	WINDOW_EVENT_VSYNC = 0x5,
} window_event_type_t;

typedef enum window_pointer_value {
	WINDOW_POINTER_RELEASE = 0,
	WINDOW_POINTER_PRESS = 0x1,
	WINDOW_POINTER_MOVE = 0x2,
} window_pointer_value_t;

typedef struct window_event {
	uint32_t type;
	uint32_t code;
	int32_t value;
	int32_t x;
	int32_t y;
} window_event_s;

typedef struct window_rect {
	int32_t x;
	int32_t y;
	uint32_t width;
	uint32_t height;
} window_rect_s;

typedef struct window_event_list {
	uint32_t count;
	uint32_t reserved;
	window_event_s events[WINDOW_EVENT_LIST_MAX];
} window_event_list_s;

typedef struct window_surface {
	uint64_t shm;
	void *pixels;
} window_surface_s;

#endif /* __YIYIYA_LIBWINDOW_WINDOW_DEFS_H__ */
