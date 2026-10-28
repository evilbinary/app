/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * linux/if_tun.h 的用户态子集（uapi）：TUN/TAP 设备的 ioctl 与标志。
 */
#ifndef _UAPI_LINUX_IF_TUN_H
#define _UAPI_LINUX_IF_TUN_H

#include <linux/types.h>
#include <linux/ioctl.h>

/* 接口标志 */
#define IFF_TUN 0x0001
#define IFF_TAP 0x0002
#define IFF_NAPI 0x0010
#define IFF_NAPI_FRAGS 0x0020
#define IFF_NO_PI 0x1000
#define IFF_ONE_QUEUE 0x2000
#define IFF_VNET_HDR 0x4000
#define IFF_TUN_EXCL 0x8000
#define IFF_MULTI_QUEUE 0x0100
#define IFF_ATTACH_QUEUE 0x0200
#define IFF_DETACH_QUEUE 0x0400
#define IFF_PERSIST 0x0800
#define IFF_NOFILTER 0x1000

/* ioctl */
#define TUNSETNOCSUM _IOW('T', 200, int)
#define TUNSETDEBUG _IOW('T', 201, int)
#define TUNSETIFF _IOW('T', 202, int)
#define TUNSETPERSIST _IOW('T', 203, int)
#define TUNSETOWNER _IOW('T', 204, int)
#define TUNSETLINK _IOW('T', 205, int)
#define TUNSETGROUP _IOW('T', 206, int)
#define TUNGETFEATURES _IOR('T', 207, unsigned int)
#define TUNSETOFFLOAD _IOW('T', 208, unsigned int)
#define TUNSETTXFILTER _IOW('T', 209, unsigned int)
#define TUNGETIFF _IOR('T', 210, unsigned int)
#define TUNGETSNDBUF _IOR('T', 211, int)
#define TUNSETSNDBUF _IOW('T', 212, int)
#define TUNATTACHFILTER _IOW('T', 213, struct sock_filter)
#define TUNDETACHFILTER _IOW('T', 214, struct sock_filter)
#define TUNGETVNETHDRSZ _IOR('T', 215, int)
#define TUNSETVNETHDRSZ _IOW('T', 216, int)
#define TUNSETQUEUE _IOW('T', 217, int)
#define TUNSETSTEERINGEBPF _IOR('T', 224, int)
#define TUNSETFILTEREBPF _IOR('T', 225, int)
#define TUNSETCARRIER _IOW('T', 226, int)
#define TUNGETDEVNETNS _IO('T', 227)

/* TUNGETFEATURES 返回 */
#define TUN_F_CSUM 0x01
#define TUN_F_TSO4 0x02
#define TUN_F_TSO6 0x04
#define TUN_F_TSO_ECN 0x08
#define TUN_F_UFO 0x10
#define TUN_F_USO4 0x20
#define TUN_F_USO6 0x40

struct tun_pi {
	__u16 flags;
	__u16 proto;
};

struct tun_filter {
	__u16 flags;
	__u16 count;
	__u8 addr[0][6];
};

#endif
