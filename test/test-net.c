/*
 * test-net.c —— V3s 网卡(EMAC)收发测试（用户态）
 *
 * 内核侧接口（duck/modules/net/v3s.c）：
 *   open("/dev/net") → read()/write() 收发**原始以太帧**（不含 FCS）
 *   ioctl: 0x01=取 MAC  0x02=链路状态  0x03=速率|双工  0x04=PHY id
 *          0x05=统计(rx_frames,rx_errors,tx_frames,tx_errors,tx_busy)
 *
 * 用法:
 *   test-net                        # 默认网段 192.168.1.，被动监听 3 秒
 *   test-net 192.168.1.             # 指定网段前缀（ARP 扫描用）
 *   test-net 192.168.1. 5           # 指定被动监听秒数
 *
 * 测试步骤:
 *   1) 打开设备，打印 MAC / 链路 / 速率双工 / PHY id / 初始计数
 *   2) 被动收包 N 秒：局域网里一般有 ARP/DHCP/广播流量，能收到就证明 RX 通路好
 *   3) ARP 扫描：在网段内发 ARP 请求，等回复并打印 IP→对端 MAC（端到端验证）
 *   4) 发自定义类型(0x88B5)广播帧，核对 tx 计数增长，验证 TX 通路
 *   5) 汇总计数
 *
 * 注意: 本机假 IP 用 <网段>.250 且只做 ARP，不配 IP、不发 IP 报文，所以不需要
 *       上层协议栈；如果局域网里真有设备占用 .250，也只会多一条 ARP 记录，无影响。
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define NET_DEV "/dev/net"
#define ETH_ALEN 6
#define FRAME_MAX 1600

#define ETH_P_ARP 0x0806
#define ETH_P_TEST 0x88B5 /* 本地实验用类型（不在标准分配里） */

#define NET_IOCTL_MAC 0x01
#define NET_IOCTL_LINK 0x02
#define NET_IOCTL_SPEED 0x03
#define NET_IOCTL_PHYID 0x04
#define NET_IOCTL_STATS 0x05

#define ARP_SCAN_MAX 32

static int fd = -1;
static unsigned char mac[ETH_ALEN];
static unsigned char frame[FRAME_MAX];
static unsigned char rxbuf[FRAME_MAX];

static unsigned int g_rx_seen;   /* 收到的帧数 */
static unsigned int g_arp_req;   /* 收到的 ARP 请求数 */
static unsigned int g_arp_reply; /* 收到的 ARP 应答数 */
static unsigned int g_other;     /* 其它类型帧 */
static unsigned int g_tx_sent;   /* 本程序发出的帧数 */

/* ------------------------------------------------------------------ */
/* 小工具                                                              */
/* ------------------------------------------------------------------ */

static void msleep(int ms) {
  struct timespec ts;
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (long)(ms % 1000) * 1000000L;
  nanosleep(&ts, NULL);
}

static void put_be16(unsigned char* p, unsigned v) {
  p[0] = (unsigned char)(v >> 8);
  p[1] = (unsigned char)(v & 0xFF);
}

static unsigned get_be16(const unsigned char* p) {
  return ((unsigned)p[0] << 8) | p[1];
}

static unsigned get_be32(const unsigned char* p) {
  return ((unsigned)p[0] << 24) | ((unsigned)p[1] << 16) | ((unsigned)p[2] << 8) | p[3];
}

static void put_be32(unsigned char* p, unsigned v) {
  p[0] = (unsigned char)(v >> 24);
  p[1] = (unsigned char)(v >> 16);
  p[2] = (unsigned char)(v >> 8);
  p[3] = (unsigned char)(v & 0xFF);
}

static void mac_str(const unsigned char* m, char* out) {
  sprintf(out, "%02x:%02x:%02x:%02x:%02x:%02x", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static void ip_str(unsigned ip, char* out) {
  sprintf(out, "%u.%u.%u.%u", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF,
          ip & 0xFF);
}

/* 解析 "192.168.1." 这样的前缀成 24 位基地址（返回 0 表示格式不对） */
static unsigned parse_prefix(const char* s) {
  unsigned a = 0, b = 0, c = 0;
  if (sscanf(s, "%u.%u.%u.", &a, &b, &c) != 3) return 0;
  if (a > 255 || b > 255 || c > 255) return 0;
  return (a << 24) | (b << 16) | (c << 8);
}

/* ------------------------------------------------------------------ */
/* 帧构造                                                              */
/* ------------------------------------------------------------------ */

static void eth_hdr(unsigned char* f, const unsigned char* dst, unsigned short type) {
  memcpy(f, dst, ETH_ALEN);
  memcpy(f + ETH_ALEN, mac, ETH_ALEN);
  put_be16(f + 12, type);
}

/* 构造 ARP 请求：who-has target_ip tell own_ip */
static unsigned build_arp_request(unsigned char* f, unsigned own_ip, unsigned target_ip) {
  unsigned len = 14 + 28;
  static const unsigned char bcast[ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  memset(f, 0, 60);
  eth_hdr(f, bcast, ETH_P_ARP);

  put_be16(f + 14, 0x0001);          /* hw type = Ethernet */
  put_be16(f + 16, 0x0800);          /* proto = IPv4 */
  f[18] = 6;                         /* hw len */
  f[19] = 4;                         /* proto len */
  put_be16(f + 20, 0x0001);          /* opcode = request */
  memcpy(f + 22, mac, ETH_ALEN);     /* sender MAC */
  put_be32(f + 28, own_ip);          /* sender IP */
  memset(f + 32, 0, ETH_ALEN);       /* target MAC = unknown */
  put_be32(f + 38, target_ip);       /* target IP */

  return len < 60 ? 60 : len;        /* 小于 60 的帧补齐（驱动也会补） */
}

static unsigned build_test_frame(unsigned char* f) {
  static const unsigned char bcast[ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  unsigned i;

  memset(f, 0, 60);
  eth_hdr(f, bcast, ETH_P_TEST);
  for (i = 14; i < 60; i++) f[i] = (unsigned char)i;

  return 60;
}

/* ------------------------------------------------------------------ */
/* 收包处理                                                            */
/* ------------------------------------------------------------------ */

static void handle_frame(const unsigned char* f, unsigned len) {
  unsigned type;
  char s[64], t[64];

  if (len < 14) return;
  g_rx_seen++;
  type = get_be16(f + 12);

  if (type == ETH_P_ARP && len >= 14 + 28) {
    unsigned op = get_be16(f + 20);
    if (op == 0x0002) { /* reply */
      unsigned spa = get_be32(f + 28);
      g_arp_reply++;
      ip_str(spa, s);
      mac_str(f + 22, t);
      printf("  [ARP 应答] %s 是 %s\n", s, t);
      return;
    }
    if (op == 0x0001) {
      g_arp_req++;
      ip_str(get_be32(f + 28), s);
      ip_str(get_be32(f + 38), t);
      printf("  [ARP 请求] %s 在问 %s 是谁\n", s, t);
      return;
    }
  }

  if (type == ETH_P_TEST) {
    printf("  [自定义帧 0x88B5] 来自 ");
    mac_str(f + 6, t);
    printf("%s len=%u（可能是别的板子在跑本测试）\n", t, len);
    return;
  }

  /* 其它类型只统计，避免刷屏 */
  g_other++;
  if (g_other <= 8) {
    mac_str(f + 6, t);
    printf("  [帧] 0x%04x len=%u from %s\n", type, len, t);
  }
}

/* 收包一段时间（ms），返回此期间收到的帧数 */
static unsigned rx_pump(int ms) {
  unsigned n = 0;
  int waited = 0;

  while (waited < ms) {
    ssize_t r = read(fd, rxbuf, sizeof(rxbuf));
    if (r > 0) {
      handle_frame(rxbuf, (unsigned)r);
      n++;
      continue; /* 可能还有，继续收 */
    }
    msleep(5);
    waited += 5;
  }
  return n;
}

/* ------------------------------------------------------------------ */
/* 各测试步骤                                                          */
/* ------------------------------------------------------------------ */

static void show_device_info(void) {
  unsigned val = 0;
  char s[64];

  printf("[1] 设备信息\n");
  if (ioctl(fd, NET_IOCTL_MAC, mac) == 0) {
    mac_str(mac, s);
    printf("    MAC        : %s\n", s);
  } else {
    memset(mac, 0, sizeof(mac));
    printf("    MAC        : 取不到（ioctl 0x01 失败）\n");
  }

  /* 链路状态：驱动既写 args 也作返回值，两种取法都判一下 */
  val = 0;
  {
    unsigned r = (unsigned)ioctl(fd, NET_IOCTL_LINK, &val);
    printf("    链路状态   : %s\n", (r > 0 || val) ? "up" : "down（检查网线/对端）");
  }

  val = 0;
  if (ioctl(fd, NET_IOCTL_SPEED, &val) == 0) {
    printf("    速率/双工  : %uM %s\n", val & 0xFFFF,
           ((val >> 16) & 0xFFFF) ? "full-duplex" : "half-duplex");
  }

  val = 0;
  if (ioctl(fd, NET_IOCTL_PHYID, &val) == 0) {
    printf("    PHY id     : %08x\n", val);
  }
}

static void show_stats(const char* tag) {
  unsigned st[5] = {0, 0, 0, 0, 0};
  if (ioctl(fd, NET_IOCTL_STATS, st) == 0) {
    printf("    [%s] rx=%u rx_err=%u tx=%u tx_err=%u tx_busy=%u\n", tag, st[0], st[1],
           st[2], st[3], st[4]);
  }
}

/* 被动收包：局域网里通常有 ARP/DHCP/广播流量 */
static void test_passive_rx(int seconds) {
  unsigned before, after;

  printf("[2] 被动收包 %d 秒（期望能看到局域网广播/ARP 等）\n", seconds);
  before = g_rx_seen;
  rx_pump(seconds * 1000);
  after = g_rx_seen;
  printf("    收到 %u 帧\n", after - before);
}

/* ARP 扫描：真正的端到端验证（TX + 网线 + 对端 + RX） */
static void test_arp_scan(unsigned prefix, unsigned own_ip) {
  unsigned i, replies_before;

  printf("[3] ARP 扫描 %u.%u.%u.1 ~ .%u（本机假 IP %u.%u.%u.250）\n", (prefix >> 24) & 0xFF,
         (prefix >> 16) & 0xFF, (prefix >> 8) & 0xFF, ARP_SCAN_MAX, (prefix >> 24) & 0xFF,
         (prefix >> 16) & 0xFF, (prefix >> 8) & 0xFF);

  replies_before = g_arp_reply;
  for (i = 1; i <= ARP_SCAN_MAX; i++) {
    unsigned len = build_arp_request(frame, own_ip, prefix | i);
    if (write(fd, frame, len) > 0) {
      g_tx_sent++;
    } else {
      printf("    write 失败（第 %u 个），链路或驱动有问题\n", i);
      break;
    }
    msleep(20);
    rx_pump(10); /* 边发边收 */
  }

  /* 再给对端一点时间回包 */
  rx_pump(1500);
  printf("    收到 %u 个 ARP 应答\n", g_arp_reply - replies_before);
}

/* 发自定义广播帧，验证 TX 计数 */
static void test_tx(unsigned n) {
  unsigned i, sent = 0;

  printf("[4] 发送 %u 个自定义广播帧（EtherType 0x88B5）\n", n);
  for (i = 0; i < n; i++) {
    unsigned len = build_test_frame(frame);
    if (write(fd, frame, len) > 0) {
      sent++;
      g_tx_sent++;
    } else {
      printf("    第 %u 个 write 失败\n", i + 1);
      break;
    }
    msleep(20);
  }
  printf("    成功写出 %u/%u 帧\n", sent, n);
  msleep(100);
}

int main(int argc, char* argv[]) {
  unsigned prefix = 0xC0A80100; /* 默认 192.168.1. */
  unsigned own_ip;
  int seconds = 3;
  char s[64];

  if (argc > 1) {
    unsigned p = parse_prefix(argv[1]);
    if (p == 0) {
      printf("用法: test-net [网段前缀，如 192.168.1.] [被动监听秒数]\n");
      return 1;
    }
    prefix = p;
  }
  if (argc > 2) {
    seconds = atoi(argv[2]);
    if (seconds < 0) seconds = 0;
  }
  own_ip = prefix | 250;

  printf("=== V3s 网卡测试 ===\n");
  fd = open(NET_DEV, O_RDWR);
  if (fd < 0) {
    printf("打不开 %s：内核里没有 net 设备（确认内核已带 duck/modules/net/v3s.c 并启动时打印过 emac: ready）\n",
           NET_DEV);
    return 1;
  }
  printf("已打开 %s\n", NET_DEV);

  show_device_info();
  ip_str(own_ip, s);
  printf("    本机假 IP  : %s（仅用于 ARP）\n", s);
  show_stats("初始");

  test_passive_rx(seconds);
  test_arp_scan(prefix, own_ip);
  test_tx(5);

  printf("[5] 汇总\n");
  printf("    本程序发出 %u 帧，收到 %u 帧（ARP 请求 %u，ARP 应答 %u，其它 %u）\n", g_tx_sent,
         g_rx_seen, g_arp_req, g_arp_reply, g_other);
  show_stats("结束");

  if (g_arp_reply > 0) {
    printf("结论: PASS —— 收/发都是通的，且能和局域网设备完成 ARP 交互\n");
  } else if (g_rx_seen > 0) {
    printf("结论: 收方向通（能收到局域网帧），但没收到 ARP 应答——\n");
    printf("      可能对端不在扫描网段内，或交换机/VLAN 隔离。可换网段前缀再试。\n");
  } else {
    printf("结论: 没有收到任何帧。检查：网线/对端是否在同一广播域、link 是否为 up\n");
  }

  close(fd);
  return 0;
}
