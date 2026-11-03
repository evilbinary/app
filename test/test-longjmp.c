/* setjmp/longjmp 基础功能探针。
 *
 * 背景：test-sigill 里 SIGILL 已经能送达 handler（hit=1），但 handler 里的
 * siglongjmp 没有跳回 sigsetjmp 的位置（没有打印 "caught"，而是 handler
 * 正常返回、rt_sigreturn 回到 udf 之后继续）。需要先分清是：
 *   A. setjmp/longjmp 本身坏了（musl 的 aarch64 汇编 + 我们的环境）
 *   B. 只有 siglongjmp 的「信号掩码恢复」那一步坏了
 *
 * 本程序只测 A（不涉及任何信号）。
 *
 * 判定：
 *   打印 "back from longjmp, val=42" + "done"  → A 正常，问题在 siglongjmp
 *   不打印，直接 "NOT REACHED" 或退出          → A 坏了（setjmp/longjmp 汇编）
 */
#include <setjmp.h>
#include <stdio.h>

static jmp_buf jb;
static int depth = 0;

static void inner(void)
{
    printf("  inner: longjmp\n");
    longjmp(jb, 42);
    printf("  inner: NOT REACHED\n");
}

int main(void)
{
    int v;

    printf("longjmp: start\n");

    v = setjmp(jb);
    if (v == 0) {
        printf("  setjmp returned 0, calling inner\n");
        depth++;
        inner();
        printf("  main: NOT REACHED\n");
    } else {
        printf("  back from longjmp, val=%d depth=%d\n", v, depth);
    }

    printf("longjmp: done\n");
    return 0;
}
