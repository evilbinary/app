/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * linux/fiemap.h 的最小子集（uapi）：struct fiemap 与 extent 布局。
 * musl 不带 linux/ 头，Debian 环境由 linux-libc-dev 提供。
 */
#ifndef _UAPI_LINUX_FIEMAP_H
#define _UAPI_LINUX_FIEMAP_H

#include <linux/types.h>

struct fiemap_extent {
	__u64 fe_logical;  /* logical offset in bytes for the start of the extent */
	__u64 fe_physical; /* physical offset in bytes for the start of the extent */
	__u64 fe_length;   /* length in bytes for this extent */
	__u64 fe_reserved64[2];
	__u32 fe_flags;    /* FIEMAP_EXTENT_* flags for this extent */
	__u32 fe_reserved[3];
};

struct fiemap {
	__u64 fm_start;          /* logical offset (inclusive) at which to start mapping */
	__u64 fm_length;         /* logical length of mapping which userspace wants */
	__u32 fm_flags;          /* FIEMAP_FLAG_* flags for request (in/out) */
	__u32 fm_mapped_extents; /* number of extents that were mapped (out) */
	__u32 fm_extent_count;   /* size of fm_extents array (in) */
	__u32 fm_reserved;
	struct fiemap_extent fm_extents[0]; /* array of mapped extents (out) */
};

#define FIEMAP_MAX_OFFSET (~0ULL)

#define FIEMAP_FLAG_SYNC 0x00000001
#define FIEMAP_FLAG_XATTR 0x00000002
#define FIEMAP_FLAG_CACHE 0x00000004

#define FIEMAP_EXTENT_LAST 0x00000001
#define FIEMAP_EXTENT_UNKNOWN 0x00000002
#define FIEMAP_EXTENT_DELALLOC 0x00000004
#define FIEMAP_EXTENT_SHARED 0x00002000

#endif
