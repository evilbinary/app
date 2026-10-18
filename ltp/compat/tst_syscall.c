/* 兼容实现：tst_syscall / tst_brkm_ —— 原版在 LTP 构建期生成
 * （ltp-version.h/syscalls.h 同理，见 gen_syscalls.py）。 */
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
/* 只引 old/test.h：与 tst_test.h 互斥（二者都有防重入哨兵） */
#include "old/test.h"

long tst_syscall(int syscall_nr, ...)
{
	va_list ap;
	unsigned long a[6] = { 0, 0, 0, 0, 0, 0 };
	int i;

	va_start(ap, syscall_nr);
	for (i = 0; i < 6; i++)
		a[i] = va_arg(ap, unsigned long);
	va_end(ap);

	return syscall(syscall_nr, a[0], a[1], a[2], a[3], a[4], a[5]);
}

/* 旧 API 的 tst_brkm_：safe_file_ops.c 等以函数形式引用，
 * 转发到 tst_res.c 的 tst_brkm__（noreturn）。
 * old/test.h 把 tst_brkm_ 定义成了宏，这里需要真函数，先 undef。 */
#undef tst_brkm_
void tst_brkm_(const char *file, const int lineno, int ttype,
	       void (*func)(void), const char *arg_fmt, ...)
{
	char tmesg[4096];
	va_list ap;

	va_start(ap, arg_fmt);
	vsnprintf(tmesg, sizeof(tmesg), arg_fmt, ap);
	va_end(ap);

	tst_brkm__(file, lineno, ttype, func, "%s", tmesg);
}
