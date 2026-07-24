#ifndef __YIYIYA_LIBSYSTEM_STATEMGR_CLIENT_H__
#define __YIYIYA_LIBSYSTEM_STATEMGR_CLIENT_H__

#include <stdint.h>

#define STATEMGR_KEY_MAX 64U
#define STATEMGR_STRING_MAX 64U
#define STATEMGR_LIST_MAX 16U

typedef enum statemgr_value_type {
	STATEMGR_VALUE_TYPE_NONE = 0,
	STATEMGR_VALUE_TYPE_BOOL = 1,
	STATEMGR_VALUE_TYPE_U32 = 2,
	STATEMGR_VALUE_TYPE_STRING = 3,
} statemgr_value_type_t;

typedef struct statemgr_entry {
	char key[STATEMGR_KEY_MAX];
	uint32_t type;
	uint32_t flags;
	uint64_t revision;
	uint64_t value_u64;
	char string_value[STATEMGR_STRING_MAX];
} statemgr_entry_s;

typedef struct statemgr_get_response {
	uint32_t found;
	uint32_t reserved;
	statemgr_entry_s entry;
} statemgr_get_response_s;

typedef struct statemgr_client statemgr_client_s;

typedef uint64_t (*statemgr_get_fn)(statemgr_client_s *client, const char *key,
				    statemgr_get_response_s *response);
typedef uint64_t (*statemgr_get_revision_fn)(statemgr_client_s *client);

typedef struct statemgr_client_ops {
	statemgr_get_fn get;
	statemgr_get_revision_fn get_revision;
} statemgr_client_ops_s;

struct statemgr_client {
	uint64_t pool_cref;
	statemgr_client_ops_s ops;
};

// YiYiYa 适配：返回 NULL，statemgr 不可用
static inline statemgr_client_s *statemgr_client_get(void) {
	return (statemgr_client_s *)0;
}

#endif /* __YIYIYA_LIBSYSTEM_STATEMGR_CLIENT_H__ */
