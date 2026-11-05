// test-exec-sock —— 隔离「fork + exec 之后还能不能读连接」。
//
// 背景：
//   test-tcpd       单进程 accept+read       → 正常
//   test-fork-sock  父 accept → fork → 子读  → 正常（客户端收到 child-ok）
//   sshd            fork + exec 后读          → read 返回 ENOTCONN
// 唯一的差异就是 exec。本程序精确复现那条链路：
//
//   test-exec-sock server   ← 监听 9997，accept 后 fork，子进程把连接
//                             dup2 到 0/1 再 execv 自己（reader 模式）
//   test-exec-sock reader   ← 从 fd 0 读，结果写 stderr（进 run.err）
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static int reader_mode(void) {
    char buf[128];
    int n;

    fprintf(stderr, "[EXEC-R] reader started, reading fd 0...\n");
    fflush(stderr);
    n = (int)read(0, buf, sizeof(buf) - 1);
    fprintf(stderr, "[EXEC-R] read=%d errno=%d\n", n, n < 0 ? errno : 0);
    fflush(stderr);
    if (n > 0) {
        buf[n] = '\0';
        fprintf(stderr, "[EXEC-R] data=[%s]\n", buf);
        fflush(stderr);
        (void)!write(1, "exec-reader-ok\n", 15);
    }
    return n > 0 ? 0 : 1;
}

int main(int argc, char **argv) {
    int s;
    struct sockaddr_in a;
    int on = 1;

    if (argc > 1 && argv[1][0] == 'r') {
        return reader_mode();
    }

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        fprintf(stderr, "[EXEC] socket failed errno=%d\n", errno);
        return 1;
    }
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(9997);
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, (struct sockaddr *)&a, sizeof(a)) < 0 ||
        listen(s, 4) < 0) {
        fprintf(stderr, "[EXEC] bind/listen failed errno=%d\n", errno);
        return 1;
    }
    fprintf(stderr, "[EXEC] listening on 9997, fd=%d\n", s);
    fflush(stderr);

    for (;;) {
        struct sockaddr_in c;
        socklen_t cl = sizeof(c);
        int fd;
        int pid;

        memset(&c, 0, sizeof(c));
        fd = accept(s, (struct sockaddr *)&c, &cl);
        if (fd < 0) {
            continue;
        }
        fprintf(stderr, "[EXEC] accept=%d\n", fd);
        fflush(stderr);

        pid = fork();
        if (pid == 0) {
            /* 子进程：把连接挪到 0/1，再 exec 自己（reader 模式）—— 与 sshd 完全同构 */
            close(s);
            dup2(fd, 0);
            dup2(fd, 1);
            if (fd > 1) {
                close(fd);
            }
            {
                char *av[3];
                av[0] = (char *)"/bin/test-exec-sock";
                av[1] = (char *)"reader";
                av[2] = NULL;
                execv("/bin/test-exec-sock", av);
            }
            fprintf(stderr, "[EXEC] execv failed errno=%d\n", errno);
            _exit(1);
        }
        close(fd);
        fprintf(stderr, "[EXEC] forked pid=%d, parent closed fd=%d\n", pid, fd);
        fflush(stderr);
    }
}
