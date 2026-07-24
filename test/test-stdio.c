#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmocka.h"

void exit_function() { printf("exit function\n"); }

void test_stdio(void **state) {
  FILE *fp;
  fp = fopen("/who.txt", "w+");
  assert_non_null(fp);
  int ret = fprintf(fp, "%s %s %s %d", "who", "are", "you", 2022);
  assert_true(ret);
  fclose(fp);

  fp = fopen("/who.txt", "r");
  assert_non_null(fp);
  fseek(fp, 0, SEEK_END);
  int len = ftell(fp);
  fclose(fp);
  assert_int_equal(len, 16);

  fp = fopen("who.txt", "r");
  assert_non_null(fp);

  char c = fgetc(fp);
  assert_true(c == 'w');
  assert_false(feof(fp));

  c = fgetc(fp);
  assert_true(c == 'h');
  assert_false(feof(fp));

  c = fgetc(fp);
  assert_true(c == 'o');
  assert_false(feof(fp));

  c = fgetc(fp);
  assert_true(c == ' ');
  assert_false(feof(fp));

  c = fgetc(fp);
  assert_true(c == 'a');
  assert_false(feof(fp));

  c = fgetc(fp);
  assert_true(c == 'r');
  assert_false(feof(fp));

  c = fgetc(fp);
  assert_true(c == 'e');
  assert_false(feof(fp));

  fclose(fp);

  fp = fopen("/fputs.txt", "w+");
  assert_non_null(fp);
  fputc('E', fp);
  fputc('V', fp);
  fputc('I', fp);
  fputc('L', fp);
  fclose(fp);

  fp = fopen("/fputs.txt", "r");
  assert_non_null(fp);

  c = fgetc(fp);
  assert_true(c == 'E');
  assert_false(feof(fp));

  c = fgetc(fp);
  assert_true(c == 'V');
  assert_false(feof(fp));

  c = fgetc(fp);
  assert_true(c == 'I');
  assert_false(feof(fp));

  c = fgetc(fp);
  assert_true(c == 'L');
  assert_false(feof(fp));

  c = fgetc(fp);
  assert_true(feof(fp));

  fpos_t position;
  fp = fopen("/file-pos.txt", "w+");
  fgetpos(fp, &position);
  fputs("Hello, World!", fp);

  fsetpos(fp, &position);
  fputs("hahaha evil gaga", fp);
  fclose(fp);

  char str[64];
  fp = fopen("/file-pos.txt", "r");
  fgets(str, 64, fp);
  assert_string_equal(str, "hahaha evil gaga");

  fclose(fp);
}

void test_open() {
  int fd = open("/dev/fb", O_RDWR);
  printf("screen init fd:%d\n", fd);
}

void test_float() {
  float a = 10.0;
  float b = a / 2.0;
  assert_true(b == 5.0);
}

void test_getline() {
  FILE *file;
  int chr;
  int count;
  int size = 0;
  char *buff = NULL;
  int read = 0;
  file = fopen("/conf/init.conf", "r");
  getline(&buff, &size, file);

  assert_true(size == 11);
}


// 测试用例函数
static void test_read_char_from_stdin(void **state) {
    (void) state; // 忽略未使用的参数

    // 创建临时文件并写入测试数据
    FILE *temp_file = tmpfile();
    assert_non_null(temp_file);
    fputc('B', temp_file);
    rewind(temp_file);

    // 重定向 stdin 到临时文件
    int stdin_fd = dup(fileno(stdin));
    assert_true(stdin_fd >= 0);
    assert_true(freopen(NULL, "r", stdin) != NULL);
    dup2(fileno(temp_file), fileno(stdin));

    // 调用被测试函数
    char result = fgetc(stdin);

    // 恢复 stdin
    fflush(stdin);
    dup2(stdin_fd, fileno(stdin));
    close(stdin_fd);
    fclose(temp_file);

    // 验证结果
    assert_int_equal(result, 'B');
}

static void test_read_char_from_stdin2(void **state) {
    (void) state; // 忽略未使用的参数

  char result = fgetc(stdin);
  printf("result=%c %x\n",result,result);
}

int main(int argc, char *argv[]) {
  const struct CMUnitTest tests[] = {
      // cmocka_unit_test(test_stdio),
      // cmocka_unit_test(test_open),
      // cmocka_unit_test(test_float),
      // cmocka_unit_test(test_read_char_from_stdin),
      cmocka_unit_test(test_read_char_from_stdin2),

  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}