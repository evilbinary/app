/*
 * test-memfd-mmap：定位 wl_shm 用的 memfd + mmap 失败原因。
 *
 * 背景：weston-desktop 的 desktop-shell 客户端在 make_shm_pool() 里
 *   memfd_create → posix_fallocate(size) → mmap(MAP_SHARED)
 * 失败（musl 报 EPERM，因为本内核 mmap 失败统一返回 -1）。
 *
 * 本程序用【原始 svc】直接取 mmap 的返回值，绕过 musl 的 -errno 转换，
 * 从而把内核真正的错误码打出来；并按尺寸扫描，找出失败阈值。
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef SYS_mmap
#define SYS_mmap 222
#endif
#ifndef SYS_munmap
#define SYS_munmap 215
#endif
#ifndef SYS_memfd_create
#define SYS_memfd_create 279
#endif
#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 0x0001u
#endif
#ifndef MFD_ALLOW_SEALING
#define MFD_ALLOW_SEALING 0x0002u
#endif

static inline long raw_syscall6(long n, long a, long b, long c, long d, long e,
                                long f) {
  register long x8 __asm__("x8") = n;
  register long x0 __asm__("x0") = a;
  register long x1 __asm__("x1") = b;
  register long x2 __asm__("x2") = c;
  register long x3 __asm__("x3") = d;
  register long x4 __asm__("x4") = e;
  register long x5 __asm__("x5") = f;
  __asm__ volatile("svc 0"
                   : "+r"(x0)
                   : "r"(x8), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5)
                   : "memory");
  return x0;
}

static long raw_mmap(void *addr, size_t len, int prot, int flags, int fd,
                     off_t off) {
  return raw_syscall6(SYS_mmap, (long)addr, (long)len, prot, flags, fd,
                      (long)off);
}

static long raw_munmap(void *addr, size_t len) {
  return raw_syscall6(SYS_munmap, (long)addr, (long)len, 0, 0, 0, 0);
}

static void show_result(const char *tag, long r) {
  if (r < 0 && r > -4096) {
    printf("%s: FAIL raw=%ld errno=%ld (%s)\n", tag, r, -r, strerror((int)-r));
  } else if (r == -1) {
    /* 内核旧的 MAP_FAILED：musl 会把它当 EPERM */
    printf("%s: FAIL raw=-1 (kernel MAP_FAILED)\n", tag);
  } else {
    printf("%s: ok raw=0x%lx\n", tag, (unsigned long)r);
  }
  fflush(stdout);
}

static int test_size(size_t size) {
  char tag[96];
  int fd = (int)syscall(SYS_memfd_create, "test-mm", MFD_CLOEXEC | MFD_ALLOW_SEALING);
  if (fd < 0) {
    printf("size=%zu: memfd_create FAIL errno=%d (%s)\n", size, errno,
           strerror(errno));
    fflush(stdout);
    return -1;
  }
  printf("size=%zu: memfd fd=%d\n", size, fd);
  fflush(stdout);

  errno = 0;
  int fr = posix_fallocate(fd, 0, (off_t)size);
  printf("size=%zu: posix_fallocate ret=%d errno=%d (%s)\n", size, fr, errno,
         strerror(errno));
  fflush(stdout);

  long r = raw_mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  snprintf(tag, sizeof(tag), "size=%zu: raw mmap", size);
  show_result(tag, r);
  if (r >= 0 && r > -4096) {
    volatile char *p = (volatile char *)(unsigned long)r;
    p[0] = 0x5a;
    p[size - 1] = 0xa5;
    printf("size=%zu: touch first=%02x last=%02x\n", size, p[0], p[size - 1]);
    fflush(stdout);
    long mr = raw_munmap((void *)(unsigned long)r, size);
    printf("size=%zu: munmap raw=%ld\n", size, mr);
    fflush(stdout);
  }

  /* 同时用 libc 的 mmap 复核 errno */
  errno = 0;
  void *lp = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  printf("size=%zu: libc mmap=%p errno=%d (%s)\n", size, lp, errno,
         strerror(errno));
  fflush(stdout);
  if (lp != MAP_FAILED)
    munmap(lp, size);

  close(fd);
  return (r >= 0 && r > -4096) ? 0 : -1;
}

int main(void) {
  size_t sizes[] = {4096, 65536, 1048576, 2097152, 4194304, 8294400};
  int i;

  printf("=== memfd mmap probe ===\n");
  fflush(stdout);

  /* 控制组：匿名 mmap 同样大小 */
  long anon = raw_mmap(NULL, 8294400, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  show_result("anon mmap 8294400", anon);
  if (anon >= 0 && anon > -4096) {
    ((volatile char *)(unsigned long)anon)[0] = 1;
    ((volatile char *)(unsigned long)anon)[8294400 - 1] = 2;
    raw_munmap((void *)(unsigned long)anon, 8294400);
  }

  for (i = 0; i < (int)(sizeof(sizes) / sizeof(sizes[0])); i++)
    test_size(sizes[i]);

  /* 父子共享验证：只在小尺寸上做，避免拖慢串口/日志 */
  {
    size_t size = 65536;
    int fd = (int)syscall(SYS_memfd_create, "test-share", MFD_CLOEXEC);
    if (fd >= 0 && posix_fallocate(fd, 0, (off_t)size) == 0) {
      long r = raw_mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
      show_result("share mmap 65536", r);
      if (r >= 0 && r > -4096) {
        volatile char *p = (volatile char *)(unsigned long)r;
        p[0] = 0x11;
        pid_t pid = fork();
        if (pid == 0) {
          p[0] = 0x77;
          _exit(p[0] == 0x77 ? 0 : 1);
        } else if (pid > 0) {
          int st = 0;
          waitpid(pid, &st, 0);
          printf("fork write: child=%d parent sees p[0]=%02x status=%x\n",
                 (int)pid, p[0], st);
          fflush(stdout);
        }
        raw_munmap((void *)(unsigned long)r, size);
      }
    }
    close(fd);
  }

  printf("=== done ===\n");
  fflush(stdout);
  return 0;
}
