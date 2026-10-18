/* 兼容实现：tst_device.c 的设备分配接口（因 linux/loop.h 依赖被排除）。
 * YiYiYa 没有 loop 设备：acquire 恒失败（NULL），tst_test.c 会走
 * .needs_device 检查失败 → 测试 TCONF 跳过，这正是我们想要的结论。 */
#define _GNU_SOURCE
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "old/test.h"

const char *tst_acquire_device__(unsigned int size);
const char *tst_acquire_device_(void (*cleanup_fn)(void), unsigned int size);
uint64_t tst_get_device_size(const char *dev_path);
int tst_release_device(const char *dev);
int tst_clear_device(const char *dev);
int tst_dev_sync(int fd);
unsigned long tst_dev_bytes_written(const char *dev);
int tst_find_free_loopdev(char *path, size_t path_len);
int tst_attach_device(const char *dev_path, const char *file_path);
int tst_detach_device(const char *dev_path);
int tst_mount_has_opt(const char *path, const char *opt);
int tst_is_mounted(const char *path);
int tst_is_mounted_ro(const char *path);
int tst_is_mounted_rw(const char *path);
int tst_is_mounted_at_tmpdir(const char *path);

const const char *tst_acquire_device__(unsigned int size)
{
	(void)size;
	return NULL;
}

const char *tst_acquire_device_(void (*cleanup_fn)(void), unsigned int size)
{
	(void)cleanup_fn;
	(void)size;
	return NULL;
}

uint64_t tst_get_device_size(const char *dev_path)
{
	(void)dev_path;
	return 0;
}

int tst_release_device(const char *dev)
{
	(void)dev;
	return 0;
}

int tst_clear_device(const char *dev)
{
	(void)dev;
	return 0;
}

int tst_dev_sync(int fd)
{
	return fsync(fd);
}

unsigned long tst_dev_bytes_written(const char *dev)
{
	/* 原实现读 /sys/block/<dev>/stat 第 9 字段；YiYiYa 无该接口 */
	(void)dev;
	return 0;
}

int tst_find_free_loopdev(char *path, size_t path_len)
{
	(void)path;
	(void)path_len;
	return -1;
}

int tst_attach_device(const char *dev_path, const char *file_path)
{
	(void)dev_path;
	(void)file_path;
	return -1;
}

int tst_detach_device(const char *dev_path)
{
	(void)dev_path;
	return -1;
}

/* /proc/mounts 解析：YiYiYa 无真实挂载点，恒返回 0/未挂载，
 * mount 系测试会走失败路径而非崩溃。 */
static int mount_has(const char *path, const char *opt)
{
	FILE *f;
	char line[512];
	int ret = 0;

	f = fopen("/proc/mounts", "r");
	if (!f)
		return 0;
	while (fgets(line, sizeof(line), f)) {
		if (path && strstr(line, path) == NULL)
			continue;
		if (opt && strstr(line, opt) == NULL)
			continue;
		ret = 1;
		break;
	}
	fclose(f);
	return ret;
}

int tst_mount_has_opt(const char *path, const char *opt)
{
	return mount_has(path, opt);
}

int tst_is_mounted(const char *path)
{
	return mount_has(path, NULL);
}

int tst_is_mounted_ro(const char *path)
{
	return mount_has(path, "ro");
}

int tst_is_mounted_rw(const char *path)
{
	return mount_has(path, "rw");
}

int tst_is_mounted_at_tmpdir(const char *path)
{
	return mount_has(path, NULL);
}
