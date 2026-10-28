/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * linux/cn_proc.h 的用户态子集（uapi）：进程事件连接器的消息结构。
 * 布局与内核 uapi 一致（成员名也照抄，测试按名字访问）。
 */
#ifndef _UAPI_LINUX_CN_PROC_H
#define _UAPI_LINUX_CN_PROC_H

#include <linux/types.h>

#define PROC_EVENT_NONE 0x00000000
#define PROC_EVENT_FORK 0x00000001
#define PROC_EVENT_EXEC 0x00000002
#define PROC_EVENT_UID 0x00000004
#define PROC_EVENT_GID 0x00000040
#define PROC_EVENT_SID 0x00000080
#define PROC_EVENT_PTRACE 0x00000100
#define PROC_EVENT_COMM 0x00000200
#define PROC_EVENT_COREDUMP 0x40000000
#define PROC_EVENT_EXIT 0x80000000

struct proc_event {
	enum what {
		PROC_EVENT_NONE_EV = 0x00000000,
		PROC_EVENT_FORK_EV = 0x00000001,
		PROC_EVENT_EXEC_EV = 0x00000002,
		PROC_EVENT_UID_EV = 0x00000004,
		PROC_EVENT_GID_EV = 0x00000040,
		PROC_EVENT_SID_EV = 0x00000080,
		PROC_EVENT_PTRACE_EV = 0x00000100,
		PROC_EVENT_COMM_EV = 0x00000200,
		PROC_EVENT_COREDUMP_EV = 0x40000000,
		PROC_EVENT_EXIT_EV = 0x80000000,
	} what;
	__u32 cpu;
	__u64 timestamp_ns;
	union {
		struct {
			__u32 err;
		} ack;
		struct {
			__u32 parent_pid;
			__u32 child_pid;
		} fork;
		struct {
			__u32 process_pid;
			__u32 process_tgid;
		} exec, sid, ptrace;
		struct {
			__u32 process_pid;
			__u32 process_tgid;
			/* 内核把有效/真实 id 分成两个子结构，测试按 id.e.euid /
			 * id.r.ruid 访问，成员名不能改。 */
			struct {
				__u32 euid;
				__u32 egid;
			} e;
			struct {
				__u32 ruid;
				__u32 rgid;
			} r;
		} id;
		struct {
			__u32 process_pid;
			__u32 process_tgid;
			__u32 exit_code;
			__u32 exit_signal;
		} exit;
		struct {
			__u32 process_pid;
			__u32 process_tgid;
			__u32 comm[16];
		} comm;
		struct {
			__u32 process_pid;
			__u32 process_tgid;
		} coredump;
	} event_data;
};

#endif
