/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * linux/sched/types.h 的最小子集（uapi）：
 * sched_setattr/sched_getattr 用的 struct sched_attr 及策略常量。
 */
#ifndef _UAPI_LINUX_SCHED_TYPES_H
#define _UAPI_LINUX_SCHED_TYPES_H

#include <linux/types.h>

#if defined(__aarch64__) || defined(__arm__)
/* 与内核 uapi 保持一致：全架构同一布局 */
#endif

struct sched_attr {
	__u32 size;
	__u32 sched_policy;
	__u64 sched_flags;
	/* SCHED_NORMAL, SCHED_BATCH */
	__s32 sched_nice;
	/* SCHED_FIFO, SCHED_RR */
	__u32 sched_priority;
	/* SCHED_DEADLINE (nsec) */
	__u64 sched_runtime;
	__u64 sched_deadline;
	__u64 sched_period;
	/* Utilization hints */
	__u32 sched_util_min;
	__u32 sched_util_max;
};

#ifndef SCHED_NORMAL
#define SCHED_NORMAL 0
#define SCHED_FIFO 1
#define SCHED_RR 2
#define SCHED_BATCH 3
#define SCHED_IDLE 5
#define SCHED_DEADLINE 6
#endif

/* sched_setattr 的 flags */
#define SCHED_FLAG_RESET_ON_FORK 0x01
#define SCHED_FLAG_RECLAIM 0x02
#define SCHED_FLAG_DL_OVERRUN 0x04
#define SCHED_FLAG_KEEP_POLICY 0x08
#define SCHED_FLAG_KEEP_PARAMS 0x10
#define SCHED_FLAG_UTIL_CLAMP_MIN 0x20
#define SCHED_FLAG_UTIL_CLAMP_MAX 0x40

#endif
