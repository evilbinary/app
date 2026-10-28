/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * linux/fs.h 的最小子集（uapi）：只收录 LTP 用到的文件属性 ioctl。
 * 完整头由 Debian 的 linux-libc-dev 提供；这里按同一编码给出，架构无关。
 */
#ifndef _UAPI_LINUX_FS_H
#define _UAPI_LINUX_FS_H

#include <linux/types.h>
#include <linux/fiemap.h>

#ifndef _IOC
#include <sys/ioctl.h>
#endif

/* 文件属性（chattr/lsattr） */
#define FS_IOC_GETFLAGS _IOR('f', 1, long)
#define FS_IOC_SETFLAGS _IOW('f', 2, long)
#define FS_IOC_GETVERSION _IOR('v', 1, long)
#define FS_IOC_SETVERSION _IOW('v', 2, long)
#define FS_IOC_FIEMAP _IOWR('f', 11, struct fiemap)
#define FS_IOC_FSGETXATTR _IOR('X', 31, struct fsxattr)
#define FS_IOC_FSSETXATTR _IOW('X', 32, struct fsxattr)

/* inode 属性位 */
#define FS_SECRM_FL 0x00000001
#define FS_UNRM_FL 0x00000002
#define FS_COMPR_FL 0x00000004
#define FS_SYNC_FL 0x00000008
#define FS_IMMUTABLE_FL 0x00000010
#define FS_APPEND_FL 0x00000020
#define FS_NODUMP_FL 0x00000040
#define FS_NOATIME_FL 0x00000080
#define FS_NOCOW_FL 0x00800000
#define FS_DAX_FL 0x02000000

struct fsxattr {
	__u32 fsx_xflags;
	__u32 fsx_extsize;
	__u32 fsx_nextents;
	__u32 fsx_projid;
	__u32 fsx_cowextsize;
	unsigned char fsx_pad[8];
};

#endif
