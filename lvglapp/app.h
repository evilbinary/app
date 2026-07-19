#ifndef __TRANQUILOS_LIBAPP_APP_H__
#define __TRANQUILOS_LIBAPP_APP_H__

#include "stdint.h"

// YiYiYa 适配：添加时间常量
#ifndef NSEC_PER_MSEC
#define NSEC_PER_MSEC (1000000ULL)
#endif
#ifndef NSEC_PER_SEC
#define NSEC_PER_SEC (1000000000ULL)
#endif

typedef struct app app_s;

typedef void (*app_lifecycle_cb_fn)(app_s *app);
typedef void (*app_update_cb_fn)(app_s *app, uint64_t mono_ms);

typedef struct app_config {
	const char *name;
	int32_t x;
	int32_t y;
	int32_t z;
	uint32_t width;
	uint32_t height;
	uint32_t window_flags;
	uint8_t enable_input;
} app_config_s;

typedef struct app_lifecycle_ops {
	app_lifecycle_cb_fn on_create;
	app_lifecycle_cb_fn on_start;
	app_lifecycle_cb_fn on_resume;
	app_lifecycle_cb_fn on_pause;
	app_lifecycle_cb_fn on_stop;
	app_lifecycle_cb_fn on_destroy;
	app_lifecycle_cb_fn on_foreground;
	app_lifecycle_cb_fn on_background;
	app_update_cb_fn on_update;
} app_lifecycle_ops_s;

struct app {
	app_config_s config;
	app_lifecycle_ops_s lifecycle;
	void *user_data;
	uint64_t last_tick_ns;
	int exit_status;
	uint8_t running;
	uint8_t created;
	uint8_t started;
	uint8_t resumed;
	uint8_t visible;
};

void app_init(app_s *app, const app_config_s *config,
	      const app_lifecycle_ops_s *lifecycle, void *user_data);
int app_run(app_s *app);
void app_request_exit(app_s *app, int status);

#endif /* __TRANQUILOS_LIBAPP_APP_H__ */
