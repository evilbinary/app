/* 【临时复现程序】FAT mkdir/chdir 问题排查用，排查完删除。 */
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

int main(void) {
  const char *d = "/tmp/LTP_tstXYZ123";
  int r = mkdir(d, 0777);
  printf("mkdir %s -> %d errno=%d(%s)\n", d, r, errno, strerror(errno));
  r = chdir(d);
  printf("chdir -> %d errno=%d(%s)\n", r, errno, strerror(errno));
  char buf[128];
  char *g = getcwd(buf, sizeof(buf));
  printf("getcwd -> %s\n", g ? g : "NULL");
  r = rmdir(d);
  printf("rmdir -> %d errno=%d\n", r, errno);
  r = chdir(d);
  printf("chdir after rmdir -> %d errno=%d\n", r, errno);
  return 0;
}
