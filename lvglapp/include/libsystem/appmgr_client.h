#ifndef __YIYIYA_LIBSYSTEM_APPMGR_CLIENT_H__
#define __YIYIYA_LIBSYSTEM_APPMGR_CLIENT_H__

#include <stdint.h>

typedef struct appmgr_client appmgr_client_s;

typedef void (*appmgr_exit_app_fn)(appmgr_client_s *client);

typedef struct appmgr_client_ops {
	appmgr_exit_app_fn exit_app;
} appmgr_client_ops_s;

struct appmgr_client {
	appmgr_client_ops_s ops;
};

// YiYiYa 适配：返回 NULL
static inline appmgr_client_s *appmgr_client_get(void) {
	return (appmgr_client_s *)0;
}

#endif /* __YIYIYA_LIBSYSTEM_APPMGR_CLIENT_H__ */
