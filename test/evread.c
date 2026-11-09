/* evread：读 /dev/input/eventN，确认鼠标/键盘事件是否进了 input 子系统。
 *
 * 用法：
 *   evread                 读 event0（鼠标），5000ms
 *   evread 10000           读 event0，10000ms
 *   evread 10000 1         读 event1（键盘），10000ms
 *   evread event1 10000    同上
 *
 * 一次 read 会取走内核队列里所有事件（evdev 语义），这里批量解析并打印
 * 可读的键名，方便确认 USB 键盘/鼠标的报告有没有走到用户态。 */
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

struct input_event {
  long sec;
  long usec;
  unsigned short type;
  unsigned short code;
  int value;
};

#define EV_SYN 0x00
#define EV_KEY 0x01
#define EV_REL 0x02

#define NEV 64

static const char *type_name(unsigned short type) {
  switch (type) {
    case EV_SYN: return "EV_SYN";
    case EV_KEY: return "EV_KEY";
    case EV_REL: return "EV_REL";
    default: return "EV_OTHER";
  }
}

static const char *rel_name(unsigned short code) {
  switch (code) {
    case 0x00: return "REL_X";
    case 0x01: return "REL_Y";
    case 0x08: return "REL_WHEEL";
    default: return "REL_?";
  }
}

static const char *key_name(unsigned short code) {
  static char buf[16];
  static const struct {
    unsigned short code;
    const char *name;
  } letters[] = {
      {30, "KEY_A"}, {48, "KEY_B"}, {46, "KEY_C"}, {32, "KEY_D"},
      {18, "KEY_E"}, {33, "KEY_F"}, {34, "KEY_G"}, {35, "KEY_H"},
      {23, "KEY_I"}, {36, "KEY_J"}, {37, "KEY_K"}, {38, "KEY_L"},
      {50, "KEY_M"}, {49, "KEY_N"}, {24, "KEY_O"}, {25, "KEY_P"},
      {16, "KEY_Q"}, {19, "KEY_R"}, {31, "KEY_S"}, {20, "KEY_T"},
      {22, "KEY_U"}, {47, "KEY_V"}, {17, "KEY_W"}, {45, "KEY_X"},
      {21, "KEY_Y"}, {44, "KEY_Z"},
  };

  for (unsigned i = 0; i < sizeof(letters) / sizeof(letters[0]); i++) {
    if (letters[i].code == code) {
      return letters[i].name;
    }
  }
  if (code >= 2 && code <= 10) {
    snprintf(buf, sizeof(buf), "KEY_%c", '1' + (code - 2));
    return buf;
  }
  if (code >= 59 && code <= 68) {
    snprintf(buf, sizeof(buf), "KEY_F%d", code - 58);
    return buf;
  }
  if (code >= 71 && code <= 83) {
    snprintf(buf, sizeof(buf), "KEY_KP%d", code);
    return buf;
  }
  if (code >= 0x110 && code <= 0x117) {
    switch (code) {
      case 0x110: return "BTN_LEFT";
      case 0x111: return "BTN_RIGHT";
      case 0x112: return "BTN_MIDDLE";
      case 0x113: return "BTN_SIDE";
      case 0x114: return "BTN_EXTRA";
      case 0x115: return "BTN_FORWARD";
      case 0x116: return "BTN_BACK";
      case 0x117: return "BTN_TASK";
      default: break;
    }
  }
  switch (code) {
    case 1: return "KEY_ESC";
    case 11: return "KEY_0";
    case 12: return "KEY_MINUS";
    case 13: return "KEY_EQUAL";
    case 14: return "KEY_BACKSPACE";
    case 15: return "KEY_TAB";
    case 16: return "KEY_Q";
    case 17: return "KEY_W";
    case 18: return "KEY_E";
    case 19: return "KEY_R";
    case 20: return "KEY_T";
    case 21: return "KEY_Y";
    case 22: return "KEY_U";
    case 23: return "KEY_I";
    case 24: return "KEY_O";
    case 25: return "KEY_P";
    case 26: return "KEY_LEFTBRACE";
    case 27: return "KEY_RIGHTBRACE";
    case 28: return "KEY_ENTER";
    case 29: return "KEY_LEFTCTRL";
    case 42: return "KEY_LEFTSHIFT";
    case 43: return "KEY_BACKSLASH";
    case 51: return "KEY_COMMA";
    case 52: return "KEY_DOT";
    case 53: return "KEY_SLASH";
    case 54: return "KEY_RIGHTSHIFT";
    case 55: return "KEY_KPASTERISK";
    case 56: return "KEY_LEFTALT";
    case 57: return "KEY_SPACE";
    case 58: return "KEY_CAPSLOCK";
    case 69: return "KEY_NUMLOCK";
    case 70: return "KEY_SCROLLLOCK";
    case 74: return "KEY_KPMINUS";
    case 78: return "KEY_KPPLUS";
    case 83: return "KEY_KPDOT";
    case 96: return "KEY_KPENTER";
    case 97: return "KEY_RIGHTCTRL";
    case 98: return "KEY_KPSLASH";
    case 99: return "KEY_SYSRQ";
    case 100: return "KEY_RIGHTALT";
    case 102: return "KEY_HOME";
    case 103: return "KEY_UP";
    case 104: return "KEY_PAGEUP";
    case 105: return "KEY_LEFT";
    case 106: return "KEY_RIGHT";
    case 107: return "KEY_END";
    case 108: return "KEY_DOWN";
    case 109: return "KEY_PAGEDOWN";
    case 110: return "KEY_INSERT";
    case 111: return "KEY_DELETE";
    case 119: return "KEY_PAUSE";
    case 125: return "KEY_LEFTMETA";
    case 126: return "KEY_RIGHTMETA";
    case 127: return "KEY_COMPOSE";
    default:
      snprintf(buf, sizeof(buf), "KEY_%u", code);
      return buf;
  }
}

int main(int argc, char **argv) {
  int ms = 5000;
  int evno = 0;
  char path[64];

  if (argc > 1 && strncmp(argv[1], "event", 5) == 0) {
    evno = atoi(argv[1] + 5);
    if (argc > 2) {
      ms = atoi(argv[2]);
    }
  } else {
    if (argc > 1) {
      ms = atoi(argv[1]);
    }
    if (argc > 2) {
      evno = atoi(argv[2]);
    }
  }
  if (ms <= 0) {
    ms = 5000;
  }

  snprintf(path, sizeof(path), "/dev/input/event%d", evno);
  int fd = open(path, O_RDONLY);
  struct timespec t0, t1;
  if (fd < 0) {
    perror(path);
    return 1;
  }
  printf("evread: %s open for %d ms\n", path, ms);

  clock_gettime(CLOCK_MONOTONIC, &t0);
  for (;;) {
    struct pollfd p = {fd, POLLIN, 0};
    clock_gettime(CLOCK_MONOTONIC, &t1);
    if ((t1.tv_sec - t0.tv_sec) * 1000 +
            (t1.tv_nsec - t0.tv_nsec) / 1000000 >= ms) {
      break;
    }
    if (poll(&p, 1, 50) > 0) {
      struct input_event evs[NEV];
      ssize_t n = read(fd, evs, sizeof(evs));
      ssize_t cnt;
      if (n < (ssize_t)sizeof(evs[0])) {
        continue;
      }
      cnt = n / (ssize_t)sizeof(evs[0]);
      for (ssize_t i = 0; i < cnt; i++) {
        struct input_event *ev = &evs[i];
        if (ev->type == EV_REL) {
          printf("%lld.%06ld %s %s value=%d\n", (long long)ev->sec,
                 ev->usec, type_name(ev->type), rel_name(ev->code),
                 ev->value);
        } else if (ev->type == EV_KEY) {
          printf("%lld.%06ld %s %s value=%d (%s)\n", (long long)ev->sec,
                 ev->usec, type_name(ev->type), key_name(ev->code),
                 ev->value,
                 ev->value == 1 ? "press" :
                 (ev->value == 0 ? "release" : "repeat"));
        } else {
          printf("%lld.%06ld %s code=%u value=%d\n", (long long)ev->sec,
                 ev->usec, type_name(ev->type), ev->code, ev->value);
        }
      }
    }
  }
  close(fd);
  printf("evread: done\n");
  return 0;
}
