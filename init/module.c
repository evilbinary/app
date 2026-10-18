
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

  /* 诊断：/dev/irq（统一中断框架的 IRQ 表，只读）。
   * 放在 devfs 之后：它要往 /dev 里挂节点；本身不依赖任何平台特性，
   * 没注册 irq_chip 的平台只会看到一行提示（不是错误）。 */
  REGISTER_MODULE(irq);


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
  /* 先切分区（sda1/sda2…），fatfs 再按这些块设备挂载。 */
  REGISTER_MODULE(partition);
  REGISTER_MODULE(fatfs);
  REGISTER_MODULE(ext);
#endif
  REGISTER_MODULE(test);
  REGISTER_MODULE(rtc);
  REGISTER_MODULE(sound);


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
  /* 先切分区（sda1/sda2…），fatfs 再按这些块设备挂载。 */
  REGISTER_MODULE(partition);
  REGISTER_MODULE(fatfs);
  REGISTER_MODULE(ext);
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
  /* bcm2712 SDHCI（microSD，dtsi sdio1@fff000）：复用 bcm2836.c 标准 SDHCI
   * 驱动。必须排在 partition / fatfs 之前 —— partition 读这张整卡切分区，
   * fatfs 再挂根文件系统。 */
  REGISTER_MODULE(sdhci);
  /* 没有接 Pi5 的电池 RTC。general.c 用启动后的 tick 提供 /dev/time。 */
  REGISTER_MODULE(rtc);
  /* RP1 Cadence GEM，BAR 在 0x1F00100000。失败只打日志。 */
  REGISTER_MODULE(net);
  /* 【BCM2712 显示：走固件 mailbox 申请 framebuffer】
   * 双 HDMI（hdmi0@7c701400 / hdmi1@7c706400）注册 /dev/fb0、/dev/fb1。
   * 注意：Pi5 固件若没开启显示输出（config.txt 无 dtoverlay=vc4-kms-v3d 且
   * 未插显示器），ALLOCATE_BUFFER tag 会失败，gpu 只打错误日志不致命。 */
  REGISTER_MODULE(gpu);

  REGISTER_MODULE(gpio);
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
  /* 先切分区（sda1/sda2…），fatfs 再按这些块设备挂载。 */
  REGISTER_MODULE(partition);
  REGISTER_MODULE(fatfs);
  REGISTER_MODULE(ext);
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

/* /dev/fan 与 /dev/cpu 在所有平台都注册：raspi5 走真硬件（fan.c/fan RP1 PWM1、
 * cpu.c/firmware mailbox），其它平台走各自的 general.c 空实现（read 返回 0 /
 * 档位只存内存）。这样用户态 fand 到处都能安静地跑，不会每 2 秒刷 open 失败。
 * 源文件由 duck/modules/{fan,cpu}/ya.py 按平台二选一编译。 */
#ifdef FAN_MODULE
  REGISTER_MODULE(fan);
#endif

#ifdef CPU_MODULE
  REGISTER_MODULE(cpu);
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

/* 【页交换 backing】必须排在 fatfs 之后（文件形式要 /mnt/* 已挂好、分区形式
 * 要 partition 已枚举出 sdaN）、sysconf 之前——sysconf 的 init 就会触发
 * sysconf_apply，之后注册的回调不会再被执行。见 duck/modules/swap/swap.c。 */
#ifdef SWAP_MODULE
  REGISTER_MODULE(swap);
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