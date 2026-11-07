/* evread：读 /dev/input/event0，确认 USB 鼠标事件是否进了 input 子系统。
 * 用法：evread [毫秒]，默认 5000ms。移动鼠标时应打印 type/code/value。 */
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

struct input_event {
  long sec;
  long usec;
  unsigned short type;
  unsigned short code;
  int value;
};

int main(int argc, char **argv) {
  int ms = argc > 1 ? atoi(argv[1]) : 5000;
  int fd = open("/dev/input/event0", O_RDONLY);
  struct timespec t0, t1;
  if (fd < 0) {
    perror("open event0");
    return 1;
  }
  printf("evread: /dev/input/event0 open\n");
  clock_gettime(CLOCK_MONOTONIC, &t0);
  for (;;) {
    struct pollfd p = {fd, POLLIN, 0};
    clock_gettime(CLOCK_MONOTONIC, &t1);
    if ((t1.tv_sec - t0.tv_sec) * 1000 +
            (t1.tv_nsec - t0.tv_nsec) / 1000000 >= ms) {
      break;
    }
    if (poll(&p, 1, 50) > 0) {
      struct input_event ev;
      if (read(fd, &ev, sizeof(ev)) == (int)sizeof(ev)) {
        printf("type=%u code=%u value=%d\n", ev.type, ev.code, ev.value);
      }
    }
  }
  close(fd);
  printf("evread: done\n");
  return 0;
}
