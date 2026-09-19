
#include "kernel/kernel.h"

extern int module_ready;

// 定义模块注册宏
#define REGISTER_MODULE(module_name)      \
  {                                       \
    extern module_t module_name##_module; \
    module_regist(&module_name##_module); \
  }

void modules_init(void) {
  u32 i = 0;
  u32 count = 0;

  log_info("module regist\n");

  // require
  REGISTER_MODULE(devfs);


#ifdef ARMV7

  REGISTER_MODULE(gpio);
  REGISTER_MODULE(serial);
  REGISTER_MODULE(hello);
  REGISTER_MODULE(spi);
  REGISTER_MODULE(lcd);

#elif ARMV5

  REGISTER_MODULE(serial);
  REGISTER_MODULE(sdhci);
  REGISTER_MODULE(gpu);
  REGISTER_MODULE(mouse);
  REGISTER_MODULE(keyboard);
#ifdef FAT_MODULE
  REGISTER_MODULE(fat);
#endif

#ifdef FATFS_MODULE
  REGISTER_MODULE(fatfs);
#endif
  REGISTER_MODULE(test);


#elif defined(ARMV7_A)

  // optional module
  REGISTER_MODULE(serial);
  REGISTER_MODULE(gpio);
  REGISTER_MODULE(i2c);
  REGISTER_MODULE(spi);

#ifdef NET_DRIVER
  /* 只有真的有网卡驱动的平台才注册（drivers 见 duck/modules/net/ya.py）。
   * 用 NET_DRIVER 而不是 NET_MODULE：模块库是静态库，net_module 一旦被引用就会
   * 拉入 net.o 并要求 net_init_device 有定义。 */
  REGISTER_MODULE(net);
#endif

  REGISTER_MODULE(usb);
  REGISTER_MODULE(mouse);
  REGISTER_MODULE(gpu);

  REGISTER_MODULE(sdhci);

#ifdef LCD_MODULE
  REGISTER_MODULE(lcd);
#endif

#ifdef FAT_MODULE
  REGISTER_MODULE(fat);
#endif

#ifdef FATFS_MODULE
  REGISTER_MODULE(fatfs);
#endif
  // REGISTER_MODULE(fat32);
  // REGISTER_MODULE(hello);
  REGISTER_MODULE(test);

  REGISTER_MODULE(rtc);

#ifdef KEYBOARD_MODULE
  REGISTER_MODULE(keyboard);
#endif

#ifdef SOUND_MODULE
  REGISTER_MODULE(sound);
#endif

#ifdef POWER_MODULE
  REGISTER_MODULE(power);
#endif

#elif  defined(ARMV8_A)
  // optional module
  REGISTER_MODULE(serial);
  // REGISTER_MODULE(gpio);
  // REGISTER_MODULE(i2c);
  // REGISTER_MODULE(spi);
#ifdef RASPI5
  /* raspi5 bring-up: 还没有 DWC2 USB / BCM2836 外设驱动
   * （Pi5 用 xHCI + GIC），先只注册有平台支持的模块 */
  REGISTER_MODULE(test);
#else
  REGISTER_MODULE(mouse);
  REGISTER_MODULE(usb);
  REGISTER_MODULE(gpu);
  REGISTER_MODULE(sdhci);
  REGISTER_MODULE(net);
  REGISTER_MODULE(test);
  REGISTER_MODULE(rtc);
#endif


#ifdef FAT_MODULE
  REGISTER_MODULE(fat);
#endif

#ifdef FATFS_MODULE
  REGISTER_MODULE(fatfs);
#endif

#elif defined(DUMMY)
  REGISTER_MODULE(hello);

#elif defined(X86)
  // optional module
  REGISTER_MODULE(serial);
  REGISTER_MODULE(pci);
  REGISTER_MODULE(keyboard);
  REGISTER_MODULE(rtc);
  // REGISTER_MODULE(vga);
  REGISTER_MODULE(qemu);
  REGISTER_MODULE(mouse);
  REGISTER_MODULE(pty);
  REGISTER_MODULE(sb16);
  REGISTER_MODULE(ahci);
  REGISTER_MODULE(fat);
  REGISTER_MODULE(test);

#elif defined(XTENSA)
  REGISTER_MODULE(hello);

#elif defined(GENERAL)
  REGISTER_MODULE(serial);
  REGISTER_MODULE(sdhci);
  REGISTER_MODULE(fat);

  REGISTER_MODULE(hello);

#elif defined(RISCV)

  REGISTER_MODULE(serial);

  // REGISTER_MODULE(fat);
#else
  REGISTER_MODULE(hello);
#endif


#ifdef POSIX_MODULE
  REGISTER_MODULE(posix);
#endif

#ifdef PTY_MODULE
  REGISTER_MODULE(pty);
#endif

#ifdef LOG_MODULE
  REGISTER_MODULE(log);
#endif

#ifdef LOADER_MODULE
  REGISTER_MODULE(loader);
#endif

/* 【系统配置】必须排在 fatfs/fat 之后：它们把 SD 卡挂到 "/"，sysconf 才能读到
 * /conf/system.conf；同时要在 xwin/用户态应用开始画之前。
 * 见 duck/modules/sysconf/sysconf.c。 */
#ifdef SYSCONF_MODULE
  REGISTER_MODULE(sysconf);
#endif

#ifdef MUSL_MODULE
  REGISTER_MODULE(musl);
#endif

#ifdef EWOK_MODULE
  REGISTER_MODULE(ewok);
#endif

#ifdef GAGA_MODULE
  REGISTER_MODULE(gaga);
#endif

#ifdef TRACE_MODULE
  REGISTER_MODULE(trace);
#endif

#ifdef PERF_MODULE
  REGISTER_MODULE(perf);
#endif

#ifdef BACKTRACE_MODULE
  REGISTER_MODULE(backtrace);
#endif


#ifdef XWIN_MODULE
  REGISTER_MODULE(xwin);
#endif

  log_info("module regist end\n");

  module_run_all();

  log_info("module run all end\n");

  /* 系统配置（/conf/system.conf）由 sysconf 模块的 init 处理：它注册在
   * fatfs 之后、xwin 之前，见 duck/modules/sysconf/sysconf.c。 */

  /* 内核 fault 回溯测试：需要时取消注释（会终止 init 线程） */
  test_kernel();

  module_ready = 1;
}