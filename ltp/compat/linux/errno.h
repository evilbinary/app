/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * linux/errno.h 兼容壳：uapi 的 EPERM/EINVAL… 与 libc 的 <errno.h> 同值，
 * 用户态直接转发。
 */
#ifndef _COMPAT_LINUX_ERRNO_H
#define _COMPAT_LINUX_ERRNO_H

#include <errno.h>

#endif
