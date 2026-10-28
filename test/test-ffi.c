/* libffi 移植验收：ffi_prep_cif + ffi_call 基本正确性。
 * 覆盖 wayland 用到的形态：void* 参数 + int32 参数、void 返回。
 * 顺便打印 cif.bytes（异常大说明 prep_cif 的 ABI 计算有问题）。 */
#include <ffi.h>
#include <stdio.h>
#include <string.h>

static int g_sum;
static int g_data_ok;

static void cb_ptr2(void* data, void* obj, int a, int b) {
  (void)obj;
  g_sum = a + b;
  g_data_ok = (data == (void*)0x1234);
}

static int add3(int a, int b, int c) { return a + b + c; }

static int run_ptr2(void) {
  ffi_cif cif;
  ffi_type* types[4];
  void* data = (void*)0x1234;
  void* obj = (void*)0x5678;
  int a = 7;
  int b = 35;
  void* vals[4];
  types[0] = &ffi_type_pointer;
  types[1] = &ffi_type_pointer;
  types[2] = &ffi_type_sint32;
  types[3] = &ffi_type_sint32;
  vals[0] = &data;
  vals[1] = &obj;
  vals[2] = &a;
  vals[3] = &b;
  ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 4, &ffi_type_void, types);
  printf("TINFO: cif bytes=%u nargs=%u\n", cif.bytes, cif.nargs);
  if (cif.bytes > 1024) {
    printf("TFAIL: cif.bytes 异常（%u）\n", cif.bytes);
    return 1;
  }
  ffi_call(&cif, (void (*)(void))cb_ptr2, NULL, vals);
  if (g_sum == 42 && g_data_ok) {
    printf("TPASS: ffi_call(ptr,ptr,int,int) sum=%d\n", g_sum);
    return 0;
  }
  printf("TFAIL: ffi_call 结果 sum=%d data_ok=%d\n", g_sum, g_data_ok);
  return 1;
}

static int run_add3(void) {
  ffi_cif cif;
  ffi_type* types[3];
  int a = 1;
  int b = 2;
  int c = 3;
  int r = 0;
  void* vals[3];
  types[0] = &ffi_type_sint32;
  types[1] = &ffi_type_sint32;
  types[2] = &ffi_type_sint32;
  vals[0] = &a;
  vals[1] = &b;
  vals[2] = &c;
  ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 3, &ffi_type_sint32, types);
  printf("TINFO: add3 cif bytes=%u\n", cif.bytes);
  ffi_call(&cif, (void (*)(void))add3, &r, vals);
  if (r == 6) {
    printf("TPASS: ffi_call 返回值 add3=%d\n", r);
    return 0;
  }
  printf("TFAIL: add3=%d（期望 6）\n", r);
  return 1;
}

int main(int argc, char* argv[]) {
  int fail = 0;
  (void)argc;
  (void)argv;
  printf("TINFO: sizeof(ffi_cif)=%u sizeof(ffi_type)=%u\n", (unsigned)sizeof(ffi_cif),
         (unsigned)sizeof(ffi_type));
  fail += run_ptr2();
  fail += run_add3();
  printf("TINFO: fail=%d\n", fail);
  printf("\nSummary:\n");
  return fail ? 1 : 0;
}
