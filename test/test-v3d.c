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
  /* 【关键】7.1 移除了 accumulator —— Mesa 的定义是
   *   devinfo->has_accumulators = devinfo->ver < 71;
   * （v3d_device_info.c:76）。设成 true 会让 RA 走进 accumulator 分支，
   * 而 implicit_rf_nodes[] 在那种情况下从不初始化，某些指令的隐式
   * 写会读到栈垃圾当节点号（实测 n2=0x71035F6C）。 */
  dev.has_accumulators = false;
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

  /* 【必须】编译器处处访问 shader->options。这里是 v3d_screen.c:552
   * 那份真实配置（Mesa 验证过）：zero-init 会让 VIR 留下 V3D 不支持的
   * 指令形态，寄存器分配断言失败（ra_add_node_interference）。 */
  static nir_shader_compiler_options opts = {
      .compact_arrays = true,
      .lower_uadd_sat = true,
      .lower_usub_sat = true,
      .lower_iadd_sat = true,
      .lower_extract_byte = true,
      .lower_extract_word = true,
      .lower_insert_byte = true,
      .lower_insert_word = true,
      .lower_bitfield_insert = true,
      .lower_bitfield_extract = true,
      .lower_bitfield_extract16 = true,
      .lower_bitfield_extract8 = true,
      .lower_bitfield_reverse = true,
      .lower_bit_count = true,
      .lower_cs_local_id_to_index = true,
      .lower_ffract = true,
      .lower_fmod = true,
      .lower_pack_unorm_2x16 = true,
      .lower_pack_snorm_2x16 = true,
      .lower_pack_unorm_4x8 = true,
      .lower_pack_snorm_4x8 = true,
      .lower_unpack_unorm_4x8 = true,
      .lower_unpack_snorm_4x8 = true,
      .lower_pack_half_2x16 = true,
      .lower_unpack_half_2x16 = true,
      .lower_pack_32_2x16 = true,
      .lower_pack_32_2x16_split = true,
      .lower_unpack_32_2x16_split = true,
      .lower_fdiv = true,
      .lower_find_lsb = true,
      .lower_flrp32 = true,
      .lower_fpow = true,
      .lower_fsqrt = true,
      .lower_ifind_msb = true,
      .lower_isign = true,
      .lower_hadd = true,
      .lower_fisnormal = true,
      .lower_mul_high = true,
      .lower_wpos_pntc = true,
      .lower_to_scalar = true,       /* V3D 是标量后端，必须标量化 */
      .lower_interpolate_at = true,
      .lower_int64_options =
          nir_lower_bcsel64 | nir_lower_bit_count64 | nir_lower_conv64 |
          nir_lower_divmod64 | nir_lower_iabs64 | nir_lower_iadd64 |
          nir_lower_icmp64 | nir_lower_imul_2x32_64 | nir_lower_imul64 |
          nir_lower_ineg64 | nir_lower_logic64 | nir_lower_minmax64 |
          nir_lower_shift64 | nir_lower_ufind_msb64,
      .lower_fquantize2f16 = true,
      .lower_ufind_msb = true,
      .has_fsub = true,
      .has_isub = true,
      .has_imul24 = true,
      .has_umul24 = true,
      .has_uclz = true,
      .divergence_analysis_options =
          nir_divergence_multiple_workgroup_per_compute_subgroup,
      .max_unroll_iterations = 16,
      .max_samples = 4,
      .force_indirect_unrolling_sampler = true,
      .scalarize_ddx = true,
      .max_varying_expression_cost = 4,
  };

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

  /* 【必须】给 IO 变量分配驱动位置。Mesa 里这步由 st/mesa 在交给
   * 驱动前完成（nir_assign_io_var_locations）。不做的话：
   *   shader->num_outputs 仍是 0、var->data.driver_location 未定义，
   * v3d 的 nir_to_vir 用它算 VPM 输出槽时会得到垃圾偏移，进而让
   * 寄存器分配器收到越界的节点索引（实测 ra_add_node_interference
   * 断言 n1/n2 < g->count 失败）。 */
  nir_assign_io_var_locations(b.shader, nir_var_shader_in);
  nir_assign_io_var_locations(b.shader, nir_var_shader_out);
  printf("io locations assigned: outputs=%u inputs=%u\r\n",
         b.shader->num_outputs, b.shader->num_inputs);

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
