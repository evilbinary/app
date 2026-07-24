#ifndef __TRANQUILOS_LIBSYSTEM_FILESYSTEM_TYPES_H__
#define __TRANQUILOS_LIBSYSTEM_FILESYSTEM_TYPES_H__

#include "stddef.h"
#include "stdint.h"

#define FS_PATH_MAX (192U)
#define FS_DIR_ENTRY_NAME_MAX (48U)
#define FS_DIR_LIST_MAX (48U)

typedef enum fs_dir_entry_flag {
	FS_DIR_ENTRY_FLAG_DIR = 0x1,
	FS_DIR_ENTRY_FLAG_MOUNT = 0x2,
} fs_dir_entry_flag_t;

typedef struct fs_dir_entry {
	char name[FS_DIR_ENTRY_NAME_MAX];
	uint32_t flags;
	uint64_t size;
} fs_dir_entry_s;

typedef struct fs_dir_list_response {
	char path[FS_PATH_MAX];
	uint32_t entry_count;
	uint32_t total_count;
	uint32_t truncated;
	fs_dir_entry_s entries[FS_DIR_LIST_MAX];
} fs_dir_list_response_s;

static inline uint32_t fs_dir_entry_is_dir(const fs_dir_entry_s *entry) {
	return entry != NULL && (entry->flags & FS_DIR_ENTRY_FLAG_DIR) != 0U;
}

static inline uint32_t fs_dir_entry_is_mount(const fs_dir_entry_s *entry) {
	return entry != NULL && (entry->flags & FS_DIR_ENTRY_FLAG_MOUNT) != 0U;
}

#endif /* __TRANQUILOS_LIBSYSTEM_FILESYSTEM_TYPES_H__ */
