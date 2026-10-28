/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * linux/loop.h 的用户态子集（uapi）：回环设备的 ioctl 与状态结构。
 */
#ifndef _UAPI_LINUX_LOOP_H
#define _UAPI_LINUX_LOOP_H

#include <linux/types.h>
#include <linux/ioctl.h>

#define LO_NAME_SIZE 64
#define LO_KEY_SIZE 32

struct loop_info {
	int lo_number;
	__u32 lo_device;
	unsigned long lo_inode;
	__u32 lo_rdevice;
	int lo_offset;
	int lo_encrypt_type;
	int lo_encrypt_key_size;
	int lo_flags;
	unsigned char lo_file_name[LO_NAME_SIZE];
	unsigned char lo_crypt_name[LO_NAME_SIZE];
	unsigned char lo_encrypt_key[LO_KEY_SIZE];
	unsigned long lo_init[2];
};

struct loop_info64 {
	__u64 lo_device;
	__u64 lo_inode;
	__u64 lo_rdevice;
	__u64 lo_offset;
	__u64 lo_sizelimit;
	__u32 lo_number;
	__u32 lo_encrypt_type;
	__u32 lo_encrypt_key_size;
	__u32 lo_flags;
	__u8 lo_file_name[LO_NAME_SIZE];
	__u8 lo_crypt_name[LO_NAME_SIZE];
	__u8 lo_encrypt_key[LO_KEY_SIZE];
	__u64 lo_init[2];
};

struct loop_config {
	__u32 fd;
	__u32 block_size;
	struct loop_info64 info;
	__u64 __reserved[8];
};

#define LO_FLAGS_READ_ONLY 1
#define LO_FLAGS_AUTOCLEAR 4
#define LO_FLAGS_PARTSCAN 8
#define LO_FLAGS_DIRECT_IO 16

#define LOOP_SET_FD 0x4C00
#define LOOP_CLR_FD 0x4C01
#define LOOP_SET_STATUS 0x4C02
#define LOOP_GET_STATUS 0x4C03
#define LOOP_SET_STATUS64 0x4C04
#define LOOP_GET_STATUS64 0x4C05
#define LOOP_CHANGE_FD 0x4C06
#define LOOP_SET_CAPACITY 0x4C07
#define LOOP_SET_DIRECT_IO 0x4C08
#define LOOP_SET_BLOCK_SIZE 0x4C09
#define LOOP_CONFIGURE 0x4C0A

#define LOOP_CTL_ADD 0x4C80
#define LOOP_CTL_REMOVE 0x4C81
#define LOOP_CTL_GET_FREE 0x4C82

#endif
