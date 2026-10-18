#!/usr/bin/env python3
# 从 LTP 的 arm64.in 生成 compat/lapi/syscalls.h（LTP 构建期产物，这里离线生成）
import os

ROOT = os.path.dirname(os.path.abspath(__file__))
IN = os.path.join(ROOT, "ltp", "include", "lapi", "syscalls", "arm64.in")
OUT_DIR = os.path.join(ROOT, "compat", "lapi")
OUT = os.path.join(OUT_DIR, "syscalls.h")

os.makedirs(OUT_DIR, exist_ok=True)
with open(IN) as f:
    lines = [x.split() for x in f if x.strip()]
with open(OUT, "w") as f:
    f.write("/* 自动生成：来自 LTP include/lapi/syscalls/arm64.in，勿手改 */\n")
    f.write("#ifndef LTP_COMPAT_SYSCALLS_H\n#define LTP_COMPAT_SYSCALLS_H\n\n")
    for parts in lines:
        if len(parts) >= 2:
            f.write("#define __NR_%s %s\n" % (parts[0], parts[1]))
    # 通用表的 time64 槽位（arm64.in 没收录，asm-generic unistd 403-423）
    time64 = {
        403: "clock_gettime64", 404: "clock_settime64", 405: "clock_adjtime64",
        406: "clock_getres_time64", 407: "clock_nanosleep_time64",
        408: "timer_gettime64", 409: "timer_settime64",
        410: "timerfd_gettime64", 411: "timerfd_settime64",
        412: "utimensat_time64", 413: "pselect6_time64", 414: "ppoll_time64",
        416: "io_pgetevents_time64", 418: "semtimedop_time64",
        419: "mq_timedsend_time64", 420: "mq_timedreceive_time64",
        422: "futex_time64", 423: "sched_rr_get_interval_time64",
    }
    for num in sorted(time64):
        f.write("#define __NR_%s %d\n" % (time64[num], num))
    f.write("\n#ifndef __LTP__NR_INVALID_SYSCALL\n")
    f.write("#define __LTP__NR_INVALID_SYSCALL -1\n#endif\n")
    f.write("#ifndef __LTP__NR_INVALID_SYSCALL\n#define __LTP__NR_INVALID_SYSCALL -1\n#endif\n")
    f.write("\n#endif\n")
print("wrote", OUT, len(lines), "syscalls")
