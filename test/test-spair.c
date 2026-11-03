/*
 * socketpair 完整性测试
 *
 * 【为什么】sshd 的 privsep 通道是 socketpair(AF_UNIX, SOCK_STREAM)。
 * 实测出现「消息前 4 字节丢失」：monitor 写了
 *     [00 00 01 CC][34][00 00 01 4D][config 333 字节]
 * 对端却从 type 字节开始读，拿到 0x34000001 当成消息长度 → bad msg_len。
 *
 * 这个测试用可校验的消息序列跑一遍 socketpair，看是否真的丢字节：
 *   消息 = [seq:4][len:4][payload: len 字节]（全部小端无关，按字节比较）
 * 写一条、读一条交替进行，避免环形缓冲溢出掩盖问题。
 *
 * 覆盖：
 *   1. 变长消息（1..64 字节 payload）连续 200 条
 *   2. 一次性写大于读分片（写 512，分 4 次读）
 *   3. 连续写多条再一起读（考验 ring 的 wrap）
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>

static unsigned char tx[8192];
static unsigned char rx[8192];

static int build(unsigned char *b, unsigned seq, unsigned len) {
    b[0] = (seq >> 24) & 0xff;
    b[1] = (seq >> 16) & 0xff;
    b[2] = (seq >> 8) & 0xff;
    b[3] = seq & 0xff;
    b[4] = (len >> 24) & 0xff;
    b[5] = (len >> 16) & 0xff;
    b[6] = (len >> 8) & 0xff;
    b[7] = len & 0xff;
    for (unsigned i = 0; i < len; i++) {
        b[8 + i] = (unsigned char)((seq * 31 + i) & 0xff);
    }
    return (int)(8 + len);
}

/* 读满 n 字节；返回实际读到的字节数 */
static int read_full(int fd, unsigned char *p, int n) {
    int got = 0;
    while (got < n) {
        ssize_t r = read(fd, p + got, (size_t)(n - got));
        if (r <= 0) {
            break;
        }
        got += (int)r;
    }
    return got;
}

int main(void) {
    int sv[2];
    int fails = 0;

    printf("spair: start\n");

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
        printf("spair: socketpair FAILED\n");
        return 1;
    }
    printf("spair: socketpair ok (%d, %d)\n", sv[0], sv[1]);

    /* --- 测试 1：变长消息，写一条读一条 --- */
    for (unsigned i = 0; i < 200; i++) {
        unsigned len = (i % 64) + 1;
        int n = build(tx, i, len);
        ssize_t w = write(sv[0], tx, (size_t)n);
        if (w != n) {
            printf("spair: t1 seq=%u write %d want %d\n", i, (int)w, n);
            fails++;
            break;
        }
        int got = read_full(sv[1], rx, n);
        if (got != n) {
            printf("spair: t1 seq=%u read %d want %d\n", i, got, n);
            fails++;
            break;
        }
        if (memcmp(tx, rx, (size_t)n) != 0) {
            int off = 0;
            while (off < n && tx[off] == rx[off]) {
                off++;
            }
            printf("spair: t1 seq=%u MISMATCH at %d len=%u (tx=%02x rx=%02x)\n",
                   i, off, len, tx[off], rx[off]);
            fails++;
            break;
        }
    }
    printf("spair: t1 (200 变长消息) %s\n", fails ? "FAIL" : "ok");

    /* --- 测试 2：一次写 512，分 4 次读 --- */
    if (!fails) {
        int n = build(tx, 900, 504); /* 8 + 504 = 512 */
        ssize_t w = write(sv[0], tx, (size_t)n);
        if (w != n) {
            printf("spair: t2 write %d want %d\n", (int)w, n);
            fails++;
        } else {
            int ok = 1;
            for (int k = 0; k < 4; k++) {
                int got = read_full(sv[1], rx + k * 128, 128);
                if (got != 128) {
                    printf("spair: t2 read#%d got %d\n", k, got);
                    ok = 0;
                    break;
                }
            }
            if (ok && memcmp(tx, rx, (size_t)n) != 0) {
                int off = 0;
                while (off < n && tx[off] == rx[off]) {
                    off++;
                }
                printf("spair: t2 MISMATCH at %d\n", off);
                ok = 0;
            }
            printf("spair: t2 (512 分 4 次读) %s\n", ok ? "ok" : "FAIL");
            if (!ok) {
                fails++;
            }
        }
    }

    /* --- 测试 3：连写 8 条小消息，再一次读出 --- */
    if (!fails) {
        int total = 0;
        for (unsigned i = 0; i < 8; i++) {
            int n = build(tx + total, 1000 + i, 16);
            total += n;
        }
        ssize_t w = write(sv[0], tx, (size_t)total);
        if (w != total) {
            printf("spair: t3 write %d want %d\n", (int)w, total);
            fails++;
        } else {
            int got = read_full(sv[1], rx, total);
            int ok = (got == total) && memcmp(tx, rx, (size_t)total) == 0;
            if (!ok) {
                int off = 0;
                while (off < total && off < got && tx[off] == rx[off]) {
                    off++;
                }
                printf("spair: t3 got %d want %d mismatch@%d\n", got, total, off);
                fails++;
            }
            printf("spair: t3 (8 条连写) %s\n", ok ? "ok" : "FAIL");
        }
    }

    /* --- 测试 4：同一个 fd 写进去，能否被自己读到 ---
     * 这是 sshd privsep 的用法：mm_get_state() 里
     *   mm_request_send(pmonitor->m_recvfd, ...)     写 fd 3
     *   mm_request_receive_expect(pmonitor->m_recvfd, ...)  又从 fd 3 读
     * socketpair 的语义是「写这端，对端才读得到」。如果同端能读到自己写的
     * 数据，就会拿到自己刚发的字节当回复，表现为 bad msg_len。 */
    if (!fails) {
        unsigned char probe[8];
        int got;
        memset(probe, 0xAB, sizeof(probe));
        if (write(sv[0], probe, sizeof(probe)) != (ssize_t)sizeof(probe)) {
            printf("spair: t4 write failed\n");
            fails++;
        } else {
            memset(rx, 0, sizeof(rx));
            /* 非阻塞地看 sv[0] 自己有没有数据可读 */
            extern int fcntl(int, int, ...);
            fcntl(sv[0], 4 /* F_SETFL */, 0x800 /* O_NONBLOCK */);
            got = (int)read(sv[0], rx, sizeof(rx));
            fcntl(sv[0], 4, 0);
            if (got > 0 && memcmp(rx, probe, sizeof(probe)) == 0) {
                printf("spair: t4 SELF-READ BUG (same fd read its own write)\n");
                fails++;
            } else {
                printf("spair: t4 (同端不可自读) ok (got=%d)\n", got);
            }
            /* 数据应该还在对端：用 sv[1] 读回来 */
            got = read_full(sv[1], rx, (int)sizeof(probe));
            if (got != (int)sizeof(probe) ||
                memcmp(rx, probe, sizeof(probe)) != 0) {
                printf("spair: t4 peer read failed got=%d\n", got);
                fails++;
            }
        }
    }

    /* --- 测试 5：A 写、B 读之后，A 再写、A 读（模拟 sshd 的交替） --- */
    if (!fails) {
        unsigned char a[16], b[16];
        memset(a, 0x11, sizeof(a));
        memset(b, 0x22, sizeof(b));
        write(sv[1], b, sizeof(b));     /* 对端写 */
        int got = read_full(sv[0], rx, (int)sizeof(b));  /* 本端读对端的 */
        if (got != (int)sizeof(b) || memcmp(rx, b, sizeof(b)) != 0) {
            printf("spair: t5 cross read failed got=%d\n", got);
            fails++;
        } else {
            printf("spair: t5 (对端写、本端读) ok\n");
        }
    }

    close(sv[0]);
    close(sv[1]);

    printf("spair: done fails=%d\n", fails);
    return fails ? 1 : 0;
}
