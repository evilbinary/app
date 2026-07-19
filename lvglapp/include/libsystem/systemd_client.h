#ifndef __TRANQUILOS_LIBSYSTEM_SYSTEMD_CLIENT_H__
#define __TRANQUILOS_LIBSYSTEM_SYSTEMD_CLIENT_H__

#include "stddef.h"
#include "stdint.h"

#define START_PROCESS_PATH_MAX 64
#define START_PROCESS_LINEAR_MAPS_MAX 16
#define SYSTEMD_PROCESS_NAME_MAX 32
#define SYSTEMD_PROCESS_LIST_MAX 32

typedef struct start_process_linear_map {
	uint64_t start;
	uint64_t size;
} start_process_linear_map_s;

typedef struct start_process_request {
	char path[START_PROCESS_PATH_MAX];
	uint64_t linear_maps_cnt;
	start_process_linear_map_s linear_maps[START_PROCESS_LINEAR_MAPS_MAX];
	uint64_t affinity_mask;
} start_process_request_s;

typedef struct start_process_from_shm_request {
	char path[START_PROCESS_PATH_MAX];
	uint64_t binary_shm;
	uint64_t binary_size;
	uint64_t linear_maps_cnt;
	start_process_linear_map_s linear_maps[START_PROCESS_LINEAR_MAPS_MAX];
	uint64_t affinity_mask;
} start_process_from_shm_request_s;

typedef struct systemd_process_path_request {
	char path[START_PROCESS_PATH_MAX];
} systemd_process_path_request_s;

typedef struct systemd_process_info {
	char name[SYSTEMD_PROCESS_NAME_MAX];
	uint64_t pid;
	uint64_t thread_count;
} systemd_process_info_s;

typedef struct systemd_process_list_response {
	uint32_t count;
	uint32_t total_count;
	uint32_t truncated;
	uint32_t reserved;
	systemd_process_info_s processes[SYSTEMD_PROCESS_LIST_MAX];
} systemd_process_list_response_s;

typedef enum ipc_systemd_service_function {
	IPC_SYSTEMD_SERVICE_FUNCTION_ALLOC_DMA = 0x1,
	IPC_SYSTEMD_SERVICE_FUNCTION_ALLOC_SHM = 0x2,
	IPC_SYSTEMD_SERVICE_FUNCTION_GET_SHM = 0x3,
	IPC_SYSTEMD_SERVICE_FUNCTION_FREE_SHM = 0x4,
	IPC_SYSTEMD_SERVICE_FUNCTION_GET_MEM_TOTAL = 0x5,
	IPC_SYSTEMD_SERVICE_FUNCTION_GET_MEM_FREE = 0x6,
	IPC_SYSTEMD_SERVICE_FUNCTION_GET_PROC_COUNT = 0x7,
	IPC_SYSTEMD_SERVICE_FUNCTION_GET_THREAD_COUNT = 0x8,
	IPC_SYSTEMD_SERVICE_FUNCTION_REGISTER_UPCALL = 0x9,
	IPC_SYSTEMD_SERVICE_FUNCTION_PAGE_FAULT = 0xA,
	IPC_SYSTEMD_SERVICE_FUNCTION_EXIT_SELF = 0xB,
	IPC_SYSTEMD_SERVICE_FUNCTION_START_PROCESS = 0xC,
	IPC_SYSTEMD_SERVICE_FUNCTION_START_PROCESS_FROM_SHM = 0xD,
	IPC_SYSTEMD_SERVICE_FUNCTION_GET_PROCESS_PID_BY_PATH = 0xE,
	IPC_SYSTEMD_SERVICE_FUNCTION_GRANT_IPC_CAP = 0xF,
	IPC_SYSTEMD_SERVICE_FUNCTION_LIST_PROCESSES = 0x10,
	IPC_SYSTEMD_SERVICE_FUNCTION_EXIT_PROCESS_BY_PID = 0x11,
} ipc_systemd_service_function_t;

static inline const char *ipc_systemd_service_function_to_string(ipc_systemd_service_function_t func) {
	switch (func) {
		case IPC_SYSTEMD_SERVICE_FUNCTION_ALLOC_DMA:
			return "ALLOC_DMA";
		case IPC_SYSTEMD_SERVICE_FUNCTION_ALLOC_SHM:
			return "ALLOC_SHM";
		case IPC_SYSTEMD_SERVICE_FUNCTION_GET_SHM:
			return "GET_SHM";
		case IPC_SYSTEMD_SERVICE_FUNCTION_FREE_SHM:
			return "FREE_SHM";
		case IPC_SYSTEMD_SERVICE_FUNCTION_GET_MEM_TOTAL:
			return "GET_MEM_TOTAL";
		case IPC_SYSTEMD_SERVICE_FUNCTION_GET_MEM_FREE:
			return "GET_MEM_FREE";
		case IPC_SYSTEMD_SERVICE_FUNCTION_GET_PROC_COUNT:
			return "GET_PROC_COUNT";
		case IPC_SYSTEMD_SERVICE_FUNCTION_GET_THREAD_COUNT:
			return "GET_THREAD_COUNT";
		case IPC_SYSTEMD_SERVICE_FUNCTION_REGISTER_UPCALL:
			return "REGISTER_UPCALL";
		case IPC_SYSTEMD_SERVICE_FUNCTION_PAGE_FAULT:
			return "PAGE_FAULT";
		case IPC_SYSTEMD_SERVICE_FUNCTION_EXIT_SELF:
			return "EXIT_SELF";
		case IPC_SYSTEMD_SERVICE_FUNCTION_START_PROCESS:
			return "START_PROCESS";
		case IPC_SYSTEMD_SERVICE_FUNCTION_START_PROCESS_FROM_SHM:
			return "START_PROCESS_FROM_SHM";
		case IPC_SYSTEMD_SERVICE_FUNCTION_GET_PROCESS_PID_BY_PATH:
			return "GET_PROCESS_PID_BY_PATH";
		case IPC_SYSTEMD_SERVICE_FUNCTION_GRANT_IPC_CAP:
			return "GRANT_IPC_CAP";
		case IPC_SYSTEMD_SERVICE_FUNCTION_LIST_PROCESSES:
			return "LIST_PROCESSES";
		case IPC_SYSTEMD_SERVICE_FUNCTION_EXIT_PROCESS_BY_PID:
			return "EXIT_PROCESS_BY_PID";
		default:
			return "UNKNOWN";
	}
}

typedef struct systemd_client systemd_client_s;

typedef uint64_t (*systemd_alloc_shm_fn)(systemd_client_s *client, uint64_t size);
typedef uint64_t (*systemd_get_shm_fn)(systemd_client_s *client, uint64_t shm_id);
typedef uint64_t (*systemd_free_shm_fn)(systemd_client_s *client, uint64_t shm_id);
typedef uint64_t (*systemd_get_mem_total_fn)(systemd_client_s *client);
typedef uint64_t (*systemd_get_mem_free_fn)(systemd_client_s *client);
typedef uint64_t (*systemd_get_proc_count_fn)(systemd_client_s *client);
typedef uint64_t (*systemd_get_thread_count_fn)(systemd_client_s *client);
typedef uint64_t (*systemd_register_upcall_fn)(systemd_client_s *client, uint64_t upcall_entry);
typedef uint64_t (*systemd_page_fault_fn)(systemd_client_s *client, uint64_t vaddr);
typedef uint64_t (*systemd_process_self_exit_fn)(systemd_client_s *client, int status);
typedef uint64_t (*systemd_start_process_fn)(systemd_client_s *client, start_process_request_s *request);
typedef uint64_t (*systemd_start_process_from_shm_fn)(systemd_client_s *client,
							      start_process_from_shm_request_s *request);
typedef uint64_t (*systemd_get_process_pid_by_path_fn)(systemd_client_s *client, const char *path);
typedef uint64_t (*systemd_grant_ipc_cap_fn)(systemd_client_s *client, uint64_t target_pid,
					     uint64_t source_cref);
typedef uint64_t (*systemd_list_processes_fn)(systemd_client_s *client,
					      systemd_process_list_response_s *response);
typedef uint64_t (*systemd_exit_process_by_pid_fn)(systemd_client_s *client, uint64_t pid);
typedef struct systemd_client_ops {
	systemd_alloc_shm_fn alloc_shm;
	systemd_get_shm_fn get_shm;
	systemd_free_shm_fn free_shm;
	systemd_get_mem_total_fn get_mem_total;
	systemd_get_mem_free_fn get_mem_free;
	systemd_get_proc_count_fn get_proc_count;
	systemd_get_thread_count_fn get_thread_count;
	systemd_register_upcall_fn register_upcall;
	systemd_page_fault_fn page_fault;
	systemd_process_self_exit_fn process_self_exit;
	systemd_start_process_fn start_process;
	systemd_start_process_from_shm_fn start_process_from_shm;
	systemd_get_process_pid_by_path_fn get_process_pid_by_path;
	systemd_grant_ipc_cap_fn grant_ipc_cap;
	systemd_list_processes_fn list_processes;
	systemd_exit_process_by_pid_fn exit_process_by_pid;
} systemd_client_ops_s;

struct systemd_client {
	uint64_t systemd_pool_cref;
	systemd_client_ops_s ops;
};

systemd_client_s *systemd_client_get();

#endif /* __TRANQUILOS_LIBSYSTEM_SYSTEMD_CLIENT_H__ */
