/*******************************************************************
 * Copyright 2021-present evilbinary
 * 作者: evilbinary on 01/01/20
 * 邮箱: rootdebug@163.com
 ********************************************************************/
#include "main.h"

int module_ready = 0;

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
  do_idle_thread();
}

/* Kept for older call sites; same as idle. */
void do_monitor_thread(void) { do_idle_thread(); }
