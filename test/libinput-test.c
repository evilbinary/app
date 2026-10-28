/*
 * libinput 最小验证程序（path 后端）。
 *
 * 用途：不依赖 udev 守护进程/热插拔，直接把 /dev/input/eventN 交给 libinput，
 * 打印它认出来的设备信息（名字/VID/PID/能力），并跑一小段事件循环。
 *
 * 走通这条链意味着下面几层都是真的在配合：
 *   /dev/input/event0（内核 input_core：ioctl 能力位、时间戳、S_IFCHR+rdev）
 *   → libudev（libudev-zero：stat 取 rdev，读 /sys/dev/char/13:64 与 uevent）
 *   → libevdev（读能力位）
 *   → libinput（ID_INPUT_* 属性 + 设备分类 + 指针曲线）
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

#include <libinput.h>

static int
open_restricted(const char *path, int flags, void *user_data)
{
	int fd = open(path, flags);

	(void)user_data;
	return fd < 0 ? -errno : fd;
}

static void
close_restricted(int fd, void *user_data)
{
	(void)user_data;
	close(fd);
}

static const struct libinput_interface interface = {
	.open_restricted = open_restricted,
	.close_restricted = close_restricted,
};

int
main(int argc, char **argv)
{
	const char *path = argc > 1 ? argv[1] : "/dev/input/event0";
	struct libinput *li;
	struct libinput_device *dev;
	struct libinput_event *ev;
	int i;

	printf("libinput test: adding %s\n", path);

	/* 先自证设备节点是 Linux 语义的字符设备：libinput 的 path 后端第一步
	 * 就是 stat() + S_ISCHR + st_rdev → /sys/dev/char/<major>:<minor>。 */
	{
		struct stat st;

		if (stat(path, &st) < 0) {
			printf("stat(%s) failed: errno=%d\n", path, errno);
		} else {
			printf("stat: mode=%06o rdev=%u:%u ischr=%d\n", st.st_mode,
			       major(st.st_rdev), minor(st.st_rdev),
			       S_ISCHR(st.st_mode) ? 1 : 0);
		}
	}

	li = libinput_path_create_context(&interface, NULL);
	if (li == NULL) {
		printf("libinput_path_create_context failed\n");
		return 1;
	}

	dev = libinput_path_add_device(li, path);
	if (dev == NULL) {
		printf("add_device(%s) FAILED (see libinput log above)\n", path);
		libinput_unref(li);
		return 1;
	}

	printf("device name   : %s\n", libinput_device_get_name(dev));
	printf("device sysname: %s\n", libinput_device_get_sysname(dev));
	printf("device ids    : vid=%04x pid=%04x\n",
	       libinput_device_get_id_vendor(dev),
	       libinput_device_get_id_product(dev));
	printf("capabilities  : keyboard=%d pointer=%d touch=%d gesture=%d switch=%d\n",
	       libinput_device_has_capability(dev,
					      LIBINPUT_DEVICE_CAP_KEYBOARD),
	       libinput_device_has_capability(dev,
					      LIBINPUT_DEVICE_CAP_POINTER),
	       libinput_device_has_capability(dev,
					      LIBINPUT_DEVICE_CAP_TOUCH),
	       libinput_device_has_capability(dev,
					      LIBINPUT_DEVICE_CAP_GESTURE),
	       libinput_device_has_capability(dev,
					      LIBINPUT_DEVICE_CAP_SWITCH));
	fflush(stdout);

	/* 1 秒事件循环：没有输入时只是反复 dispatch（验证 epoll 路径不炸） */
	for (i = 0; i < 100; i++) {
		libinput_dispatch(li);

		while ((ev = libinput_get_event(li)) != NULL) {
			enum libinput_event_type type = libinput_event_get_type(ev);

			if (type == LIBINPUT_EVENT_POINTER_MOTION) {
				struct libinput_event_pointer *p =
					libinput_event_get_pointer_event(ev);

				printf("motion dx=%.2f dy=%.2f\n",
				       libinput_event_pointer_get_dx(p),
				       libinput_event_pointer_get_dy(p));
			} else if (type == LIBINPUT_EVENT_POINTER_BUTTON) {
				struct libinput_event_pointer *p =
					libinput_event_get_pointer_event(ev);

				printf("button %u %s\n",
				       libinput_event_pointer_get_button(p),
				       libinput_event_pointer_get_button_state(p) ==
						       LIBINPUT_BUTTON_STATE_PRESSED ?
					       "pressed" :
					       "released");
			}
			libinput_event_destroy(ev);
			fflush(stdout);
		}
		usleep(10000);
	}

	libinput_unref(li);
	printf("libinput test: done\n");
	return 0;
}
