/* 【临时复现程序】FAT mkdir/chdir/file 生命周期排查用，排查完删除。 */
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

int main(void) {
  const char *d = "/tmp/LTP_tstXYZ123";
  char fbuf[128];
  snprintf(fbuf, sizeof(fbuf), "%s/ipcfile", d);
  int r = mkdir(d, 0777);
  printf("mkdir -> %d\n", r);
  r = chdir(d);
  printf("chdir -> %d\n", r);

  int fd = open(fbuf, O_CREAT | O_EXCL | O_RDWR, 0600);
  printf("open crea -> %d errno=%d\n", fd, errno);
  if (fd < 0) return 1;
  int t = ftruncate(fd, 4096);
  printf("ftruncate -> %d errno=%d\n", t, errno);
  void *m = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  printf("mmap -> %p\n", m);
  if (m != MAP_FAILED) {
    ((int *)m)[0] = 1234;
    munmap(m, 4096);
  }
  close(fd);

  fd = open(fbuf, O_RDWR);
  printf("reopen -> %d errno=%d\n", fd, errno);
  if (fd >= 0) {
    m = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    printf("mmap2 -> %p first=%d\n", m, m != MAP_FAILED ? ((int *)m)[0] : -1);
    if (m != MAP_FAILED) munmap(m, 4096);
    close(fd);
  }
  r = unlink(fbuf);
  printf("unlink -> %d errno=%d\n", r, errno);
  r = chdir("/");
  printf("chdir back -> %d\n", r);
  r = rmdir(d);
  printf("rmdir -> %d errno=%d\n", r, errno);
  return 0;
}
