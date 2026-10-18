/* 兼容头：musl 移植没带 linux/types.h，LTP 的 lapi 头引用它。 */
#ifndef _LINUX_TYPES_H
#define _LINUX_TYPES_H

#include <stdint.h>
#include <stddef.h>

#define __bitwise

typedef uint8_t  __u8;
typedef uint16_t __u16;
typedef uint32_t __u32;
typedef uint64_t __u64;
typedef int8_t   __s8;
typedef int16_t  __s16;
typedef int32_t  __s32;
typedef int64_t  __s64;

typedef __u16 __le16;
typedef __u32 __le32;
typedef __u64 __le64;
typedef __u16 __be16;
typedef __u32 __be32;
typedef __u64 __be64;

typedef __u16 __sum16;
typedef __u32 __wsum;

#define __aligned_u64 __u64 __attribute__((aligned(8)))
#define __aligned_be64 __be64 __attribute__((aligned(8)))
#define __aligned_le64 __le64 __attribute__((aligned(8)))

/* asm/posix_types.h 的 __kernel_* 别名（musl 的基本类型直通） */
typedef int  __kernel_pid_t;
typedef unsigned int __kernel_uid_t;
typedef unsigned int __kernel_gid_t;
typedef long  __kernel_suseconds_t;
typedef long  __kernel_clock_t;
typedef int   __kernel_daddr_t;
typedef char *__kernel_caddr_t;
typedef long  __kernel_ptrdiff_t;
typedef long  __kernel_off_t;
typedef long long __kernel_loff_t;
typedef long  __kernel_time_t;
typedef long  __kernel_ssize_t;
typedef unsigned int __kernel_size_t;
typedef unsigned long __kernel_ulong_t;
typedef long __kernel_long_t;
typedef __kernel_pid_t __kernel_old_pid_t;
typedef __kernel_uid_t __kernel_old_uid_t;
typedef __kernel_gid_t __kernel_old_gid_t;

typedef unsigned int __kernel_key_t;
typedef unsigned int __kernel_uid32_t;
typedef unsigned int __kernel_gid32_t;
typedef unsigned int __kernel_mode_t;
typedef unsigned long __kernel_ino_t;
typedef unsigned int __kernel_dev_t;
typedef unsigned int __kernel_nlink_t;
typedef long __kernel_daddr_t2;
typedef unsigned short __kernel_ipc_pid_t;
typedef long long __kernel_time64_t;
#endif
