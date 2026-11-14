/*
 * pushd —— 极简 HTTP 文件服务器（PUT 上传 / GET 下载）
 *
 *   curl -T file http://192.168.1.250:9000/bin/sshd
 *   curl       http://192.168.1.250:9000/etc/sshd.log
 *
 * 【不用 poll】内核的 poll 对 TCP socket 会谎报 POLLIN（返回"可读"但
 * read 又给 EAGAIN），于是 poll→read→poll 变成纯忙等，连接永远处理不完
 * （实测 ps 里 calls 是 63/73 交替、CPU 48%）。这里改成 read + usleep
 * 重试：有数据时 read 立即返回，没数据时每 5ms 试一次，连续 600 次
 * （3 秒）没有任何进展才放弃。
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* 9001：与旧版(9000)并存，便于「先起新的、再停旧的」热更新 ——
 * 端口相同的话必须先杀旧进程，而杀完之后就没有通道再起新进程了。 */
#define PUSHD_PORT 9004
#define HDR_MAX    4096
/* 读空闲超时。原来是 600（3 秒），实测推 7.7MB 时只写到 163KB 就 short：
 * pushd 写 FAT 的速度跟不上 curl 的发送，TCP 接收窗口满后对端会暂停发送，
 * 一旦暂停超过 3 秒就被误判成「没有更多数据」。放宽到 60 秒。 */
#define IDLE_MAX   12000        /* 12000 × 5ms = 60 秒 */

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

/* 按长度搜索，不依赖 NUL 终止：TCP 数据里混入 0x00 时，
 * find_ci 的 while(*p) 会提前停住，导致 Content-Length 搜不到。 */
static const char *find_ci_len(const char *hay, int hlen, const char *needle) {
    int nl = (int)strlen(needle);
    int i;
    for (i = 0; i + nl <= hlen; i++) {
        int j;
        int ok = 1;
        for (j = 0; j < nl; j++) {
            char ca = hay[i + j];
            char cb = needle[j];
            if (ca >= 'A' && ca <= 'Z') {
                ca = (char)(ca - 'A' + 'a');
            }
            if (cb >= 'A' && cb <= 'Z') {
                cb = (char)(cb - 'A' + 'a');
            }
            if (ca != cb) {
                ok = 0;
                break;
            }
        }
        if (ok) {
            return hay + i;
        }
    }
    return NULL;
}

/* 按长度查找字节串（memmem 等价）。strstr 遇到中间的 0x00 会提前停住，
 * 而 TCP 数据里确实会混入 NUL，导致 "\r\n\r\n" 找不到、body 起点算不出。 */
static const char *find_bytes(const char *hay, int hlen, const char *needle,
                              int nlen) {
    int i;
    for (i = 0; i + nlen <= hlen; i++) {
        if (memcmp(hay + i, needle, (size_t)nlen) == 0) {
            return hay + i;
        }
    }
    return NULL;
}

static int find_content_length(const char *hdr, int hlen) {
    const char *p = find_ci_len(hdr, hlen, "content-length:");
    if (p == NULL) {
        return -1;
    }
    return atoi(p + 15);
}

/* 把收到的请求头原样落到 /bin/hdr.bin，便于用 GET 拉下来看真实字节
 * （串口只能打印 40 字节，中间混了 0x00 或错位时看不出来）。 */
static void dump_hdr(const char *hdr, int hlen) {
    int f;
    /* 先删再建：FAT 上 O_TRUNC 不一定生效（实测文件保留旧内容），
     * 导致 dump 出来的是上一次请求的头。 */
    unlink("/bin/hdr.bin");
    f = open("/bin/hdr.bin", O_WRONLY | O_CREAT, 0666);
    if (f >= 0) {
        if (hlen > 0) {
            (void)write(f, hdr, (size_t)hlen);
        }
        close(f);
    }
}

/* read 封装：EAGAIN 就短睡重试，连续 idle_max 次无进展返回 0 */

/* ================= MD5（RFC 1321，自包含实现） ================= */
typedef struct {
    unsigned int state[4];
    unsigned long long count;
    unsigned char buf[64];
} md5_ctx_t;

static const unsigned int md5_k[64] = {
    0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,
    0xa8304613,0xfd469501,0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,
    0x6b901122,0xfd987193,0xa679438e,0x49b40821,0xf61e2562,0xc040b340,
    0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
    0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,
    0x676f02d9,0x8d2a4c8a,0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,
    0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,0x289b7ec6,0xeaa127fa,
    0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
    0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,
    0xffeff47d,0x85845dd1,0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,
    0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391
};
static const unsigned char md5_s[64] = {
    7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
    5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
    4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
    6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21
};
#define MD5_ROTL(x,c) (((x) << (c)) | ((x) >> (32 - (c))))

static void md5_transform(unsigned int st[4], const unsigned char blk[64]) {
    unsigned int a = st[0], b = st[1], c = st[2], d = st[3];
    unsigned int m[16];
    int i;
    for (i = 0; i < 16; i++) {
        m[i] = (unsigned int)blk[i * 4] |
               ((unsigned int)blk[i * 4 + 1] << 8) |
               ((unsigned int)blk[i * 4 + 2] << 16) |
               ((unsigned int)blk[i * 4 + 3] << 24);
    }
    for (i = 0; i < 64; i++) {
        unsigned int f, g, tmp;
        if (i < 16) {
            f = (b & c) | (~b & d);
            g = (unsigned int)i;
        } else if (i < 32) {
            f = (d & b) | (~d & c);
            g = (unsigned int)(5 * i + 1) & 15;
        } else if (i < 48) {
            f = b ^ c ^ d;
            g = (unsigned int)(3 * i + 5) & 15;
        } else {
            f = c ^ (b | ~d);
            g = (unsigned int)(7 * i) & 15;
        }
        tmp = d;
        d = c;
        c = b;
        b = b + MD5_ROTL(a + f + md5_k[i] + m[g], md5_s[i]);
        a = tmp;
    }
    st[0] += a;
    st[1] += b;
    st[2] += c;
    st[3] += d;
}

static void md5_init(md5_ctx_t *c) {
    c->state[0] = 0x67452301;
    c->state[1] = 0xefcdab89;
    c->state[2] = 0x98badcfe;
    c->state[3] = 0x10325476;
    c->count = 0;
}

static void md5_update(md5_ctx_t *c, const void *data, int len) {
    const unsigned char *p = (const unsigned char *)data;
    unsigned int used = (unsigned int)(c->count & 63);
    c->count += (unsigned long long)len;
    if (used) {
        unsigned int need = 64 - used;
        if ((unsigned int)len < need) {
            memcpy(c->buf + used, p, (size_t)len);
            return;
        }
        memcpy(c->buf + used, p, need);
        md5_transform(c->state, c->buf);
        p += need;
        len -= (int)need;
    }
    while (len >= 64) {
        md5_transform(c->state, p);
        p += 64;
        len -= 64;
    }
    if (len > 0) {
        memcpy(c->buf, p, (size_t)len);
    }
}

static void md5_final(md5_ctx_t *c, unsigned char out[16]) {
    unsigned long long bits = c->count * 8;
    unsigned int used = (unsigned int)(c->count & 63);
    unsigned int i;
    c->buf[used++] = 0x80;
    if (used > 56) {
        memset(c->buf + used, 0, 64 - used);
        md5_transform(c->state, c->buf);
        used = 0;
    }
    memset(c->buf + used, 0, 56 - used);
    for (i = 0; i < 8; i++) {
        c->buf[56 + i] = (unsigned char)(bits >> (8 * i));
    }
    md5_transform(c->state, c->buf);
    for (i = 0; i < 4; i++) {
        out[i * 4] = (unsigned char)c->state[i];
        out[i * 4 + 1] = (unsigned char)(c->state[i] >> 8);
        out[i * 4 + 2] = (unsigned char)(c->state[i] >> 16);
        out[i * 4 + 3] = (unsigned char)(c->state[i] >> 24);
    }
}

static void md5_hex(const unsigned char d[16], char out[33]) {
    static const char h[] = "0123456789abcdef";
    int i;
    for (i = 0; i < 16; i++) {
        out[i * 2] = h[d[i] >> 4];
        out[i * 2 + 1] = h[d[i] & 15];
    }
    out[32] = '\0';
}

static int read_wait(int fd, char *buf, int want, int *idle, int idle_max) {
    for (;;) {
        int r = (int)read(fd, buf, (size_t)want);
        if (r > 0) {
            *idle = 0;
            return r;
        }
        if (r == 0) {
            return 0;                   /* 真 EOF */
        }
        if (errno != EAGAIN && errno != EINTR) {
            return -1;
        }
        if (++(*idle) > idle_max) {
            return 0;                   /* 超时：当作没有更多数据 */
        }
        usleep(5000);
    }
}

static int read_header(int fd, char *hdr, int max) {
    int got = 0;
    int idle = 0;
    hdr[0] = '\0';
    while (got < max - 1) {
        int want = max - 1 - got;
        int r;
        if (want > 512) {
            want = 512;
        }
        r = read_wait(fd, hdr + got, want, &idle, IDLE_MAX);
        if (r <= 0) {
            break;
        }
        got += r;
        hdr[got] = '\0';
        if (strstr(hdr, "\r\n\r\n") != NULL || strstr(hdr, "\n\n") != NULL) {
            break;
        }
    }
    hdr[got] = '\0';
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
    /* 【16KB 静态缓冲】原来每轮只写 1KB 到 FAT，推 7.7MB 实测只有 8KB/s
     * （280 秒才发出 2.2MB）。线程栈只有 4KB，大数组必须放静态区。 */
    static char buf[16384];
    char err[160];
    char okbody[96];
    int out;
    int content_len = find_content_length(hdr, hlen);
    int total = 0;
    int body_in_hdr = 0;
    int idle = 0;
    md5_ctx_t md5;
    const char *sep = find_bytes(hdr, hlen, "\r\n\r\n", 4);
    int sep_len = 4;

    md5_init(&md5);

    if (sep == NULL) {
        sep = find_bytes(hdr, hlen, "\n\n", 2);
        sep_len = 2;
    }
    if (sep != NULL) {
        body_in_hdr = hlen - (int)(sep - hdr) - sep_len;
    }

    dump_hdr(hdr, hlen);   /* 只存 PUT 的头，避免被后续 GET 覆盖 */
    {
        /* 把 hdr 尾部 48 字节按 hex 打到串口。"\r\n\r\n" 就在尾部，
         * 如果这几个字节变成了 00，说明 lwip_recv 在跨 pbuf 拷贝时
         * 把边界处的字节写坏了（strstr/find_bytes 找不到分隔符，
         * 但 Content-Length 在前面所以能解析出来）。 */
        int s = (hlen > 48) ? (hlen - 48) : 0;
        int i;
        printf("devd: tail[%d..%d]=", s, hlen);
        for (i = s; i < hlen; i++) {
            printf("%02x", (unsigned char)hdr[i]);
        }
        printf("\n");
    }
    printf("devd: PUT %s clen=%d hdr=%d in_hdr=%d\n", path, content_len, hlen,
           body_in_hdr);

    /* 【直接写模式】带 X-Direct: 1 时跳过 tmp+rename，直接写目标文件。
     * 用途：FAT 根目录的 rename 目前是坏的（根节点 fat_path 为空，
     * fat_volume_path 直接返回 -1），而 raspi5 的 kernel8.img 就在根目录、
     * 换内核必须写它。先备份再直写，中断也不会把启动盘写废。 */
    /* 大小写不敏感：curl 发的是 "X-Direct: 1" */
    if (find_ci_len(hdr, hlen, "x-direct: 1") != NULL) {
        snprintf(tmp, sizeof(tmp), "%s", path);
        printf("devd: DIRECT write %s\n", tmp);
    } else {
        snprintf(tmp, sizeof(tmp), "%s.new", path);
    }
    out = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (out < 0) {
        snprintf(err, sizeof(err), "open %s: %s", tmp, strerror(errno));
        printf("devd: %s\n", err);
        reply_status(fd, "500 Cannot Open", err);
        return;
    }

    if (body_in_hdr > 0) {
        md5_update(&md5, sep + sep_len, body_in_hdr);
        if (write_full(out, sep + sep_len, body_in_hdr) == 0) {
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
        r = read_wait(fd, buf, want, &idle, IDLE_MAX);
        if (r <= 0) {
            break;
        }
        md5_update(&md5, buf, r);
        if (write_full(out, buf, r) < 0) {
            break;
        }
        total += r;
    }
    close(out);

    if (content_len > 0 && total != content_len) {
        unlink(tmp);
        snprintf(err, sizeof(err), "short %d/%d", total, content_len);
        printf("devd: %s\n", err);
        reply_status(fd, "500 Short Body", err);
        return;
    }
    if (total == 0) {
        unlink(tmp);
        printf("devd: empty body\n");
        reply_status(fd, "500 Empty Body", "no data");
        return;
    }

    {
        unsigned char md5d[16];
        char md5h[33];
        md5_final(&md5, md5d);
        md5_hex(md5d, md5h);
        snprintf(okbody, sizeof(okbody), "OK\nmd5=%s\nsize=%d\n", md5h, total);
    }
    if (strcmp(tmp, path) == 0) {
        /* direct 模式：写的就是目标文件本身，没有 tmp 需要改名 */
        printf("devd: wrote %s (%d bytes, direct)\n", path, total);
        reply_status(fd, "200 OK", okbody);
        return;
    }

    unlink(path);
    if (rename(tmp, path) != 0) {
        snprintf(err, sizeof(err), "rename: %s", strerror(errno));
        printf("devd: %s\n", err);
        reply_status(fd, "500 Rename Failed", err);
        return;
    }
    printf("devd: wrote %s (%d bytes)\n", path, total);
    reply_status(fd, "200 OK", okbody);
}

static void do_md5(int fd, const char *path) {
    static char buf[16384];
    md5_ctx_t md5;
    unsigned char digest[16];
    char hex[33];
    char body[96];
    int in = open(path, O_RDONLY);
    int total = 0;
    int r;

    if (in < 0) {
        reply_status(fd, "404 Not Found", "no such file");
        return;
    }
    md5_init(&md5);
    while ((r = (int)read(in, buf, sizeof(buf))) > 0) {
        md5_update(&md5, buf, r);
        total += r;
    }
    close(in);
    if (r < 0) {
        reply_status(fd, "500 Read Error", "read failed");
        return;
    }
    md5_final(&md5, digest);
    md5_hex(digest, hex);
    snprintf(body, sizeof(body), "md5=%s\nsize=%d\n", hex, total);
    reply_status(fd, "200 OK", body);
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
    printf("devd: sent %s (%d bytes)\n", path, total);
}

/* POST /run —— body 是要执行的命令，返回 stdout+stderr。
 * 用途：改完代码/内核后不用人工操作，远程跑程序、看 ps、拉日志。
 * 先回 200 再流式输出，靠 Connection: close 表示结束
 * （输出长度事先不知道，不能用 Content-Length）。 */
static void do_run(int fd, char *hdr, int hlen) {
    char cmd[1024];
    char buf[1024];
    int clen = find_content_length(hdr, hlen);
    int got = 0;
    int idle = 0;
    int pfd[2];
    pid_t pid;
    const char *sep = find_bytes(hdr, hlen, "\r\n\r\n", 4);
    int sep_len = 4;

    if (sep == NULL) {
        sep = find_bytes(hdr, hlen, "\n\n", 2);
        sep_len = 2;
    }
    if (sep == NULL) {
        reply_status(fd, "400 Bad Request", "no body");
        return;
    }
    {
        int in_hdr = hlen - (int)(sep - hdr) - sep_len;
        if (in_hdr > 0) {
            if (in_hdr > (int)sizeof(cmd) - 1) {
                in_hdr = (int)sizeof(cmd) - 1;
            }
            memcpy(cmd, sep + sep_len, (size_t)in_hdr);
            got = in_hdr;
        }
    }
    while (clen > 0 && got < clen && got < (int)sizeof(cmd) - 1) {
        int want = clen - got;
        int r;
        if (want > (int)sizeof(cmd) - 1 - got) {
            want = (int)sizeof(cmd) - 1 - got;
        }
        r = read_wait(fd, cmd + got, want, &idle, IDLE_MAX);
        if (r <= 0) {
            break;
        }
        got += r;
    }
    cmd[got] = '\0';
    printf("devd: RUN [%s]\n", cmd);

    if (pipe(pfd) != 0) {
        reply_status(fd, "500 Pipe", NULL);
        return;
    }
    pid = fork();
    if (pid == 0) {
        close(pfd[0]);
        /* 后台命令（带 &）不能接 pipe：pushd 处理完请求就关 pipe，
         * 后台进程再写 stdout 会收到 SIGPIPE 直接死（实测 sshd 起来
         * 3 秒后 22 端口就从 OPEN 变 refused）。改成 /dev/null。
         * 前台命令接 pipe 以便把输出回给客户端。 */
        if (strchr(cmd, '&') != NULL) {
            /* 后台进程的 stdout/stderr 落到 /etc/run.err：只打开一次、
             * 由子进程一直持有，不反复 open/close —— 后者会破坏 FAT 里
             * 别的进程正在用的 FIL（实测导致 "fat write error code 9"）。
             * 这样 sshd -ddd 和 sshd-auth 的全部调试输出都能远程取回。 */
            int logf = open("/etc/run.err", O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (logf >= 0) {
                (void)dup2(logf, 1);
                (void)dup2(logf, 2);
                if (logf > 2) {
                    close(logf);
                }
            }
            /* 彻底脱离：新建会话，并关掉从 pushd 继承的所有 fd
             * （尤其是 9002 的监听 socket）。否则后台进程长期持有监听
             * fd，pushd 退出后端口状态错乱；实测还会把 pushd 一起带崩。 */
            (void)setsid();
            {
                int k;
                for (k = 3; k < 64; k++) {
                    close(k);
                }
            }
        } else {
            (void)dup2(pfd[1], 1);
            /* 前台命令合并 stderr：sshd -ddd 的调试/致命信息都走 stderr，
             * 不合并的话客户端只能看到 stdout（几乎是空的），
             * 查 exec 失败原因时什么都看不到。 */
            (void)dup2(pfd[1], 2);
        }
        close(pfd[1]);
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127);
    }
    if (pid < 0) {
        close(pfd[0]);
        close(pfd[1]);
        reply_status(fd, "500 Fork", NULL);
        return;
    }
    close(pfd[1]);
    write_full(fd, "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n", 40);
    for (;;) {
        int r = (int)read(pfd[0], buf, sizeof(buf));
        if (r <= 0) {
            break;
        }
        if (write_full(fd, buf, r) < 0) {
            break;
        }
    }
    close(pfd[0]);
    /* 【绝不无限等】原来是 waitpid(pid, NULL, 0)：如果 sh 在 "cmd &" 之后
     * 不退出（自制 shell 的行为不确定），pushd 就会永久阻塞，而它是单线程
     * 串行的 —— 一个请求卡死，后面所有 HTTP 请求全部排在 accept 之外，
     * 表现为「端口 OPEN 但任何 GET/PUT 都超时」。改成最多轮询 3 秒，
     * 超时就把 sh 留在后台（僵尸最终由 init 回收），本次请求照常返回。 */
    if (strchr(cmd, '&') == NULL) {
        /* 只有前台命令需要等它结束；后台命令（&）已经 setsid 脱离，
         * 等它毫无意义，还会拖住 pushd 单线程。 */
        int waited = 0;
        while (waited < 300) {
            if (waitpid(pid, NULL, WNOHANG) != 0) {
                break;
            }
            usleep(10000);
            waited++;
        }
    }
    printf("devd: RUN done\n");
}

static void handle(int fd) {
    static char hdr[HDR_MAX + 1];
    int hlen = read_header(fd, hdr, sizeof(hdr));
    char *path;
    char *p;

    if (hlen <= 0) {
        printf("devd: header read failed\n");
        reply_status(fd, "400 Bad Request", "no header");
        return;
    }
    printf("devd: hdr %d: %.40s\n", hlen, hdr);

    if (strncmp(hdr, "PUT ", 4) == 0) {
        path = hdr + 4;
        p = strchr(path, ' ');
        if (p != NULL) {
            *p = '\0';
        }
        do_put(fd, path, hdr, hlen);
    } else if (strncmp(hdr, "GET /md5/", 9) == 0) {
        path = hdr + 9;
        p = strchr(path, ' ');
        if (p != NULL) {
            *p = '\0';
        }
        do_md5(fd, path);
    } else if (strncmp(hdr, "GET ", 4) == 0) {
        path = hdr + 4;
        p = strchr(path, ' ');
        if (p != NULL) {
            *p = '\0';
        }
        do_get(fd, path);
    } else if (strncmp(hdr, "POST /reboot", 12) == 0) {
        /* 一条 curl 就能让设备重启（走 PSCI），配合推内核自动化：
         *   curl -X POST http://<ip>:9004/reboot
         * 先回响应再复位，避免客户端只看到连接被重置。 */
        reply_status(fd, "200 OK", "rebooting\n");
        sleep(1);
        {
            extern int reboot(int);
            (void)reboot(0x01234567 /* RB_AUTOBOOT */);
        }
        for (;;) {
            sleep(1);
        }
    } else if (strncmp(hdr, "POST /run", 9) == 0) {
        do_run(fd, hdr, hlen);
    } else {
        reply_status(fd, "405 Method Not Allowed", "PUT/GET/POST /run");
    }
}

int main(void) {
    int srv;
    int on = 1;
    struct sockaddr_in addr;
    int pid;
    int port = PUSHD_PORT;

    printf("devd: starting on port %d\n", PUSHD_PORT);

    srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) {
        perror("devd: socket");
        return 1;
    }
    /* 【不要 SO_REUSEADDR】有它时 bind() 会对「已被别人 listen 的端口」
     * 也返回成功，随后 listen() 才报 EADDRINUSE —— 那样端口递增逻辑
     * 根本走不到。去掉后 bind 直接失败，才能顺利换下一个端口。
     * （代价：进程刚退出时可能有 TIME_WAIT，但 pushd 是常驻监听，
     *   正常更新都会换新端口，不受影响。） */
    (void)on;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    /* 【端口自动递增】旧版还在占着默认端口时，新版本要能起来 ——
     * 否则每次更新 pushd 都得先杀旧的，而杀掉旧的就再没有通道启动新的。
     * 从 PUSHD_PORT 开始往后试 16 个。 */
    {
        int bound = 0;
        for (port = PUSHD_PORT; port < PUSHD_PORT + 16; port++) {
            addr.sin_port = htons((unsigned short)port);
            if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
                printf("devd: bound port %d\n", port);
                bound = 1;
                break;
            }
        }
        if (!bound) {
            perror("devd: bind");
            close(srv);
            return 1;
        }
    }
    if (listen(srv, 4) < 0) {
        perror("devd: listen");
        close(srv);
        return 1;
    }
    printf("devd: ready  curl -T <file> http://<ip>:%d/<path>\n", port);

    pid = fork();
    if (pid < 0) {
        perror("devd: fork");
        close(srv);
        return 1;
    }
    if (pid > 0) {
        printf("devd: background pid=%d\n", pid);
        close(srv);
        return 0;
    }

    for (;;) {
        struct sockaddr_in peer;
        socklen_t plen = sizeof(peer);
        int c = accept(srv, (struct sockaddr *)&peer, &plen);
        if (c < 0) {
            /* 监听 socket 是非阻塞的：没有新连接时 accept 返回 EAGAIN。
             * 直接 continue 会变成纯忙等（实测两个 pushd 各占 23% / 9% CPU，
             * ps 里 calls 全是 202=accept）。短睡 5ms 把 CPU 降下来。 */
            usleep(5000);
            continue;
        }
        handle(c);
        close(c);
    }
    return 0;
}
