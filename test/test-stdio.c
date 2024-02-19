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

int main(int argc, char *argv[]) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_stdio),
      cmocka_unit_test(test_open),
      cmocka_unit_test(test_float),

  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}