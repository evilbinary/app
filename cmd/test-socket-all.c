// test-socket-all —— 一次性验证内核 socket 接口的每个环节。
//
// 为什么要自己写：LTP 的框架（tst_test.c setup_ipc）要求 mmap(MAP_SHARED)
// 建立进程间共享区，而本机根文件系统是 FAT，mmap 直接返回 EPERM ——
// 所以 LTP 的所有 socket 用例都会在准备阶段 TBROK，跟 socket 实现无关。
//
// 本程序按顺序调用并打印每一步的结果，任何一步失败都能立刻定位：
//   socket → setsockopt(REUSEADDR) → bind → getsockname → listen
//   → fork 客户端 → connect → accept → getpeername
//   → send/recv 双向 → poll → shutdown(WR) → close
//
// 结果写到 stderr（重定向进 run.err），用 [SOCK] 前缀便于过滤。
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 9996

static void report(const char *what, int ok, int err) {
    fprintf(stderr, "[SOCK] %-22s %s%s%d\n", what, ok ? "OK" : "FAIL",
            ok ? "" : " errno=", ok ? 0 : err);
    fflush(stderr);
}

int main(void) {
    int s, c, fd, r, n;
    struct sockaddr_in a, peer;
    socklen_t sl;
    int on = 1;
    char buf[64];
    struct pollfd pfd;

    /* 1. socket */
    s = socket(AF_INET, SOCK_STREAM, 0);
    report("socket", s >= 0, errno);
    if (s < 0) {
        return 1;
    }

    /* 2. setsockopt */
    r = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    report("setsockopt", r == 0, errno);

    /* 3. bind */
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(PORT);
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    r = bind(s, (struct sockaddr *)&a, sizeof(a));
    report("bind", r == 0, errno);
    if (r != 0) {
        return 1;
    }

    /* 4. getsockname */
    memset(&a, 0, sizeof(a));
    sl = sizeof(a);
    r = getsockname(s, (struct sockaddr *)&a, &sl);
    report("getsockname", r == 0, errno);
    if (r == 0) {
        fprintf(stderr, "[SOCK] local port=%u\n",
                (unsigned)((a.sin_port >> 8) | ((a.sin_port & 0xff) << 8)));
        fflush(stderr);
    }

    /* 5. listen */
    r = listen(s, 4);
    report("listen", r == 0, errno);
    if (r != 0) {
        return 1;
    }
    fprintf(stderr, "[SOCK] listening on %d\n", PORT);
    fflush(stderr);

    /* 6. fork 一个客户端 */
    if (fork() == 0) {
        int cs;
        struct sockaddr_in sa;
        char msg[32] = "hello-server";

        sleep(1);
        cs = socket(AF_INET, SOCK_STREAM, 0);
        report("client socket", cs >= 0, errno);
        if (cs < 0) {
            _exit(1);
        }
        memset(&sa, 0, sizeof(sa));
        sa.sin_family = AF_INET;
        sa.sin_port = htons(PORT);
        sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        r = connect(cs, (struct sockaddr *)&sa, sizeof(sa));
        /* 非阻塞 socket：EINPROGRESS 表示"正在建立"，用 poll 等写就绪，
         * 再用 SO_ERROR 取真实结果。 */
        if (r != 0 && errno == EINPROGRESS) {
            struct pollfd cp;
            cp.fd = cs;
            cp.events = POLLOUT;
            cp.revents = 0;
            if (poll(&cp, 1, 3000) > 0) {
                int soerr = 0;
                socklen_t sl2 = sizeof(soerr);
                getsockopt(cs, SOL_SOCKET, SO_ERROR, &soerr, &sl2);
                r = (soerr == 0) ? 0 : -1;
                errno = soerr;
            }
        }
        report("connect", r == 0, errno);
        if (r == 0) {
            n = (int)write(cs, msg, strlen(msg));
            report("client send", n == (int)strlen(msg), errno);
            n = (int)read(cs, buf, sizeof(buf) - 1);
            report("client recv", n > 0, errno);
            if (n > 0) {
                buf[n] = 0;
                fprintf(stderr, "[SOCK] client got [%s]\n", buf);
                fflush(stderr);
            }
            shutdown(cs, SHUT_RDWR);
        }
        close(cs);
        _exit(0);
    }

    /* 7. accept —— 内核的 socket 一律非阻塞（避免在 mbox 上死锁），
     * 所以 EAGAIN 表示"还没有连接"，必须自己重试。 */
    fd = -1;
    errno = 0;
    for (int i = 0; i < 300; i++) {   /* 最多等 3 秒 */
        memset(&peer, 0, sizeof(peer));
        sl = sizeof(peer);
        fd = accept(s, (struct sockaddr *)&peer, &sl);
        if (fd >= 0 || errno != EAGAIN) {
            break;
        }
        usleep(10000);
    }
    report("accept", fd >= 0, errno);
    if (fd < 0) {
        return 1;
    }

    /* 8. getpeername */
    memset(&peer, 0, sizeof(peer));
    sl = sizeof(peer);
    r = getpeername(fd, (struct sockaddr *)&peer, &sl);
    report("getpeername", r == 0, errno);
    if (r == 0) {
        fprintf(stderr, "[SOCK] peer port=%u\n",
                (unsigned)((peer.sin_port >> 8) | ((peer.sin_port & 0xff) << 8)));
        fflush(stderr);
    }

    /* 9. poll（等数据到达） */
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    r = poll(&pfd, 1, 3000);
    report("poll", r > 0, errno);

    /* 10. recv */
    memset(buf, 0, sizeof(buf));
    n = (int)read(fd, buf, sizeof(buf) - 1);
    report("server recv", n > 0, errno);
    if (n > 0) {
        fprintf(stderr, "[SOCK] server got [%s]\n", buf);
        fflush(stderr);
    }

    /* 11. getpeername / send 回去 */
    n = (int)write(fd, "hello-client", 12);
    report("server send", n == 12, errno);

    /* 12. shutdown + close */
    r = shutdown(fd, SHUT_RDWR);
    report("shutdown", r == 0, errno);
    close(fd);
    close(s);
    fprintf(stderr, "[SOCK] ALL DONE\n");
    fflush(stderr);
    return 0;
}
