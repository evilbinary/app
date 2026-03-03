/*******************************************************************
 * Copyright 2021-present evilbinary
 * 作者: evilbinary on 01/01/20
 * 邮箱: rootdebug@163.com
 ********************************************************************/
#include "../../duck/init/main.h"
#include "../../duck/init/init.h"


extern void do_kernel_thread();

void kstart(int argc, char* argv[], char** envp) {
  boot_info_t* boot_info = envp[0];
  // int cpu = envp[1];
  int cpu = cpu_get_id();
  arch_init(boot_info, cpu);

  kmain(argc, argv);
   
  for (;;) {
    cpu_halt();
  }
}




// 通过系统调用写入LCD填充矩形
void lcd_fill_rect(int x, int y, int w, int h, u16 color) {
     
  // 打开LCD设备
int fd = syscall2(SYS_OPEN, "/dev/lcd", 0);
if (fd < 0) {
  kprintf("open /dev/lcd failed!\n");
  return;
}

// 构造填充命令: "FILL x y w h color\n"
char cmd_buf[64];
sprintf(cmd_buf, "FILL %d %d %d %d %d\n", x, y, w, h, color);

// 写入命令
syscall3(SYS_WRITE, fd, cmd_buf, kstrlen(cmd_buf));

// 关闭设备
syscall1(SYS_CLOSE, fd);
}


void thread_lcd(){
    // wait module ready
    while (module_ready <= 0) {
      // sleep();
    }

    while(1){
      sleep_ms(1000);
      
      // 填充红色矩形 (x:10-50, y:10-50)
      lcd_fill_rect(10, 10, 40, 40, 0xf800);  // RED
      
      // 填充蓝色矩形 (x:60-100, y:10-50)
      lcd_fill_rect(60, 10, 40, 40, 0x001f); // BLUE
      
      // 循环动画：移动的绿色矩形
      for (int i = 0; i < 5; i++) {
          // 绘制绿色矩形
          lcd_fill_rect(10 + i * 10, 60, 40, 40, 0x07e0); // GREEN
          sleep_ms(200);
          // 擦除（用黑色）
          lcd_fill_rect(10 + i * 10, 60, 40, 40, 0x0000); // BLACK
      }
      // 最后绘制黄色矩形
      lcd_fill_rect(60, 60, 40, 40, 0xffe0); // YELLOW
  }
    
}


void thread_lcd2(){

  while(1){
    sleep_ms(200);
  }
}


int kmain(int argc, char* argv[]) {
  kernel_init();

  log_info("kernel thread init\n");

  thread_t* t1 = thread_create_name_level("kernel", (void*)&do_kernel_thread,
                                          NULL, LEVEL_KERNEL_SHARE);
  thread_t* t2 = thread_create_name("lcd", (void*)&thread_lcd, NULL);
  thread_run(t1);
  thread_run(t2);

  thread_t* t3 = thread_create_name("lcd2", (void*)&thread_lcd2, NULL);
  thread_run(t3);


  log_info("kernel run start\n");

  kernel_run();

  return 0;
}