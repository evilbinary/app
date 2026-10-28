/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * linux/netlink.h 的用户态子集（uapi）。musl 不带 linux/ 头，
 * Debian 环境由 linux-libc-dev 提供。
 */
#ifndef _UAPI_LINUX_NETLINK_H
#define _UAPI_LINUX_NETLINK_H

#include <linux/types.h>
#include <sys/socket.h>

/* AF_NETLINK 由 libc 的 <sys/socket.h> 定义（musl: 16） */

struct sockaddr_nl {
	unsigned short nl_family; /* AF_NETLINK */
	unsigned short nl_pad;    /* zero */
	__u32 nl_pid;             /* port ID */
	__u32 nl_groups;          /* multicast groups mask */
};

struct nlmsghdr {
	__u32 nlmsg_len;
	__u16 nlmsg_type;
	__u16 nlmsg_flags;
	__u32 nlmsg_seq;
	__u32 nlmsg_pid;
};

struct nlmsgerr {
	int error;
	struct nlmsghdr msg;
};

struct nlattr {
	__u16 nla_len;
	__u16 nla_type;
};

struct nl_pktinfo {
	__u32 group;
};

struct nl_mmap_req {
	unsigned int nm_block_size;
	unsigned int nm_block_nr;
	unsigned int nm_frame_size;
	unsigned int nm_frame_nr;
};

/* netlink 协议族 */
#define NETLINK_ROUTE 0
#define NETLINK_UNUSED 1
#define NETLINK_USERSOCK 2
#define NETLINK_FIREWALL 3
#define NETLINK_SOCK_DIAG 4
#define NETLINK_NFLOG 5
#define NETLINK_XFRM 6
#define NETLINK_SELINUX 7
#define NETLINK_ISCSI 8
#define NETLINK_AUDIT 9
#define NETLINK_FIB_LOOKUP 10
#define NETLINK_CONNECTOR 11
#define NETLINK_NETFILTER 12
#define NETLINK_IP6_FW 13
#define NETLINK_DNRTMSG 14
#define NETLINK_KOBJECT_UEVENT 15
#define NETLINK_GENERIC 16
#define NETLINK_SCSITRANSPORT 18
#define NETLINK_ECRYPTFS 19
#define NETLINK_RDMA 20
#define NETLINK_CRYPTO 21
#define NETLINK_SMC 22

/* 消息类型 */
#define NLMSG_NOOP 0x1
#define NLMSG_ERROR 0x2
#define NLMSG_DONE 0x3
#define NLMSG_OVERRUN 0x4
#define NLMSG_MIN_TYPE 0x10

/* 消息标志 */
#define NLM_F_REQUEST 0x01
#define NLM_F_MULTI 0x02
#define NLM_F_ACK 0x04
#define NLM_F_ECHO 0x08
#define NLM_F_DUMP_INTR 0x10
#define NLM_F_DUMP_FILTERED 0x20

#define NLM_F_ROOT 0x100
#define NLM_F_MATCH 0x200
#define NLM_F_ATOMIC 0x400
#define NLM_F_DUMP (NLM_F_ROOT | NLM_F_MATCH)

#define NLM_F_REPLACE 0x100
#define NLM_F_EXCL 0x200
#define NLM_F_CREATE 0x400
#define NLM_F_APPEND 0x800

#define NLMSG_ALIGNTO 4U
#define NLMSG_ALIGN(len) (((len) + NLMSG_ALIGNTO - 1) & ~(NLMSG_ALIGNTO - 1))
#define NLMSG_HDRLEN ((int)NLMSG_ALIGN(sizeof(struct nlmsghdr)))
#define NLMSG_LENGTH(len) ((len) + NLMSG_HDRLEN)
#define NLMSG_SPACE(len) NLMSG_ALIGN(NLMSG_LENGTH(len))
#define NLMSG_DATA(nlh) ((void *)(((char *)(nlh)) + NLMSG_HDRLEN))
#define NLMSG_NEXT(nlh, len)                                                   \
	((len) -= NLMSG_ALIGN((nlh)->nlmsg_len),                              \
	 (struct nlmsghdr *)(((char *)(nlh)) + NLMSG_ALIGN((nlh)->nlmsg_len)))
#define NLMSG_OK(nlh, len)                                                     \
	((len) >= (int)sizeof(struct nlmsghdr) &&                              \
	 (nlh)->nlmsg_len >= sizeof(struct nlmsghdr) && (nlh)->nlmsg_len <= (len))
#define NLMSG_PAYLOAD(nlh, len) ((nlh)->nlmsg_len - NLMSG_SPACE((len)))

#define NLA_ALIGNTO 4
#define NLA_ALIGN(len) (((len) + NLA_ALIGNTO - 1) & ~(NLA_ALIGNTO - 1))
#define NLA_HDRLEN ((int)NLA_ALIGN(sizeof(struct nlattr)))
#define NLA_DATA(nla) ((void *)(((char *)(nla)) + NLA_HDRLEN))
#define NLA_NEXT(nla, attrlen)                                                 \
	((attrlen) -= NLA_ALIGN((nla)->nla_len),                              \
	 (struct nlattr *)(((char *)(nla)) + NLA_ALIGN((nla)->nla_len)))
#define NLA_OK(nla, len)                                                       \
	((len) >= (int)sizeof(struct nlattr) &&                                \
	 (nla)->nla_len >= sizeof(struct nlattr) &&                            \
	 (nla)->nla_len <= (len))

/* setsockopt 等级/选项 */
#define SOL_NETLINK 270
#define NETLINK_ADD_MEMBERSHIP 1
#define NETLINK_DROP_MEMBERSHIP 2
#define NETLINK_PKTINFO 3
#define NETLINK_BROADCAST_ERROR 4
#define NETLINK_NO_ENOBUFS 5
#define NETLINK_RX_RING 6
#define NETLINK_TX_RING 7
#define NETLINK_LISTEN_ALL_NSID 8
#define NETLINK_LIST_MEMBERSHIPS 9
#define NETLINK_CAP_ACK 10
#define NETLINK_EXT_ACK 11

#endif
