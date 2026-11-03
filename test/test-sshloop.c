/*
 * test-sshloop —— 在 qemu 里用 loopback 跑通 sshd 的入站路径
 *
 * 【为什么需要它】
 * sshd 的 privsep 问题（mm_get_state 的 4 字节错位）必须有一个真实客户端
 * 连上来才会触发。真机验证一次要拔卡、插卡、烧写，代价太高。
 * lwIP 已开启 LWIP_NETIF_LOOPBACK，所以 qemu 里可以 127.0.0.1 自连：
 *   1. fork + exec /bin/sshd -d -p 2222
 *   2. 父进程等 2 秒（sshd 完成 bind/listen）
 *   3. 连 127.0.0.1:2222，发 SSH 版本串
 *   4. 读服务端 banner —— 这一步就会触发 privsep_preauth + mm_get_state
 * 只要 banner 能读到，说明 SSH 握手第一层通；sshd 那边的诊断日志会同时
 * 打在 console 上，定位 privsep 错位不需要任何硬件操作。
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>

#define TEST_PORT 2222

static int connect_local(int port) {
    int fd;
    struct sockaddr_in addr;
    int i;

    for (i = 0; i < 40; i++) {           /* 最多等 4 秒 */
        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            return -1;
        }
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons((unsigned short)port);
        addr.sin_addr.s_addr = htonl(0x7f000001); /* 127.0.0.1 */
        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
            return fd;
        }
        close(fd);
        usleep(100 * 1000);
    }
    return -1;
}

int main(int argc, char **argv) {
    pid_t pid;
    int fd;
    int n;
    char buf[512];
    const char *banner = "SSH-2.0-YiYiYaTest\r\n";

    (void)argc;
    (void)argv;

    printf("sshloop: start\n");

    pid = fork();
    if (pid < 0) {
        printf("sshloop: fork failed\n");
        return 1;
    }
    if (pid == 0) {
        /* 子进程：起 sshd（调试模式，前台，指定端口） */
        execl("/bin/sshd", "/bin/sshd", "-d", "-p", "2222", (char *)NULL);
        execl("/BIN/SSHD", "/BIN/SSHD", "-d", "-p", "2222", (char *)NULL);
        printf("sshloop: exec sshd failed: %s\n", strerror(errno));
        _exit(127);
    }

    printf("sshloop: sshd pid=%d, waiting for listen...\n", (int)pid);

    fd = connect_local(TEST_PORT);
    if (fd < 0) {
        printf("sshloop: connect 127.0.0.1:%d FAILED\n", TEST_PORT);
        kill(pid, 9);
        waitpid(pid, NULL, 0);
        return 1;
    }
    printf("sshloop: connected\n");

    n = (int)write(fd, banner, strlen(banner));
    printf("sshloop: sent banner (%d bytes)\n", n);

    memset(buf, 0, sizeof(buf));
    n = (int)read(fd, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        /* 只显示第一行，避免把二进制糊到终端 */
        char *nl = strchr(buf, '\n');
        if (nl != NULL) {
            *nl = '\0';
        }
        printf("sshloop: server banner: %s\n", buf);
        printf("sshloop: READ OK (privsep path reached)\n");
    } else {
        printf("sshloop: read banner failed n=%d errno=%d\n", n, errno);
    }

    close(fd);
    /* 给 sshd 一点时间把诊断打完，然后收尸 */
    sleep(1);
    kill(pid, 15);
    waitpid(pid, NULL, 0);

    printf("sshloop: done\n");
    return 0;
}
