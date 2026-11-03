/*******************************************************************
 * Copyright 2021-present evilbinary
 * 作者: evilbinary on 01/01/20
 * 邮箱: rootdebug@163.com
 ********************************************************************/
#include "main.h"

int module_ready = 0;

/* 看门狗由 watchdog 模块提供（duck/modules/watchdog/）。用 weak 引用：
 * 模块没编进内核时这里是 NULL，判空即可，不产生链接依赖。 */
extern void watchdog_poll(void) __attribute__((weak));

/* Per-CPU idle: stay RUNNING + WFI. Ticks on this thread = idle time. */
void do_idle_thread(void) {
  for (;;) {
    cpu_wait();
  }
}

void do_kernel_thread(void) {
  kprintf("init kernel thread\n");
  modules_init();

  /* Become CPU0 idle so busy% = 100 - idle_ticks/cpu_ticks. */
  {
    thread_t* cur = thread_current();
    if (cur != NULL) {
      cur->name = (u8*)"idle";
      cur->priority = THREAD_PRIORITY_IDLE;
      cur->priority_base = THREAD_PRIORITY_IDLE;
    }
  }

  /* 【复用本线程做看门狗】它就是 CPU0 的 idle/monitor：每次 tick 中断把
   * 它从 cpu_wait() 唤醒时，顺手做一次健康检查。不新建线程，也不改调度器。 */
  for (;;) {
    if (watchdog_poll != NULL) {
      watchdog_poll();
    }
    cpu_wait();
  }
}

/* Kept for older call sites; same as idle. */
void do_monitor_thread(void) { do_idle_thread(); }
