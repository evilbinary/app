// mkdir —— 最小目录创建工具。
// 起因：LTP 的 tst_tmpdir 要 mkdtemp("/tmp/LTP_socXXXXXX")，但根目录下
// 根本没有 /tmp，mkdir 的父目录找不到直接返回 -1（musl 翻成 EPERM），
// 所有 socket/tmpdir 用例都 BROK 在这一步。
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>

int main(int argc, char **argv) {
    int i;
    if (argc < 2) {
        printf("usage: mkdir <dir> [dir...]\n");
        return 1;
    }
    for (i = 1; i < argc; i++) {
        if (mkdir(argv[i], 0755) < 0) {
            printf("mkdir: %s failed errno=%d\n", argv[i], errno);
            return 1;
        }
        printf("created %s\n", argv[i]);
    }
    return 0;
}
