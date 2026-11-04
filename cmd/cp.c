// cp —— 设备内文件复制。
// 用途：内核的 rename 目前坏着（根节点 fat_path 为空，fat_volume_path
// 返回 -1），devd 的「写 .new 再改名」走不通。而 open/write 是好的，
// 所以用本程序在设备内部把新内核直接拷到目标路径 —— 全程不经过网络，
// 不存在「传到一半断开」把 FAT 写坏的风险。
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

int main(int argc, char **argv) {
    int in, out;
    char buf[4096];
    int n;
    long total = 0;

    if (argc != 3) {
        fprintf(stderr, "usage: cp <src> <dst>\n");
        return 1;
    }
    in = open(argv[1], O_RDONLY);
    if (in < 0) {
        fprintf(stderr, "cp: open %s: %s\n", argv[1], strerror(errno));
        return 1;
    }
    out = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (out < 0) {
        fprintf(stderr, "cp: create %s: %s\n", argv[2], strerror(errno));
        close(in);
        return 1;
    }
    for (;;) {
        n = (int)read(in, buf, sizeof(buf));
        if (n == 0) {
            break;
        }
        if (n < 0) {
            fprintf(stderr, "cp: read: %s\n", strerror(errno));
            close(in);
            close(out);
            return 1;
        }
        if (write(out, buf, (size_t)n) != n) {
            fprintf(stderr, "cp: write: %s\n", strerror(errno));
            close(in);
            close(out);
            return 1;
        }
        total += n;
    }
    close(in);
    close(out);
    printf("cp: %s -> %s (%ld bytes)\n", argv[1], argv[2], total);
    return 0;
}
