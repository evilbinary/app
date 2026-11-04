// test-tcpd —— 最小 TCP server，用来独立验证 lwIP 的 accept/recv 链路。
// 背景：sshd 的 banner 交换 read 返回 ENOTCONN(-107)，但 privsep 的 AF_UNIX
// 通道完全正常。需要确认「accept 出来的 TCP socket 到底能不能读」，
// 不掺 SSH 的任何逻辑。
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
    printf("[TCPD] socket=%d\n", s);
    if (s < 0) {
        printf("[TCPD] socket failed errno=%d\n", errno);
        return 1;
    }
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(9999);
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, (struct sockaddr *)&a, sizeof(a)) < 0) {
        printf("[TCPD] bind failed errno=%d\n", errno);
        return 1;
    }
    if (listen(s, 4) < 0) {
        printf("[TCPD] listen failed errno=%d\n", errno);
        return 1;
    }
    printf("[TCPD] listening on 9999, fd=%d\n", s);
    for (;;) {
        struct sockaddr_in c;
        socklen_t cl = sizeof(c);
        int fd;
        char buf[64];
        int n;

        memset(&c, 0, sizeof(c));
        fd = accept(s, (struct sockaddr *)&c, &cl);
        printf("[TCPD] accept=%d errno=%d\n", fd, fd < 0 ? errno : 0);
        if (fd < 0) {
            continue;
        }
        n = (int)read(fd, buf, sizeof(buf) - 1);
        printf("[TCPD] read=%d errno=%d\n", n, n < 0 ? errno : 0);
        if (n > 0) {
            buf[n] = '\0';
            printf("[TCPD] data=[%s]\n", buf);
        }
        (void)!write(fd, "hello\n", 6);
        close(fd);
        printf("[TCPD] closed\n");
    }
}
