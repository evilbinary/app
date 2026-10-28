/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * linux/unistd.h 兼容壳：老代码用它拿系统调用号，用户态等价物是
 * <sys/syscall.h>（musl 提供 __NR_*/SYS_*）。
 */
#ifndef _COMPAT_LINUX_UNISTD_H
#define _COMPAT_LINUX_UNISTD_H

#include <sys/syscall.h>
#include <unistd.h>

#ifndef __NR_syscall_max
#define __NR_syscall_max 0xffff
#endif

#endif
