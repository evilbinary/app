/* test-v3d.c - 验证 Mesa 的 v3d 编译器库能被链接并初始化
 *
 * 这一版只做「编译器初始化/释放」，目的不是功能，而是：
 *   1. 把 nir + vir + qpu 的整条 .o 链拉进最终二进制
 *   2. 一次性暴露所有缺符号（util/auxiliary 等还没编的部分）
 * 下一步再往「构造 NIR → v3d_compile → QPU words」推进。
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "broadcom/compiler/v3d_compiler.h"
#include "broadcom/common/v3d_device_info.h"
#include "broadcom/common/v3d_util.h"

int main(void) {
  printf("=== test-v3d (Mesa v3d 编译器初始化) ===\r\n");

  /* BCM2712 / V3D 7.1 的设备参数（与 gpud 探测结果一致：
   * ver=7 ncores=1 iprev=10；每核 4 slice × 3 QPU = 12） */
  struct v3d_device_info dev;
  memset(&dev, 0, sizeof(dev));
  dev.ver = 71;
  dev.rev = 1;
  dev.compat_rev = 10;
  dev.max_perfcnt = 0;
  dev.vpm_size = 16 * 1024;
  dev.qpu_count = 12;
  dev.has_accumulators = true;
  dev.clipper_xy_granularity = 1.0f;
  dev.cle_readahead = 256;
  dev.page_size = 4096;
  dev.max_framebuffer_size = 16384;
  dev.max_render_targets = 8;

  const struct v3d_compiler* c = v3d_compiler_init(&dev, 1);
  printf("compiler init = %p %s\r\n", (void*)c, c ? "OK" : "FAIL");
  if (c == NULL) {
    return 1;
  }

  /* 顺带验证 util 数学函数（util 也在这条链里） */
  uint32_t stride = v3d_compute_rt_row_row_stride_128_bits(64, 1);
  printf("stride(64,1) = %u (expect 32) %s\r\n", stride,
         stride == 32 ? "OK" : "FAIL");

  v3d_compiler_free(c);
  printf("=== PASS ===\r\n");
  return 0;
}
