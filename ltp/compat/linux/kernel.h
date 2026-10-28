/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * linux/kernel.h 兼容壳。
 * 这是内核内部头，不在 uapi 里；有测试顺手 include 它只为了 ARRAY_SIZE 之类
 * 的辅助宏，这里给出用户态可用的等价定义。
 */
#ifndef _COMPAT_LINUX_KERNEL_H
#define _COMPAT_LINUX_KERNEL_H

#include <linux/types.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef clamp
#define clamp(v, lo, hi) min(max(v, lo), hi)
#endif

#endif
