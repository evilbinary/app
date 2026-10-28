/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * linux/rtnetlink.h 的用户态子集（uapi）：路由 netlink 的消息类型与
 * 常用属性结构。完整头由 linux-libc-dev 提供。
 */
#ifndef _UAPI_LINUX_RTNETLINK_H
#define _UAPI_LINUX_RTNETLINK_H

#include <linux/netlink.h>
#include <linux/types.h>

struct rtattr {
	unsigned short rta_len;
	unsigned short rta_type;
};

struct rtmsg {
	unsigned char rtm_family;
	unsigned char rtm_dst_len;
	unsigned char rtm_src_len;
	unsigned char rtm_tos;
	unsigned char rtm_table;
	unsigned char rtm_protocol;
	unsigned char rtm_scope;
	unsigned char rtm_type;
	unsigned rtm_flags;
};

struct ifinfomsg {
	unsigned char ifi_family;
	unsigned char __ifi_pad;
	unsigned short ifi_type;
	int ifi_index;
	unsigned ifi_flags;
	unsigned ifi_change;
};

struct ifaddrmsg {
	__u8 ifa_family;
	__u8 ifa_prefixlen;
	__u8 ifa_flags;
	__u8 ifa_scope;
	__u32 ifa_index;
};

struct prefixmsg {
	unsigned char prefix_family;
	unsigned char prefix_pad1;
	unsigned short prefix_pad2;
	int prefix_ifindex;
	unsigned char prefix_type;
	unsigned char prefix_len;
	unsigned char prefix_flags;
	unsigned char prefix_pad3;
};

/* 消息类型 */
#define RTM_BASE 16
#define RTM_NEWLINK (RTM_BASE + 0)
#define RTM_DELLINK (RTM_BASE + 1)
#define RTM_GETLINK (RTM_BASE + 2)
#define RTM_SETLINK (RTM_BASE + 3)

#define RTM_NEWADDR (RTM_BASE + 4)
#define RTM_DELADDR (RTM_BASE + 5)
#define RTM_GETADDR (RTM_BASE + 6)

#define RTM_NEWROUTE (RTM_BASE + 8)
#define RTM_DELROUTE (RTM_BASE + 9)
#define RTM_GETROUTE (RTM_BASE + 10)

#define RTM_NEWNEIGH (RTM_BASE + 12)
#define RTM_DELNEIGH (RTM_BASE + 13)
#define RTM_GETNEIGH (RTM_BASE + 14)

#define RTM_NEWRULE (RTM_BASE + 16)
#define RTM_DELRULE (RTM_BASE + 17)
#define RTM_GETRULE (RTM_BASE + 18)

/* 属性类型（link/addr 常用部分） */
#define IFLA_UNSPEC 0
#define IFLA_ADDRESS 1
#define IFLA_BROADCAST 2
#define IFLA_IFNAME 3
#define IFLA_MTU 4
#define IFLA_LINK 5
#define IFLA_QDISC 6
#define IFLA_STATS 7
#define IFLA_MASTER 10
#define IFLA_OPERSTATE 16
#define IFLA_LINKMODE 17
#define IFLA_IFALIAS 20

#define IFA_UNSPEC 0
#define IFA_ADDRESS 1
#define IFA_LOCAL 2
#define IFA_LABEL 3
#define IFA_BROADCAST 4
#define IFA_ANYCAST 5
#define IFA_CACHEINFO 6
#define IFA_MULTICAST 7

/* scope */
#define RT_SCOPE_UNIVERSE 0
#define RT_SCOPE_SITE 200
#define RT_SCOPE_LINK 253
#define RT_SCOPE_HOST 254
#define RT_SCOPE_NOWHERE 255

#define RTA_ALIGNTO 4
#define RTA_ALIGN(len) (((len) + RTA_ALIGNTO - 1) & ~(RTA_ALIGNTO - 1))
#define RTA_LENGTH(len) (RTA_ALIGN(sizeof(struct rtattr)) + (len))
#define RTA_DATA(rta) ((void *)(((char *)(rta)) + RTA_LENGTH(0)))
#define RTA_PAYLOAD(rta) ((int)((rta)->rta_len) - RTA_LENGTH(0))
#define RTA_OK(rta, len)                                                       \
	((len) >= (int)sizeof(struct rtattr) &&                                \
	 (rta)->rta_len >= sizeof(struct rtattr) && (rta)->rta_len <= (len))
#define RTA_NEXT(rta, len)                                                     \
	((len) -= RTA_ALIGN((rta)->rta_len),                                   \
	 (struct rtattr *)(((char *)(rta)) + RTA_ALIGN((rta)->rta_len)))

#endif
