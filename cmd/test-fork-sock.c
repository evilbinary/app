// test-fork-sock —— 验证「父进程 accept 后 fork，子进程读同一个连接」。
// 背景：单进程 accept/read/write 完全正常（test-tcpd 已验证），
// 但 sshd 是父进程 accept、fork+exec 后子进程读，read 返回 ENOTCONN。
// 本测试复现这条最小链路，用来判断是不是「父进程 close 把 socket 销毁了」。
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(void) {
    int s;
    struct sockaddr_in a;
    int on = 1;

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        printf("[FS] socket failed errno=%d\n", errno);
        return 1;
    }
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(9998);
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, (struct sockaddr *)&a, sizeof(a)) < 0) {
        printf("[FS] bind failed errno=%d\n", errno);
        return 1;
    }
    if (listen(s, 4) < 0) {
        printf("[FS] listen failed errno=%d\n", errno);
        return 1;
    }
    printf("[FS] listening on 9998, fd=%d\n", s);

    for (;;) {
        struct sockaddr_in c;
        socklen_t cl = sizeof(c);
        int fd;
        int pid;
        char buf[64];
        int n;

        memset(&c, 0, sizeof(c));
        fd = accept(s, (struct sockaddr *)&c, &cl);
        if (fd < 0) {
            continue;
        }
        printf("[FS] accept=%d\n", fd);

        pid = fork();
        if (pid == 0) {
            /* 子进程：关掉监听，读连接 —— 这一步等价于 sshd-session */
            close(s);
            printf("[FS-CHILD] pid=%d fd=%d reading...\n", getpid(), fd);
            n = (int)read(fd, buf, sizeof(buf) - 1);
            printf("[FS-CHILD] read=%d errno=%d\n", n, n < 0 ? errno : 0);
            if (n > 0) {
                buf[n] = '\0';
                printf("[FS-CHILD] data=[%s]\n", buf);
                (void)!write(fd, "child-ok\n", 9);
            }
            close(fd);
            _exit(0);
        }

        /* 父进程：立刻关掉连接 fd（这是 sshd 的真实行为） */
        close(fd);
        printf("[FS] parent forked pid=%d, closed fd=%d\n", pid, fd);
    }
}
