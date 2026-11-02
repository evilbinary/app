/* test-v3d.c - Mesa v3d 编译器链验证（初始化 → NIR → QPU）
 *
 * 阶段 1：v3d_compiler_init            （已验证通过）
 * 阶段 2：手工构造 NIR 顶点 shader → v3d_compile → QPU words
 *
 * 目的：证明「Mesa 编 shader」这条路在 YiYiYa/BCM2712 上可行，
 * 之后把 QPU 二进制塞进 shader record 接回 3D 管线。
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "broadcom/compiler/v3d_compiler.h"
#include "broadcom/common/v3d_device_info.h"
#include "broadcom/common/v3d_util.h"
#include "compiler/nir/nir_builder.h"
#include "compiler/glsl_types.h"

static void comp_dbg(const char* msg, void* data) {
  (void)data;
  printf("%s", msg);
  fflush(stdout);
}

int main(void) {
  int fail = 0;

  printf("=== test-v3d (Mesa v3d 编译器链) ===\r\n");

  /* 【必须】GLSL 类型系统是单例：不初始化的话 glsl_vec4_type() 等返回
   * 未构造的类型，nir_variable_create 之后一访问就崩（实测 FAR=0x29）。 */
  glsl_type_singleton_init_or_ref();

  /* BCM2712 / V3D 7.1 的设备参数（与 gpud 探测一致：ver=71 iprev=10） */
  static struct v3d_device_info dev;
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

  uint32_t stride = v3d_compute_rt_row_row_stride_128_bits(64, 1);
  printf("stride(64,1) = %u (expect 32) %s\r\n", stride,
         stride == 32 ? "OK" : "FAIL");
  if (stride != 32) {
    fail++;
  }

  /* ---------- 构造一个最小顶点 shader ----------
   * gl_Position = vec4(0.0, 0.0, 0.0, 1.0)
   * 只验证编译链路，不涉及属性读取/varying。 */
  const struct glsl_type* vec4 = glsl_vec4_type();
  printf("glsl_vec4_type = %p %s\r\n", (void*)vec4, vec4 ? "OK" : "NULL");
  if (vec4 == NULL) {
    fail++;
    goto out;
  }

  /* 【必须】编译器处处访问 shader->options（Mesa 里由 pipe_screen 提供）。
   * 这里先给一份 zero-init 的：只为验证链路不崩，正确性配置后补
   * （v3d_screen.c:544 有一整套 lower_* 开关）。 */
  static nir_shader_compiler_options opts;

  nir_builder b;
  memset(&b, 0, sizeof(b));
  b = nir_builder_init_simple_shader(MESA_SHADER_VERTEX, &opts, "test-vs");
  printf("nir builder shader = %p\r\n", (void*)b.shader);
  if (b.shader == NULL) {
    fail++;
    goto out;
  }

  nir_def* pos = nir_imm_vec4(&b, 0.0f, 0.0f, 0.0f, 1.0f);
  printf("imm_vec4 = %p\r\n", (void*)pos);

  nir_variable* out_var = nir_variable_create(b.shader, nir_var_shader_out,
                                              vec4, "gl_Position");
  printf("out_var = %p\r\n", (void*)out_var);
  if (out_var == NULL) {
    fail++;
    goto out;
  }
  out_var->data.location = VARYING_SLOT_POS;
  nir_store_var(&b, out_var, pos, 0xf /* xyzw */);

  printf("nir shader built (outputs=%u)\r\n", b.shader->num_outputs);

  /* ---------- 编译到 QPU ---------- */
  struct v3d_key key;
  memset(&key, 0, sizeof(key));
  key.is_last_geometry_stage = true;

  struct v3d_prog_data* prog_data = NULL;
  uint32_t asm_size = 0;
  /* debug 回调：编译器把各阶段 dump 送进来，崩之前能看到走到哪一步 */
  uint64_t* words = v3d_compile(c, &key, &prog_data, b.shader, comp_dbg, NULL,
                                0 /* program_id */, 0 /* variant_id */,
                                &asm_size);

  printf("v3d_compile: words=%p size=%u bytes (%u 条 QPU)\r\n", (void*)words,
         asm_size, asm_size / 8);

  if (words != NULL && asm_size > 0) {
    uint32_t n = asm_size / 8;
    uint32_t show = n < 6 ? n : 6;
    for (uint32_t i = 0; i < show; i++) {
      printf("  qpu[%u] = 0x%016llx\r\n", i, (unsigned long long)words[i]);
    }
    printf("v3d_compile OK\r\n");
  } else {
    printf("v3d_compile FAIL\r\n");
    fail++;
  }

  if (prog_data != NULL) {
    printf("prog_data ok\r\n");
  }

out:
  if (b.shader != NULL) {
    ralloc_free(b.shader);
  }
  v3d_compiler_free(c);
  glsl_type_singleton_decref();

  printf("=== %s (fail=%d) ===\r\n", fail == 0 ? "PASS" : "FAIL", fail);
  return fail;
}
