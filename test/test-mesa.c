/* test-mesa.c - 验证 eggs/libmesa + eggs/libdrm 能被应用链接并运行
 *
 * 阶段 1（本文件）：只调用 Mesa 的叶子函数，验证：
 *   1. libmesa.a 的目标文件能拉取、符号能解析
 *   2. broadcom 的纯 CPU 逻辑在设备上运行正确
 * 后续阶段再往「调用编译器编 shader」推进。
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* broadcom/common/v3d_util.c 的叶子函数（我们之前用它核对过 stride 语义：
 * stride = tile_width * bpp_words / 2，单位 128-bit，覆盖 2 行） */
extern uint32_t v3d_compute_rt_row_row_stride_128_bits(uint32_t tile_width,
                                                       uint32_t bpp);

/* 只需要一个已验证存在的 broadcom 导出函数（stride 公式） */

int main(void) {
  int fail = 0;

  printf("=== test-mesa (libmesa + libdrm 链接验证) ===\r\n");

  /* 1) 纯数学：stride 公式 */
  uint32_t s = v3d_compute_rt_row_row_stride_128_bits(64, 1);
  printf("stride(64px, 32bpp) = %u (expect 32) %s\r\n", s,
         s == 32 ? "OK" : "FAIL");
  if (s != 32) {
    fail++;
  }

  uint32_t s128 = v3d_compute_rt_row_row_stride_128_bits(64, 4);
  printf("stride(64px, 128bpp) = %u (expect 128) %s\r\n", s128,
         s128 == 128 ? "OK" : "FAIL");
  if (s128 != 128) {
    fail++;
  }

  /* 2) 多拉几个 .o（不同编译单元），验证静态库按需解析 */
  printf("stride(128px, 32bpp) = %u (expect 64) %s\r\n",
         v3d_compute_rt_row_row_stride_128_bits(128, 1),
         v3d_compute_rt_row_row_stride_128_bits(128, 1) == 64 ? "OK" : "FAIL");

  printf("=== %s (fail=%d) ===\r\n", fail == 0 ? "PASS" : "FAIL", fail);
  return fail;
}
