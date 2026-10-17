/*******************************************************************
 * Copyright 2021-present evilbinary
 * 作者: evilbinary on 01/01/20
 * 邮箱: rootdebug@163.com
 ********************************************************************/
/* fand：用户态风扇温控守护。
 *
 * 内核只提供两个原语（都在 /dev 下，读写各几行代码）：
 *   /dev/cpu  read 4 字节 = 毫摄氏度（duck/modules/cpu，firmware mailbox）
 *   /dev/fan  read/write 1 字节 = level 0-10（duck/modules/fan，RP1 PWM1）
 *
 * 策略放这里而不是内核线程：这套内核的内核线程调用帧会和别的线程互踩
 * （内核态可被时钟抢占 + 调用帧在共享栈上），一个 2 秒周期的策略没必要
 * 冒这个险；放用户态崩了也只死一个进程，内核照常跑。
 *
 * 【滞回】升档立刻响应（mc 达到阈值就升），降档要等温度比当前档位的阈值
 * 再低 HYST_MC 才降。没有滞回时，温度停在 45°C 附近会在 2 档/4 档之间
 * 每 2 秒来回跳（实测 45.5 -> 44.9 -> 45.5 ...），听感上风扇一直变速。
 *
 * 【手动】每轮会先读回 /dev/fan 的当前档位再决定是否改，所以手动 write
 * 会在 2 秒内被温度策略纠正；想长期手动就 kill 掉 fand。
 */
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#define TEMP_STEPS 4
#define LEVEL_MIN 2
#define HYST_MC 3000 /* 3°C 滞回；嫌切换频繁就调大，嫌反应慢就调小 */

/* 升档阈值（毫摄氏度）与对应档位：>= 阈值就取该档 */
static const int up_mc[TEMP_STEPS] = {75000, 65000, 55000, 45000};
static const int level_of[TEMP_STEPS] = {10, 8, 6, 4};

static int level_for(int mc, int cur) {
  int target = LEVEL_MIN;
  for (int i = 0; i < TEMP_STEPS; i++) {
    if (mc >= up_mc[i]) {
      target = level_of[i];
      break;
    }
  }
  if (target >= cur) {
    return target; /* 升温：立刻升档 */
  }
  for (int i = 0; i < TEMP_STEPS; i++) {
    if (level_of[i] == cur) {
      /* 降温：要低于"当前档位的阈值 - 滞回"才降，避免阈值附近抖动 */
      return (mc <= up_mc[i] - HYST_MC) ? target : cur;
    }
  }
  return target; /* 当前档位不在表里（手动设过 0/1/3...）→ 直接跟随 */
}

static void sleep_ms(unsigned int ms) {
  struct timespec ts;
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (long)(ms % 1000) * 1000000L;
  nanosleep(&ts, NULL);
}

int main(int argc, char* argv[]) {
  printf("fand: start\n");

  for (;;) {
    int mc = 0;
    int has_mc = 0;
    int cur = -1;

    int fd = open("/dev/cpu", O_RDONLY);
    if (fd >= 0) {
      int n = read(fd, &mc, sizeof(mc));
      close(fd);
      has_mc = (n == (int)sizeof(mc));
    }

    fd = open("/dev/fan", O_RDONLY);
    if (fd >= 0) {
      unsigned char c = 0;
      if (read(fd, &c, 1) == 1) {
        cur = (int)c;
      }
      close(fd);
    }

    if (has_mc && cur >= 0) {
      int level = level_for(mc, cur);
      if (level != cur) {
        fd = open("/dev/fan", O_WRONLY);
        if (fd >= 0) {
          unsigned char v = (unsigned char)level;
          if (write(fd, &v, 1) == 1) {
            printf("fand: %d.%d c -> level %d\n", mc / 1000, (mc % 1000) / 100,
                   level);
          }
          close(fd);
        }
      }
    }
    sleep_ms(2000);
  }
  return 0;
}
