/*
 * test-timerfd：验证 timerfd + epoll 是否真的能按期唤醒。
 * weston 的 repaint timer 就是 timerfd + epoll；如果这里不 ready，
 * 合成器会停在 start_repaint_loop 后永不 repaint。
 */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char **argv) {
  struct itimerspec its;
  struct epoll_event ev;
  struct timespec t0, t1;
  int tfd, ep, i, n;
  int tmo = (argc > 1) ? atoi(argv[1]) : 300;
  uint64_t exp;

  memset(&its, 0, sizeof(its));
  its.it_value.tv_nsec = 50 * 1000 * 1000; /* 50ms */

  tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
  printf("timerfd_create=%d errno=%d\n", tfd, errno);
  if (tfd < 0)
    return 1;

  ep = epoll_create1(0);
  printf("epoll_create1=%d errno=%d\n", ep, errno);
  if (ep < 0)
    return 1;

  memset(&ev, 0, sizeof(ev));
  ev.events = EPOLLIN;
  ev.data.u64 = 0x1234;
  printf("epoll_ctl=%d errno=%d\n",
         epoll_ctl(ep, EPOLL_CTL_ADD, tfd, &ev), errno);

  printf("timerfd_settime=%d errno=%d\n",
         timerfd_settime(tfd, 0, &its, NULL), errno);

  clock_gettime(CLOCK_MONOTONIC, &t0);
  for (i = 0; i < 200; i++) {
    memset(&ev, 0, sizeof(ev));
    n = epoll_wait(ep, &ev, 1, 300);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    if (n != 0 || (i % 10) == 0)
      printf("iter=%d epoll=%d events=%x data=%llx errno=%d elapsed=%ldms\n",
             i, n, ev.events, (unsigned long long)ev.data.u64, errno,
             (long)((t1.tv_sec - t0.tv_sec) * 1000 +
                    (t1.tv_nsec - t0.tv_nsec) / 1000000));
    if (n > 0) {
      exp = 0;
      errno = 0;
      n = read(tfd, &exp, sizeof(exp));
      printf("  read=%d exp=%llu errno=%d\n", n,
             (unsigned long long)exp, errno);
      break;
    }
  }

  close(ep);
  close(tfd);
  printf("=== done ===\n");
  return 0;
}
