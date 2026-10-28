/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * linux/connector.h 的用户态子集（uapi）：进程事件连接器（cn_proc）。
 */
#ifndef _UAPI_LINUX_CONNECTOR_H
#define _UAPI_LINUX_CONNECTOR_H

#include <linux/types.h>

#define CN_IDX_PROC 0x1
#define CN_VAL_PROC 0x1
#define CN_IDX_CIFS 0x2
#define CN_VAL_CIFS 0x1

enum proc_cn_mcast_op {
	PROC_CN_MCAST_LISTEN = 1,
	PROC_CN_MCAST_IGNORE = 2,
};

struct cb_id {
	__u32 idx;
	__u32 val;
};

struct cn_msg {
	struct cb_id id;
	__u32 seq;
	__u32 ack;
	__u16 len;
	__u16 flags;
	__u8 data[0];
};

#endif
