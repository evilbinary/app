/* 兼容实现：tst_umount 原版在 lib/tst_device.c（因 linux/loop.h 依赖被
 * 排除）。这里提供等价函数：EBUSY 重试后放弃。 */
#define TST_NO_DEFAULT_MAIN
#include <errno.h>
#include <unistd.h>
#include "tst_test.h"

int tst_umount(const char *path)
{
	int i;

	for (i = 0; i < 20; i++) {
		if (umount(path) == 0)
			return 0;

		if (errno != EBUSY)
			break;

		usleep(50000);
	}

	tst_res(TWARN, "umount('%s') failed: %s", path, strerror(errno));
	return -1;
}
