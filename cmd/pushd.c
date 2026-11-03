/*
 * pushd —— 极简 HTTP 文件服务器（PUT 上传 / GET 下载）
 *
 *   curl -T file http://192.168.1.250:9000/bin/sshd
 *   curl       http://192.168.1.250:9000/etc/sshd.log
 *
 * 【关键】socket 的 read 在接收队列为空时返回 EAGAIN，不会像 pipe 那样
 * 挂起线程。所以每次 read 之前必须 poll 等可读，否则会退化成
 * "read→EAGAIN→usleep→read" 的死循环（实测跑出 96600 次调用、
 * CPU 12%、连接永远处理不完）。
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PUSHD_PORT 9000
#define HDR_MAX    4096
#define IO_TIMEOUT_MS 5000

static int wait_readable(int fd, int ms) {
    struct pollfd pfd;
    int r;
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    r = poll(&pfd, 1, ms);
    return (r > 0 && (pfd.revents & POLLIN)) ? 0 : -1;
}

static int write_full(int fd, const char *b, int n) {
    int put = 0;
    while (put < n) {
        int r = (int)write(fd, b + put, (size_t)(n - put));
        if (r <= 0) {
            if (r < 0 && (errno == EINTR || errno == EAGAIN)) {
                usleep(1000);
                continue;
            }
            return -1;
        }
        put += r;
    }
    return 0;
}

/* 手写大小写不敏感的子串查找（不依赖 strcasestr） */
static const char *find_ci(const char *hay, const char *needle) {
    const char *p = hay;
    while (*p != '\0') {
        const char *a = p;
        const char *b = needle;
        while (*a != '\0' && *b != '\0') {
            char ca = *a;
            char cb = *b;
            if (ca >= 'A' && ca <= 'Z') {
                ca = (char)(ca - 'A' + 'a');
            }
            if (cb >= 'A' && cb <= 'Z') {
                cb = (char)(cb - 'A' + 'a');
            }
            if (ca != cb) {
                break;
            }
            a++;
            b++;
        }
        if (*b == '\0') {
            return p;
        }
        p++;
    }
    return NULL;
}

static int find_content_length(const char *hdr) {
    const char *p = find_ci(hdr, "content-length:");
    if (p == NULL) {
        return -1;
    }
    return atoi(p + 15);
}

/* 读请求头（到空行为止）。返回读取长度，<0 表示失败 */
static int read_header(int fd, char *hdr, int max) {
    int got = 0;
    hdr[0] = '\0';
    while (got < max - 1) {
        int want = max - 1 - got;
        int r;
        if (wait_readable(fd, IO_TIMEOUT_MS) != 0) {
            return -1;
        }
        if (want > 512) {
            want = 512;
        }
        r = (int)read(fd, hdr + got, (size_t)want);
        if (r < 0) {
            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }
            return -1;
        }
        if (r == 0) {
            break;
        }
        got += r;
        hdr[got] = '\0';
        if (strstr(hdr, "\r\n\r\n") != NULL || strstr(hdr, "\n\n") != NULL) {
            break;
        }
    }
    return got;
}

static void reply_status(int fd, const char *status, const char *body) {
    char buf[256];
    int n = snprintf(buf, sizeof(buf),
                     "HTTP/1.1 %s\r\nContent-Length: %d\r\n"
                     "Connection: close\r\n\r\n%s",
                     status, body ? (int)strlen(body) : 0,
                     body ? body : "");
    write_full(fd, buf, n);
}

static void do_put(int fd, const char *path, char *hdr, int hlen) {
    char tmp[256];
    char buf[1024];
    char err[128];
    int out;
    int content_len = find_content_length(hdr);
    int total = 0;
    int body_in_hdr = 0;
    const char *sep = strstr(hdr, "\r\n\r\n");
    int sep_len = 4;

    if (sep == NULL) {
        sep = strstr(hdr, "\n\n");
        sep_len = 2;
    }
    if (sep != NULL) {
        body_in_hdr = hlen - (int)(sep - hdr) - sep_len;
    }

    printf("pushd: PUT %s clen=%d body_in_hdr=%d\n", path, content_len,
           body_in_hdr);

    snprintf(tmp, sizeof(tmp), "%s.new", path);
    out = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (out < 0) {
        snprintf(err, sizeof(err), "open %s: %s", tmp, strerror(errno));
        printf("pushd: %s\n", err);
        reply_status(fd, "500 Cannot Open", err);
        return;
    }

    if (body_in_hdr > 0) {
        const char *b = sep + sep_len;
        if (write_full(out, b, body_in_hdr) == 0) {
            total += body_in_hdr;
        }
    }

    while (content_len <= 0 || total < content_len) {
        int want = (int)sizeof(buf);
        int r;
        if (content_len > 0 && want > content_len - total) {
            want = content_len - total;
        }
        if (want <= 0) {
            break;
        }
        if (wait_readable(fd, IO_TIMEOUT_MS) != 0) {
            break;                  /* 超时：认为结束 */
        }
        r = (int)read(fd, buf, (size_t)want);
        if (r < 0) {
            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }
            break;
        }
        if (r == 0) {
            break;
        }
        if (write_full(out, buf, r) < 0) {
            break;
        }
        total += r;
    }
    close(out);

    if (content_len > 0 && total != content_len) {
        unlink(tmp);
        snprintf(err, sizeof(err), "short %d/%d", total, content_len);
        printf("pushd: %s\n", err);
        reply_status(fd, "500 Short Body", err);
        return;
    }
    if (total == 0) {
        unlink(tmp);
        reply_status(fd, "500 Empty Body", "no data");
        return;
    }

    unlink(path);
    if (rename(tmp, path) != 0) {
        snprintf(err, sizeof(err), "rename: %s", strerror(errno));
        printf("pushd: %s\n", err);
        reply_status(fd, "500 Rename Failed", err);
        return;
    }
    printf("pushd: wrote %s (%d bytes)\n", path, total);
    reply_status(fd, "200 OK", "OK\n");
}

static void do_get(int fd, const char *path) {
    char buf[1024];
    int in = open(path, O_RDONLY);
    int total = 0;
    char head[128];
    int n;

    if (in < 0) {
        reply_status(fd, "404 Not Found", "no such file");
        return;
    }
    n = snprintf(head, sizeof(head),
                 "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n");
    write_full(fd, head, n);
    for (;;) {
        int r = (int)read(in, buf, sizeof(buf));
        if (r <= 0) {
            break;
        }
        if (write_full(fd, buf, r) < 0) {
            break;
        }
        total += r;
    }
    close(in);
    printf("pushd: sent %s (%d bytes)\n", path, total);
}

static void handle(int fd) {
    static char hdr[HDR_MAX + 1];
    int hlen = read_header(fd, hdr, sizeof(hdr));
    char *path;
    char *p;

    if (hlen <= 0) {
        printf("pushd: header read failed\n");
        reply_status(fd, "400 Bad Request", "no header");
        return;
    }
    printf("pushd: hdr %d: %.32s\n", hlen, hdr);

    if (strncmp(hdr, "PUT ", 4) == 0) {
        path = hdr + 4;
        p = strchr(path, ' ');
        if (p != NULL) {
            *p = '\0';
        }
        do_put(fd, path, hdr, hlen);
    } else if (strncmp(hdr, "GET ", 4) == 0) {
        path = hdr + 4;
        p = strchr(path, ' ');
        if (p != NULL) {
            *p = '\0';
        }
        do_get(fd, path);
    } else {
        reply_status(fd, "405 Method Not Allowed", "PUT or GET");
    }
}

int main(void) {
    int srv;
    int on = 1;
    struct sockaddr_in addr;
    int pid;

    printf("pushd: starting on port %d\n", PUSHD_PORT);

    srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) {
        perror("pushd: socket");
        return 1;
    }
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PUSHD_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("pushd: bind");
        close(srv);
        return 1;
    }
    if (listen(srv, 4) < 0) {
        perror("pushd: listen");
        close(srv);
        return 1;
    }
    printf("pushd: ready  curl -T <file> http://<ip>:%d/<path>\n", PUSHD_PORT);

    pid = fork();
    if (pid < 0) {
        perror("pushd: fork");
        close(srv);
        return 1;
    }
    if (pid > 0) {
        printf("pushd: background pid=%d\n", pid);
        close(srv);
        return 0;
    }

    for (;;) {
        struct sockaddr_in peer;
        socklen_t plen = sizeof(peer);
        int c;
        if (wait_readable(srv, 1000) != 0) {
            continue;               /* 没有新连接，继续等 */
        }
        c = accept(srv, (struct sockaddr *)&peer, &plen);
        if (c < 0) {
            continue;
        }
        handle(c);
        close(c);
    }
    return 0;
}
