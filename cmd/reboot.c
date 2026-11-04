// 自动重启：reboot(2) → 内核 sys_reboot → PSCI SYSTEM_RESET。
// 用法：
//   reboot        正常重启
//   reboot hang   关中断死循环（验证硬件看门狗：15 秒后应自动复位）
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/reboot.h>
#include <sys/syscall.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "hang") == 0) {
        printf("hang: disabling IRQ inside kernel, waiting for HW watchdog\n");
        /* 0xdeadbeef 是内核里约定的「挂死」命令字 */
        syscall(SYS_reboot, 0xfee1dead, 672274793, 0xdeadbeef, 0);
        perror("hang");
        return 1;
    }
    printf("reboot: PSCI SYSTEM_RESET\n");
    /* musl 会按 Linux 约定发 syscall(SYS_reboot, 0xfee1dead, 672274793, cmd, 0) */
    if (reboot(RB_AUTOBOOT) != 0) {
        perror("reboot");
        return 1;
    }
    return 0;
}
