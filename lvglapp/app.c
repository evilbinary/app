#include "app.h"

#include "stddef.h"
#include "string.h"
#include "libwindow/window.h"
#include "liblvgl/lv_port_disp.h"
#include "liblvgl/lv_port_indev.h"
#include "libsystem/appmgr_client.h"
#include "libsyscall/syscall.h"
#include "screen.h"
#include "xwin_user.h"

#define APP_MAX_FRAME_DELTA_NS (250ULL * NSEC_PER_MSEC)
#define APP_VSYNC_IDLE_SLEEP_NS (1ULL * NSEC_PER_MSEC)

static inline void app_invoke(app_lifecycle_cb_fn callback, app_s *app)
{
	if (callback != NULL) {
		callback(app);
	}
}

static void app_notify_appmgr_exit(void)
{
	appmgr_client_s *appmgr = appmgr_client_get();

	if (appmgr == NULL || appmgr->ops.exit_app == NULL) {
		return;
	}

	(void)appmgr->ops.exit_app(appmgr);
}

static void app_enter_foreground(app_s *app)
{
	if (app == NULL) {
		return;
	}
	if (!app->visible) {
		app->visible = 1;
		app_invoke(app->lifecycle.on_foreground, app);
	}
	if (!app->resumed) {
		app->resumed = 1;
		app_invoke(app->lifecycle.on_resume, app);
	}
}

static void app_enter_background(app_s *app)
{
	if (app == NULL || !app->visible) {
		return;
	}
	if (app->resumed) {
		app->resumed = 0;
		app_invoke(app->lifecycle.on_pause, app);
	}
	app->visible = 0;
	app_invoke(app->lifecycle.on_background, app);
}

static void app_sync_visibility(app_s *app)
{
	uint8_t visible = 0;

	if (app == NULL) {
		return;
	}

	visible = (uint8_t)lv_port_disp_is_visible();
	if (visible && !app->visible) {
		app_enter_foreground(app);
	} else if (!visible && app->visible) {
		app_enter_background(app);
	}
}

void app_init(app_s *app, const app_config_s *config,
	      const app_lifecycle_ops_s *lifecycle, void *user_data)
{
	if (app == NULL) {
		return;
	}

	memset(app, 0, sizeof(*app));
	if (config != NULL) {
		app->config = *config;
	}
	if (lifecycle != NULL) {
		app->lifecycle = *lifecycle;
	}
	app->user_data = user_data;
	app->running = 1;
}

void app_request_exit(app_s *app, int status)
{
	if (app == NULL) {
		return;
	}

	app->exit_status = status;
	app->running = 0;
}

int app_run(app_s *app)
{
	window_s *window = NULL;

	if (app == NULL) {
		return 1;
	}

	libsyscall_init();
	lv_init();

	// 在初始化前设置窗口尺寸
	if (app->config.width > 0 || app->config.height > 0) {
		screen_info_t* si = screen_info();
		if (app->config.width > 0) si->width = app->config.width;
		if (app->config.height > 0) si->height = app->config.height;
	}

	lv_port_disp_init();
	if (app->config.enable_input) {
		lv_port_indev_init();
	}

	window = lv_port_disp_get_window();
	if (window == NULL) {
		app->running = 0;
		app->exit_status = 1;
		return app->exit_status;
	}

	// 设置窗口属性
	window->x = app->config.x;
	window->y = app->config.y;
	window->z = app->config.z;
	window->flags = app->config.window_flags;

	if (window == NULL) {
		app->running = 0;
		app->exit_status = 1;
		return app->exit_status;
	}

	app_invoke(app->lifecycle.on_create, app);
	app->created = 1;
	app_invoke(app->lifecycle.on_start, app);
	app->started = 1;

	app->last_tick_ns = OSSysCtrlGetMonoTime();
	app->visible = 0;
	app->resumed = 0;
	if (lv_port_disp_is_visible()) {
		app_enter_foreground(app);
	} else {
		// YiYiYa 适配：强制设置可见
		app->visible = 1;
		app->resumed = 1;
	}

	while (app->running) {
		uint64_t frame_start_ns = 0;
		uint64_t delta_ns = 0;
		uint32_t delta_ms = 0;

		lv_port_disp_sync();
		app_sync_visibility(app);
		if (!window_consume_vsync(window)) {
			OSSelfNanoSleep(APP_VSYNC_IDLE_SLEEP_NS);
			continue;
		}
		frame_start_ns = OSSysCtrlGetMonoTime();
		if (!app->visible) {
			continue;
		}

		delta_ns = frame_start_ns - app->last_tick_ns;
		if (delta_ns > APP_MAX_FRAME_DELTA_NS) {
			delta_ns = APP_MAX_FRAME_DELTA_NS;
		}

		delta_ms = (uint32_t)(delta_ns / NSEC_PER_MSEC);
		if (delta_ms == 0U) {
			delta_ms = 1U;
		}
		app->last_tick_ns = frame_start_ns;

		lv_tick_inc(delta_ms);

		if (app->config.enable_input) {
			lv_port_indev_poll();
		}
		if (app->lifecycle.on_update != NULL) {
			app->lifecycle.on_update(app, frame_start_ns / NSEC_PER_MSEC);
		}
		lv_timer_handler();
		lv_port_disp_submit();
	}

	if (app->visible) {
		app_enter_background(app);
	}
	if (app->started) {
		app->started = 0;
		app_invoke(app->lifecycle.on_stop, app);
	}
	if (app->created) {
		app->created = 0;
		app_invoke(app->lifecycle.on_destroy, app);
	}
	if (window != NULL) {
		(void)window_destroy(window);
	}

	app_notify_appmgr_exit();
	return app->exit_status;
}
