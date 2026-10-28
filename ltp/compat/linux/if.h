/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * linux/if.h 兼容壳：接口名长度、IFF_* 标志与 ARPHRD_* 硬件类型。
 * 结构体（ifreq/ifmap）在 libc 的 <net/if.h> 里，这里直接转发，
 * 只补 uapi 独有、libc 没给的常量。
 */
#ifndef _COMPAT_LINUX_IF_H
#define _COMPAT_LINUX_IF_H

#include <linux/types.h>
#include <net/if.h>

#ifndef IF_NAMESIZE
#define IF_NAMESIZE IFNAMSIZ
#endif

/* ARPHRD_*（/sys/class/net/<if>/type 的取值） */
#ifndef ARPHRD_ETHER
#define ARPHRD_NETROM 0
#define ARPHRD_ETHER 1
#define ARPHRD_EETHER 2
#define ARPHRD_AX25 3
#define ARPHRD_IEEE802 6
#define ARPHRD_ARCNET 7
#define ARPHRD_APPLETLK 8
#define ARPHRD_DLCI 15
#define ARPHRD_ATM 19
#define ARPHRD_IEEE1394 24
#define ARPHRD_EUI64 27
#define ARPHRD_INFINIBAND 32
#define ARPHRD_SLIP 256
#define ARPHRD_CSLIP 257
#define ARPHRD_SLIP6 258
#define ARPHRD_PPP 512
#define ARPHRD_TUNNEL 768
#define ARPHRD_LOOPBACK 772
#define ARPHRD_SIT 776
#define ARPHRD_IPGRE 778
#define ARPHRD_IP6GRE 823
#endif

#endif
