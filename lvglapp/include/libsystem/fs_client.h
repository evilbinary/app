#ifndef __TRANQUILOS_LIBSYSTEM_FILESYSTEM_CLIENT_H__
#define __TRANQUILOS_LIBSYSTEM_FILESYSTEM_CLIENT_H__

#include "stdint.h"
#include "libsystem/fs_types.h"

typedef enum ipc_fs_service_function {
	IPC_FS_SERVICE_FUNCTION_OPEN = 0x1,
	IPC_FS_SERVICE_FUNCTION_READ = 0x2,
	IPC_FS_SERVICE_FUNCTION_WRITE = 0x3,
	IPC_FS_SERVICE_FUNCTION_CLOSE = 0x4,
	IPC_FS_SERVICE_FUNCTION_LIST_DIR = 0x5,
} ipc_fs_service_function_t;

static inline const char *ipc_fs_service_function_to_string(ipc_fs_service_function_t func) {
	switch (func) {
		case IPC_FS_SERVICE_FUNCTION_OPEN:
			return "OPEN";
		case IPC_FS_SERVICE_FUNCTION_READ:
			return "READ";
		case IPC_FS_SERVICE_FUNCTION_WRITE:
			return "WRITE";
		case IPC_FS_SERVICE_FUNCTION_CLOSE:
			return "CLOSE";
		case IPC_FS_SERVICE_FUNCTION_LIST_DIR:
			return "LIST_DIR";
		default:
			return "UNKNOWN";
	}
}

typedef struct fs_client fs_client_s;

typedef uint64_t (*fs_open_fn)(fs_client_s *client, char *filepath);
typedef uint64_t (*fs_read_fn)(fs_client_s *client, uint64_t fd, uint64_t shm, uint64_t len);
typedef uint64_t (*fs_write_fn)(fs_client_s *client, uint64_t fd, uint64_t shm, uint64_t len);
typedef uint64_t (*fs_close_fn)(fs_client_s *client, uint64_t fd);
typedef uint64_t (*fs_list_dir_fn)(fs_client_s *client, const char *filepath,
				       fs_dir_list_response_s *response);
typedef struct fs_client_ops {
	fs_open_fn open;
	fs_read_fn read;
	fs_write_fn write;
	fs_close_fn close;
	fs_list_dir_fn list_dir;
} fs_client_ops_s;

struct fs_client {
	uint64_t fs_pool_cref;
	fs_client_ops_s ops;
};

fs_client_s *fs_client_get();

#endif /* __TRANQUILOS_LIBSYSTEM_FILESYSTEM_CLIENT_H__ */
