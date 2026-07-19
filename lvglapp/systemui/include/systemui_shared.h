#ifndef __TRANQUILOS_APPS_SYSTEMUI_SHARED_H__
#define __TRANQUILOS_APPS_SYSTEMUI_SHARED_H__

#include "stdint.h"
#include "libsystem/ipc.h"
#include "libsystem/statemgr_client.h"
#include "libsystem/systemd_client.h"

#define SYSTEMUI_WIDTH 1024
#define SYSTEMUI_HEIGHT 1024

#define SYSTEMUI_STATUS_BAR_HEIGHT 72
#define SYSTEMUI_NAV_BAR_HEIGHT 108

#define SYSTEMUI_OVERLAY_WINDOW_Z 100
#define SYSTEMUI_STATUS_WINDOW_Z 110
#define SYSTEMUI_NAV_WINDOW_Z 110
#define SYSTEMUI_LOCK_WINDOW_Z 230
#define SYSTEMUI_DEBUG_WINDOW_Z 240

#define SYSTEMUI_APPEARANCE_REFRESH_INTERVAL_MS 100ULL
#define SYSTEMUI_IDLE_LOCK_TIMEOUT_DEFAULT_SEC 10U
#define SYSTEMUI_IDLE_LOCK_TIMEOUT_MEDIUM_SEC 15U
#define SYSTEMUI_IDLE_LOCK_TIMEOUT_LONG_SEC 30U
#define SYSTEMUI_IDLE_LOCK_RETRY_MS 500ULL
#define SYSTEMUI_TIMEZONE_OFFSET_HOURS 8

#define SYSTEMUI_LAUNCHER_PROCESS_PATH "./bin/launcher.elf"
#define SYSTEMUI_LOCKSCREEN_PROCESS_PATH "./bin/lockui.elf"
#define SYSTEMUI_STATE_KEY_IDLE_LOCK_TIMEOUT_SEC "ui.lockscreen.timeout_sec"

#define SYSTEMUI_COLOR_BG_LIGHT        0xfff4eeU
#define SYSTEMUI_COLOR_BG_DARK         0x151826U
#define SYSTEMUI_COLOR_PANEL_LIGHT     0xfffffbU
#define SYSTEMUI_COLOR_PANEL_DARK      0x1f2434U
#define SYSTEMUI_COLOR_PANEL_ALT_LIGHT 0xffefe7U
#define SYSTEMUI_COLOR_PANEL_ALT_DARK  0x2a3045U
#define SYSTEMUI_COLOR_LINE_LIGHT      0xf0ddd2U
#define SYSTEMUI_COLOR_LINE_DARK       0x434b67U
#define SYSTEMUI_COLOR_TEXT_LIGHT      0x24324aU
#define SYSTEMUI_COLOR_TEXT_DARK       0xfff6eeU
#define SYSTEMUI_COLOR_DIM_LIGHT       0x7e6d68U
#define SYSTEMUI_COLOR_DIM_DARK        0xc6b5adU
#define SYSTEMUI_COLOR_ACCENT_LIGHT    0xff7d5cU
#define SYSTEMUI_COLOR_ACCENT_DARK     0xffa07cU
#define SYSTEMUI_COLOR_SUCCESS_LIGHT   0x44be8bU
#define SYSTEMUI_COLOR_SUCCESS_DARK    0x74d7afU
#define SYSTEMUI_COLOR_WARNING_LIGHT   0xffb458U
#define SYSTEMUI_COLOR_WARNING_DARK    0xf5c575U
#define SYSTEMUI_COLOR_OVERLAY_LIGHT   0xfff6f1U
#define SYSTEMUI_COLOR_OVERLAY_DARK    0x090d16U

#define SYSTEMUI_DESKTOP_COLOR_SKY     0xeaf8ffU
#define SYSTEMUI_DESKTOP_COLOR_MINT    0xe9fbf2U
#define SYSTEMUI_DESKTOP_COLOR_SAND    0xffefe3U
#define SYSTEMUI_DESKTOP_COLOR_SLATE   0x253550U
#define SYSTEMUI_DESKTOP_COLOR_INK     0x131f33U

typedef enum systemui_theme_mode {
	SYSTEMUI_THEME_MODE_LIGHT = 0,
	SYSTEMUI_THEME_MODE_DARK = 1,
} systemui_theme_mode_e;

typedef struct systemui_palette {
	uint32_t bg;
	uint32_t panel;
	uint32_t panel_alt;
	uint32_t line;
	uint32_t text;
	uint32_t dim;
	uint32_t accent;
	uint32_t success;
	uint32_t warning;
	uint32_t overlay;
} systemui_palette_s;

typedef struct systemui_appearance {
	uint32_t theme_mode;
	uint32_t desktop_color;
} systemui_appearance_s;

typedef struct systemui_runtime_state {
	statemgr_client_s *statemgr;
	systemd_client_s *systemd;
	systemui_appearance_s appearance;
	systemui_palette_s palette;
	uint32_t idle_lock_timeout_sec;
	uint8_t net_enabled;
	uint8_t appearance_dirty;
	uint64_t last_appearance_ms;
	uint64_t last_appearance_revision;
} systemui_runtime_state_s;

typedef struct systemui_swatch_spec {
	const char *label;
	uint32_t color;
} systemui_swatch_spec_s;

typedef enum systemui_overlay_panel {
	SYSTEMUI_OVERLAY_PANEL_NOTIFICATION = 0x1,
	SYSTEMUI_OVERLAY_PANEL_CONTROL = 0x2,
	SYSTEMUI_OVERLAY_PANEL_RECENT = 0x3,
	SYSTEMUI_OVERLAY_PANEL_ALL = 0xFF,
} systemui_overlay_panel_e;

typedef enum systemui_overlay_action {
	SYSTEMUI_OVERLAY_ACTION_CLOSE = 0,
	SYSTEMUI_OVERLAY_ACTION_OPEN = 1,
	SYSTEMUI_OVERLAY_ACTION_TOGGLE = 2,
} systemui_overlay_action_e;

typedef enum ipc_systemui_overlay_service_function {
	IPC_SYSTEMUI_OVERLAY_SERVICE_FUNCTION_REQUEST = 0x1,
} ipc_systemui_overlay_service_function_t;

typedef enum ipc_systemui_lockscreen_service_function {
	IPC_SYSTEMUI_LOCKSCREEN_SERVICE_FUNCTION_REQUEST = 0x1,
} ipc_systemui_lockscreen_service_function_t;

extern const systemui_swatch_spec_s g_systemui_control_swatches[5];

void systemui_runtime_init(systemui_runtime_state_s *state);
uint8_t systemui_sync_appearance(systemui_runtime_state_s *state, uint64_t mono_ms, uint8_t force);
void systemui_update_palette(systemui_runtime_state_s *state);
uint8_t systemui_write_state_u64(const char *key, uint32_t type, uint64_t value);
uint64_t systemui_lookup_process_pid(const char *path);
const char *systemui_theme_name(uint32_t theme_mode);
const char *systemui_desktop_color_name(uint32_t color);
uint32_t systemui_is_dark_color(uint32_t color);
uint64_t systemui_request_overlay_action(uint32_t panel, uint32_t action);
uint64_t systemui_request_lockscreen_action(uint32_t action);

#endif /* __TRANQUILOS_APPS_SYSTEMUI_SHARED_H__ */
