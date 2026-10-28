/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * linux/rtc.h 的最小子集（uapi）：struct rtc_time 与常用 ioctl。
 */
#ifndef _UAPI_LINUX_RTC_H
#define _UAPI_LINUX_RTC_H

#include <linux/ioctl.h>
#include <linux/types.h>

struct rtc_time {
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	int tm_year;
	int tm_wday;
	int tm_yday;
	int tm_isdst;
};

struct rtc_wkalrm {
	unsigned char enabled;
	unsigned char pending;
	struct rtc_time time;
};

struct rtc_param {
	__u64 param;
	union {
		__u64 uvalue;
		__s64 svalue;
		__u64 ptr;
	};
	__u32 index;
	__u32 flags;
};

#define RTC_AIE_ON _IO('p', 0x01)
#define RTC_AIE_OFF _IO('p', 0x02)
#define RTC_UIE_ON _IO('p', 0x03)
#define RTC_UIE_OFF _IO('p', 0x04)
#define RTC_PIE_ON _IO('p', 0x05)
#define RTC_PIE_OFF _IO('p', 0x06)
#define RTC_WIE_ON _IO('p', 0x0f)
#define RTC_WIE_OFF _IO('p', 0x10)

#define RTC_ALM_SET _IOW('p', 0x07, struct rtc_time)
#define RTC_ALM_READ _IOR('p', 0x08, struct rtc_time)
#define RTC_RD_TIME _IOR('p', 0x09, struct rtc_time)
#define RTC_SET_TIME _IOW('p', 0x0a, struct rtc_time)
#define RTC_IRQP_READ _IOR('p', 0x0b, unsigned long)
#define RTC_IRQP_SET _IOW('p', 0x0c, unsigned long)
#define RTC_EPOCH_READ _IOR('p', 0x0d, unsigned long)
#define RTC_EPOCH_SET _IOW('p', 0x0e, unsigned long)

#define RTC_WKALM_SET _IOW('p', 0x0f, struct rtc_wkalrm)
#define RTC_WKALM_RD _IOR('p', 0x10, struct rtc_wkalrm)

#define RTC_PARAM_GET _IOW('p', 0x13, struct rtc_param)
#define RTC_PARAM_SET _IOW('p', 0x14, struct rtc_param)

#endif
