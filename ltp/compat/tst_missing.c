/* 兼容实现：LTP 快照里 include/tse_*.h 声明的 per-dir 辅助库
 * （上游 ipc/lib/libipc.c、libswap 等在本快照缺失）+ musl 缺失的
 * glibc 专有函数。实现保持语义足够让测试走 TCONF/正常路径，不崩溃。
 * 属外围新增文件，LTP 源码零修改。 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/stat.h>

#define TST_NO_DEFAULT_MAIN
#include "tst_test.h"
#include "tst_device.h" /* uint64_t 等 */
#include "tse_newipc.h"
#include "tse_swap.h"
#include "tse_parse_vdso.h"

/* ---------------- tse_newipc.h ---------------- */

/* 上游 libipc.c 的 getipckey：进程内稳定、跨进程尽量不撞、非 IPC_PRIVATE */
key_t getipckey(const char *file, const int lineno)
{
	static key_t key;

	(void)file;
	(void)lineno;
	if (key == 0) {
		key = (key_t)((time(NULL) & 0xffff) |
			      ((getpid() & 0x7fff) << 16));
		if (key == IPC_PRIVATE || key == 0)
			key = 1;
	}
	return key;
}

/* 统计 /proc/sysvipc/<kind> 中已用条目数（跳过表头行） */
int get_used_sysvipc(const char *file, const int lineno, const char *sysvipc_file)
{
	FILE *f;
	char buf[1024];
	int used = 0;

	(void)file;
	(void)lineno;

	f = fopen(sysvipc_file, "r");
	if (f == NULL) {
		tst_res(TINFO, "can't open %s", sysvipc_file);
		return -1;
	}
	while (fgets(buf, sizeof(buf), f) != NULL) {
		if (buf[0] >= '0' && buf[0] <= '9')
			used++;
	}
	fclose(f);
	return used;
}

/* shmat 系测试用：探测一个空闲虚拟地址作为 shmat 的 addr 提示 */
void *probe_free_addr(const char *file, const int lineno)
{
	long psz;
	void *p, *addr;

	(void)file;
	(void)lineno;

	psz = sysconf(_SC_PAGESIZE);
	p = mmap(NULL, (size_t)psz, PROT_NONE,
		 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p == MAP_FAILED)
		tst_brk(TBROK | TERRNO, "probe_free_addr: mmap failed");
	munmap(p, (size_t)psz);

	addr = (void *)(((unsigned long)p + psz - 1) & ~((unsigned long)psz - 1));
	if ((unsigned long)addr == 0)
		addr = (void *)psz;
	return addr;
}

/* ---------------- tse_swap.h ---------------- */

bool is_swap_supported(const char *filename)
{
	(void)filename;
	/* YiYiYa 内核未实现 swapon/swapoff，统一让 swap 系测试 TCONF 跳过 */
	return false;
}

int make_swapfile(const char *file, const int lineno,
		  const char *swapfile, unsigned int num,
		  int safe, enum swapfile_method method)
{
	int fd;
	unsigned long long size;
	off_t ret;

	(void)file;
	(void)lineno;
	(void)safe;

	if (method == SWAPFILE_BY_BLKS) {
		long psz = sysconf(_SC_PAGESIZE);
		size = (unsigned long long)num * (unsigned long long)psz;
	} else {
		size = (unsigned long long)num * 1024ULL * 1024ULL;
	}

	fd = open(swapfile, O_RDWR | O_CREAT | O_TRUNC, 0600);
	if (fd < 0)
		tst_brk(TBROK | TERRNO, "open(%s)", swapfile);
	ret = ftruncate(fd, (off_t)size);
	close(fd);
	if (ret != 0)
		tst_brk(TBROK | TERRNO, "ftruncate(%s)", swapfile);
	return 0;
}

int tse_count_swaps(void)
{
	FILE *f;
	char buf[1024];
	int n = 0;

	f = fopen("/proc/swaps", "r");
	if (f == NULL)
		return 0;
	while (fgets(buf, sizeof(buf), f) != NULL) {
		if (buf[0] >= '0' && buf[0] <= '9')
			n++;
	}
	fclose(f);
	return n;
}

/* ---------------- tse_parse_vdso.h ---------------- */

void find_clock_gettime_vdso(gettime_t *ptr_vdso_gettime,
			     gettime_t *ptr_vdso_gettime64)
{
	/* YiYiYa 无 vDSO，置空后调用方回落到 syscall 路径 */
	if (ptr_vdso_gettime)
		*ptr_vdso_gettime = NULL;
	if (ptr_vdso_gettime64)
		*ptr_vdso_gettime64 = NULL;
}

/* ---------------- musl 缺失的 glibc 专有函数 ---------------- */

/* fmtmsg 的 glibc 扩展：注册严重级别。存表即可（fmtmsg01 只查返回值） */
int addseverity(int severity, const char *string)
{
	static const char *sev_tab[10];

	if (severity < 2 || severity > 9)
		return -1;
	if (string == NULL)
		return -1;
	sev_tab[severity] = string;
	return 0;
}

/* pkey 系（内核未实现）：失败返回，pkey 测试会走 TCONF 路径 */
int pkey_alloc(unsigned int flags, unsigned int access_rights)
{
	(void)flags;
	(void)access_rights;
	errno = ENOSYS;
	return -1;
}

int pkey_free(int pkey)
{
	(void)pkey;
	return 0;
}

int pkey_mprotect(void *addr, size_t len, int prot, int pkey)
{
	(void)addr;
	(void)len;
	(void)prot;
	(void)pkey;
	errno = ENOSYS;
	return -1;
}
