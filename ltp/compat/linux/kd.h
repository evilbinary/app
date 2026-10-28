/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * linux/kd.h 的最小子集（uapi）：虚拟控制台（VT）键盘/显示 ioctl。
 */
#ifndef _UAPI_LINUX_KD_H
#define _UAPI_LINUX_KD_H

#include <linux/types.h>

#define GIO_FONT 0x4B60
#define PIO_FONT 0x4B61
#define GIO_FONTX 0x4B6B
#define PIO_FONTX 0x4B6C
#define PIO_FONTRESET 0x4B6D

#define GIO_CMAP 0x4B70
#define PIO_CMAP 0x4B71

#define KIOCSOUND 0x4B2F
#define KDMKTONE 0x4B30

#define KDGETLED 0x4B31
#define KDSETLED 0x4B32
#define KDGKBTYPE 0x4B33
#define KDADDIO 0x4B34
#define KDDELIO 0x4B35
#define KDENABIO 0x4B36
#define KDDISABIO 0x4B37

#define KDSETMODE 0x4B3A
#define KDGETMODE 0x4B3B

#define KDMAPDISP 0x4B3C
#define KDUNMAPDISP 0x4B3D

#define KDGKBMODE 0x4B44
#define KDSKBMODE 0x4B45
#define KDGKBMETA 0x4B62
#define KDSKBMETA 0x4B63
#define KDGKBENT 0x4B46
#define KDSKBENT 0x4B47
#define KDGKBSENT 0x4B48
#define KDSKBSENT 0x4B49
#define KDGKBDIACR 0x4B4A
#define KDSKBENT_DIACR 0x4B4B
#define KDGKBENT_DIACR 0x4B4C
#define KDGKBDIACR 0x4B4A
#define KDSKBDIACR 0x4B4B
#define KDGKBDIACRUC 0x4BFA
#define KDSKBDIACRUC 0x4BFB

#define KDGETKEYCODE 0x4B4C
#define KDSETKEYCODE 0x4B4D
#define KDSIGACCEPT 0x4B4E

#define KDKBREPEAT 0x4B50
#define KDGKBLED 0x4B64
#define KDSKBLED 0x4B65

#define KDGETMODE 0x4B3B
#define KDSETMODE 0x4B3A

/* 键盘模式 */
#define K_RAW 0x00
#define K_XLATE 0x01
#define K_MEDIUMRAW 0x02
#define K_UNICODE 0x03
#define K_OFF 0x04

/* LED */
#define LED_SCR 0x01
#define LED_NUM 0x02
#define LED_CAP 0x04

/* 显示模式 */
#define KD_TEXT 0x00
#define KD_GRAPHICS 0x01
#define KD_TEXT0 0x02
#define KD_TEXT1 0x03

/* 键盘类型 */
#define KB_84 0x01
#define KB_101 0x02
#define KB_OTHER 0x03

struct kbentry {
	unsigned char kb_table;
	unsigned char kb_index;
	__u16 kb_value;
};

struct kbsentry {
	unsigned char kb_func;
	unsigned char kb_string[512];
};

struct kbdiacr {
	unsigned char diacr;
	unsigned char base;
	unsigned char result;
};

struct kbdiacrs {
	unsigned int kb_cnt;
	struct kbdiacr kbdiacr[256];
};

struct kbdiacruc {
	unsigned int diacr;
	unsigned int base;
	unsigned int result;
};

struct kbdiacrsuc {
	unsigned int kb_cnt;
	struct kbdiacruc kbdiacruc[256];
};

struct kbkeycode {
	unsigned int scancode;
	unsigned int keycode;
};

#endif
