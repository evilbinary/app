#include "systemui_shared.h"

#include "string.h"

const systemui_swatch_spec_s g_systemui_control_swatches[5] = {
	{ "Sky", SYSTEMUI_DESKTOP_COLOR_SKY },
	{ "Mint", SYSTEMUI_DESKTOP_COLOR_MINT },
	{ "Peach", SYSTEMUI_DESKTOP_COLOR_SAND },
	{ "Slate", SYSTEMUI_DESKTOP_COLOR_SLATE },
	{ "Ink", SYSTEMUI_DESKTOP_COLOR_INK },
};

static uint32_t systemui_normalize_idle_lock_timeout_sec(uint64_t value)
{
	switch ((uint32_t)value) {
	case SYSTEMUI_IDLE_LOCK_TIMEOUT_DEFAULT_SEC:
	case SYSTEMUI_IDLE_LOCK_TIMEOUT_MEDIUM_SEC:
	case SYSTEMUI_IDLE_LOCK_TIMEOUT_LONG_SEC:
		return (uint32_t)value;
	default:
		return SYSTEMUI_IDLE_LOCK_TIMEOUT_DEFAULT_SEC;
	}
}

static uint8_t systemui_read_state_u64(systemui_runtime_state_s *state,
				       const char *key,
				       uint32_t expected_type,
				       uint64_t *value_out)
{
	statemgr_get_response_s response = {0};

	if (state == NULL || key == NULL || value_out == NULL) {
		return 0U;
	}
	if (state->statemgr == NULL) {
		state->statemgr = statemgr_client_get();
	}
	if (state->statemgr == NULL || state->statemgr->ops.get == NULL) {
		return 0U;
	}
	if (state->statemgr->ops.get(state->statemgr, key, &response) == 0U || !response.found) {
		return 0U;
	}
	if (response.entry.type != expected_type) {
		return 0U;
	}

	*value_out = response.entry.value_u64;
	return 1U;
}

void systemui_runtime_init(systemui_runtime_state_s *state)
{
	if (state == NULL) {
		return;
	}

	memset(state, 0, sizeof(*state));
	state->appearance.theme_mode = SYSTEMUI_THEME_MODE_LIGHT;
	state->appearance.desktop_color = SYSTEMUI_DESKTOP_COLOR_SAND;
	state->idle_lock_timeout_sec = SYSTEMUI_IDLE_LOCK_TIMEOUT_DEFAULT_SEC;
	state->net_enabled = 1U;
	systemui_update_palette(state);
}

void systemui_update_palette(systemui_runtime_state_s *state)
{
	if (state == NULL) {
		return;
	}

	if (state->appearance.theme_mode == SYSTEMUI_THEME_MODE_DARK) {
		state->palette.bg = SYSTEMUI_COLOR_BG_DARK;
		state->palette.panel = SYSTEMUI_COLOR_PANEL_DARK;
		state->palette.panel_alt = SYSTEMUI_COLOR_PANEL_ALT_DARK;
		state->palette.line = SYSTEMUI_COLOR_LINE_DARK;
		state->palette.text = SYSTEMUI_COLOR_TEXT_DARK;
		state->palette.dim = SYSTEMUI_COLOR_DIM_DARK;
		state->palette.accent = SYSTEMUI_COLOR_ACCENT_DARK;
		state->palette.success = SYSTEMUI_COLOR_SUCCESS_DARK;
		state->palette.warning = SYSTEMUI_COLOR_WARNING_DARK;
		state->palette.overlay = SYSTEMUI_COLOR_OVERLAY_DARK;
		return;
	}

	state->palette.bg = SYSTEMUI_COLOR_BG_LIGHT;
	state->palette.panel = SYSTEMUI_COLOR_PANEL_LIGHT;
	state->palette.panel_alt = SYSTEMUI_COLOR_PANEL_ALT_LIGHT;
	state->palette.line = SYSTEMUI_COLOR_LINE_LIGHT;
	state->palette.text = SYSTEMUI_COLOR_TEXT_LIGHT;
	state->palette.dim = SYSTEMUI_COLOR_DIM_LIGHT;
	state->palette.accent = SYSTEMUI_COLOR_ACCENT_LIGHT;
	state->palette.success = SYSTEMUI_COLOR_SUCCESS_LIGHT;
	state->palette.warning = SYSTEMUI_COLOR_WARNING_LIGHT;
	state->palette.overlay = SYSTEMUI_COLOR_OVERLAY_LIGHT;
}

uint8_t systemui_sync_appearance(systemui_runtime_state_s *state, uint64_t mono_ms, uint8_t force)
{
	uint64_t revision = 0;
	uint64_t value = 0;
	uint32_t theme_mode = 0;
	uint32_t desktop_color = 0;
	uint32_t idle_lock_timeout_sec = 0;
	uint8_t net_enabled = 0;
	uint8_t changed = 0U;

	if (state == NULL) {
		return 0U;
	}
	if (!force && mono_ms < state->last_appearance_ms + SYSTEMUI_APPEARANCE_REFRESH_INTERVAL_MS) {
		return 0U;
	}

	state->last_appearance_ms = mono_ms;
	if (state->statemgr == NULL) {
		state->statemgr = statemgr_client_get();
	}
	if (state->statemgr == NULL || state->statemgr->ops.get_revision == NULL) {
		return 0U;
	}

	revision = state->statemgr->ops.get_revision(state->statemgr);
	if (revision == 0U || (!force && revision == state->last_appearance_revision)) {
		return 0U;
	}

	theme_mode = state->appearance.theme_mode;
	desktop_color = state->appearance.desktop_color;
	idle_lock_timeout_sec = state->idle_lock_timeout_sec;
	net_enabled = state->net_enabled;

	if (systemui_read_state_u64(state, "ui.theme.mode", STATEMGR_VALUE_TYPE_U32, &value)) {
		theme_mode = value == SYSTEMUI_THEME_MODE_DARK ?
			     SYSTEMUI_THEME_MODE_DARK : SYSTEMUI_THEME_MODE_LIGHT;
	}
	if (systemui_read_state_u64(state, "ui.desktop.color", STATEMGR_VALUE_TYPE_U32, &value)) {
		desktop_color = (uint32_t)value;
	}
	if (systemui_read_state_u64(state, SYSTEMUI_STATE_KEY_IDLE_LOCK_TIMEOUT_SEC,
				    STATEMGR_VALUE_TYPE_U32, &value)) {
		idle_lock_timeout_sec = systemui_normalize_idle_lock_timeout_sec(value);
	}
	if (systemui_read_state_u64(state, "net.enabled", STATEMGR_VALUE_TYPE_BOOL, &value)) {
		net_enabled = value != 0U ? 1U : 0U;
	}

	state->last_appearance_revision = revision;
	if (theme_mode != state->appearance.theme_mode) {
		state->appearance.theme_mode = theme_mode;
		changed = 1U;
	}
	if (desktop_color != state->appearance.desktop_color) {
		state->appearance.desktop_color = desktop_color;
		changed = 1U;
	}
	if (idle_lock_timeout_sec != state->idle_lock_timeout_sec) {
		state->idle_lock_timeout_sec = idle_lock_timeout_sec;
	}
	if (net_enabled != state->net_enabled) {
		state->net_enabled = net_enabled;
		changed = 1U;
	}

	if (changed) {
		state->appearance_dirty = 1U;
		systemui_update_palette(state);
	}

	return changed;
}

uint8_t systemui_write_state_u64(const char *key, uint32_t type, uint64_t value)
{
	statemgr_entry_s entry = {0};
	statemgr_client_s *statemgr = NULL;

	if (key == NULL || key[0] == '\0') {
		return 0U;
	}

	statemgr = statemgr_client_get();
	if (statemgr == NULL || statemgr->ops.set == NULL) {
		return 0U;
	}

	strncpy(entry.key, key, STATEMGR_KEY_MAX - 1U);
	entry.type = type;
	entry.value_u64 = value;
	return statemgr->ops.set(statemgr, &entry) != 0U ? 1U : 0U;
}

uint64_t systemui_lookup_process_pid(const char *path)
{
	systemd_client_s *systemd = NULL;

	if (path == NULL || path[0] == '\0') {
		return 0;
	}

	systemd = systemd_client_get();
	if (systemd == NULL || systemd->ops.get_process_pid_by_path == NULL) {
		return 0;
	}

	return systemd->ops.get_process_pid_by_path(systemd, path);
}

const char *systemui_theme_name(uint32_t theme_mode)
{
	return theme_mode == SYSTEMUI_THEME_MODE_DARK ? "Dark" : "Light";
}

const char *systemui_desktop_color_name(uint32_t color)
{
	for (uint32_t i = 0; i < 5U; i++) {
		if (g_systemui_control_swatches[i].color == color) {
			return g_systemui_control_swatches[i].label;
		}
	}
	return "Custom";
}

uint32_t systemui_is_dark_color(uint32_t color)
{
	uint32_t r = (color >> 16) & 0xFFU;
	uint32_t g = (color >> 8) & 0xFFU;
	uint32_t b = color & 0xFFU;
	uint32_t luma = (r * 299U) + (g * 587U) + (b * 114U);

	return luma < 140000U;
}

uint64_t systemui_request_overlay_action(uint32_t panel, uint32_t action)
{
	static uint64_t overlay_pool = 0;

	if (overlay_pool == 0) {
		overlay_pool = sys_get_service_pool(IPC_SYSTEMUI_OVERLAY_SERVICE_ID);
	}
	if (overlay_pool == 0) {
		return 0;
	}

	return OSIpcEndPointPoolCall4(overlay_pool,
				      IPC_SYSTEMUI_OVERLAY_SERVICE_FUNCTION_REQUEST,
				      panel, action);
}

uint64_t systemui_request_lockscreen_action(uint32_t action)
{
	static uint64_t lockscreen_pool = 0;

	if (lockscreen_pool == 0) {
		lockscreen_pool = sys_get_service_pool(IPC_SYSTEMUI_LOCKSCREEN_SERVICE_ID);
	}
	if (lockscreen_pool == 0) {
		return 0;
	}

	return OSIpcEndPointPoolCall3(lockscreen_pool,
				      IPC_SYSTEMUI_LOCKSCREEN_SERVICE_FUNCTION_REQUEST,
				      action);
}
