/* 兼容实现：tst_wallclock_save/restore —— 原版在 lib/tst_wallclock.c
 * （依赖 linux/rtc.h 的 RTC 干扰检测被排除）。YiYiYa 无此干扰源，空实现。 */
#include "tst_wallclock.h"

void tst_wallclock_save(void)
{
}

void tst_wallclock_restore(void)
{
}
