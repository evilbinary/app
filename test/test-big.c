/* 大 .bss 探针：验证「大程序崩」是不是内核缺页/映射的问题。
 *
 * 背景：test-rand（5.6MB，链 libcrypto）在 qemu 里单步时，PC 从一条普通
 *       `str w1, [sp,#20]` 直接跳到非段地址（0xafe345f0），疑似缺页/异常
 *       处理返回 PC 错误。sshd（7MB）同样崩。而 ls/cat（200KB）正常。
 *
 * 判定：
 *   打印 "big: ok"        → 大 .bss 没问题，锅在 OpenSSL
 *   崩/输出异常/不提 ok   → 内核在大段缺页上有 bug（通杀修复）
 */
#include <stdio.h>

static char big[6 * 1024 * 1024];

int main(void) {
    int i;

    printf("big: start, size=%d MB\n", (int)(sizeof(big) >> 20));

    /* 每页碰一次，强制触发缺页（.bss 是零页映射，不碰就按需分配） */
    for (i = 0; i < (int)sizeof(big); i += 4096)
        big[i] = (char)i;

    printf("big: touched all pages\n");

    /* 回读校验，确认写进去的内容没丢 */
    for (i = 0; i < (int)sizeof(big); i += 4096) {
        if (big[i] != (char)i) {
            printf("big: mismatch at %d\n", i);
            return 1;
        }
    }

    printf("big: ok\n");
    return 0;
}
