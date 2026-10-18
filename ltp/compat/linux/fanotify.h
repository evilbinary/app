/* 兼容垫片：lib/tst_fd.c 只用到 FAN_CLASS_NOTIF 一个常量
 * （fanotify_init 在 YiYiYa 内核未实现，open 时返回错误走 TCONF 路径）。 */
#ifndef COMPAT_LINUX_FANOTIFY_H
#define COMPAT_LINUX_FANOTIFY_H

#define FAN_CLASS_NOTIF		0x00000001
#define FAN_CLASS_CONTENT	0x00000002
#define FAN_CLASS_PRE_CONTENT	0x00000008
#define FAN_CLOEXEC		0x00000001
#define FAN_NONBLOCK		0x00000002

#endif /* COMPAT_LINUX_FANOTIFY_H */
