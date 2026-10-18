/* 兼容头：glibc 风格 SYS_*（musl 移植没带 sys/syscall.h）。
 * 直接映射到 LTP 生成的 __NR_* 表。 */
#ifndef _COMPAT_SYS_SYSCALL_H
#define _COMPAT_SYS_SYSCALL_H

#include <lapi/syscalls.h>

#define SYS_io_setup __NR_io_setup
#define SYS_getpid __NR_getpid
#define SYS_gettid __NR_gettid
#define SYS_read __NR_read
#define SYS_write __NR_write
#define SYS_open __NR_open
#define SYS_close __NR_close
#define SYS_fork __NR_fork
#define SYS_wait4 __NR_wait4
#define SYS_mmap __NR_mmap
#define SYS_munmap __NR_munmap
#define SYS_mprotect __NR_mprotect
#define SYS_brk __NR_brk
#define SYS_nanosleep __NR_nanosleep
#define SYS_clock_gettime __NR_clock_gettime
#define SYS_gettimeofday __NR_gettimeofday
#define SYS_dup __NR_dup
#define SYS_dup2 __NR_dup2
#define SYS_dup3 __NR_dup3
#define SYS_pipe __NR_pipe
#define SYS_pipe2 __NR_pipe2
#define SYS_unlink __NR_unlink
#define SYS_mkdir __NR_mkdir
#define SYS_rmdir __NR_rmdir
#define SYS_chdir __NR_chdir
#define SYS_getuid __NR_getuid
#define SYS_geteuid __NR_geteuid
#define SYS_getgid __NR_getgid
#define SYS_getegid __NR_getegid
#define SYS_setuid __NR_setuid
#define SYS_setgid __NR_setgid
#define SYS_kill __NR_kill
#define SYS_exit __NR_exit
#define SYS_exit_group __NR_exit_group
#define SYS_uname __NR_uname
#define SYS_stat __NR_stat
#define SYS_fstat __NR_fstat
#define SYS_lseek __NR_lseek
#define SYS_fcntl __NR_fcntl
#define SYS_ioctl __NR_ioctl

#endif
