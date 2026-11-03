/* 最小 SIGILL + sigsetjmp/siglongjmp 复现。
 *
 * 背景：OpenSSL 的 OPENSSL_cpuid_setup() 用「执行一条试探指令 + 捕获 SIGILL」
 * 来探测 CPU 指令集（因为裸机没有 auxv，getauxval 恒返回 0）。在设备上它
 * 表现为死循环（calls 全是 rt_sigprocmask、CPU 20%+），我们用 armcap 补丁
 * 绕过了。但**绕过不等于没问题**：如果内核的信号返回路径（siglongjmp 恢复）
 * 有 bug，任何依赖 SIGILL 探测的程序都会中招。
 *
 * 本程序就是那个「最小复现」：故意执行一条未定义指令 udf #0 触发 SIGILL，
 * handler 里 siglongjmp 跳回来。
 *
 * 判定：
 *   打印 "SIGILL caught" + "done"   → 内核信号路径正常，OpenSSL 那个坑是
 *                                      多层嵌套/其它因素，属于库自身用法问题
 *   卡住/无输出/重复打印 "executing" → 内核 bug（siglongjmp 恢复不了）
 */
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

static sigjmp_buf jb;
static volatile sig_atomic_t hit = 0;

static void on_sigill(int sig)
{
    (void)sig;
    printf("  [h] handler enter\n");
    hit = 1;
    printf("  [h] handler before longjmp\n");
    siglongjmp(jb, 1);
    printf("  [h] handler after longjmp (NOT REACHED)\n");
}

int main(void)
{
    struct sigaction sa;
    int round;

    printf("sigill: start\n");

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_sigill;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_NODEFER;
    if (sigaction(SIGILL, &sa, NULL) != 0) {
        printf("sigill: sigaction failed\n");
        return 1;
    }
    printf("sigill: handler installed\n");

    /* 连做 3 轮，模拟 OpenSSL 逐条指令试探的行为 */
    for (round = 0; round < 3; round++) {
        printf("sigill: round %d, executing udf\n", round);
        if (sigsetjmp(jb, 0) == 0) {
            __asm__ __volatile__("udf #0");   /* 未定义指令 → SIGILL */
            printf("sigill: round %d NOT TRAPPED\n", round);
        } else {
            printf("sigill: round %d caught, back from longjmp\n", round);
        }
    }

    printf("sigill: done (hit=%d)\n", (int)hit);
    return 0;
}
