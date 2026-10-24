/* rm：删除文件（FAT 上底层 f_unlink 也能删空目录，与 Linux 的 rm 对空目录
 * 行为差别仅在于这里不会先问）。内核不替用户决定"该删什么"，清理动作交给
 * 命令本身：例如 X server 异常退出留下的 /tmp/.X0-lock，直接
 *   rm /tmp/.X0-lock
 * 比在文件系统初始化里硬编码一堆特例干净得多。 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  int force = 0;
  int rc = 0;
  int i = 1;

  if (i < argc && (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--force") == 0)) {
    force = 1;
    i++;
  }
  if (i >= argc) {
    fprintf(stderr, "usage: rm [-f] file...\n");
    return 1;
  }
  for (; i < argc; i++) {
    if (unlink(argv[i]) != 0) {
      /* -f 语义：文件不存在不算失败 */
      if (!force) {
        fprintf(stderr, "rm: cannot remove '%s'\n", argv[i]);
        rc = 1;
      }
    }
  }
  return rc;
}
