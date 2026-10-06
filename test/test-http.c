/*
 * test-http.c —— lwIP 上的最小 HTTP 服务。
 *
 * 板子地址是 192.168.1.250/24，网关 192.168.1.1。启动后在 Mac 上：
 *   curl http://192.168.1.250/
 *
 * 协议栈在内核里已经占着 /dev/net。不要同时跑 test-net，两边会抢同一张网卡。
 */

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define HTTP_PORT 80

/* 不用 printf：第一次进 libc 会碰用户堆 HEAP_ADDR(0x70100000)，
 * 内核那条 page fault 会和卡住处混在一起。write 只走已经映射好的只读串。 */
static void step(const char* s) {
  int n = 0;
  while (s[n] != 0) n++;
  if (n > 0) write(2, s, (size_t)n);
}

static int write_all(int fd, const char* buf, int n) {
  int off = 0;
  while (off < n) {
    int w = write(fd, buf + off, (size_t)(n - off));
    if (w < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    if (w == 0) return -1;
    off += w;
  }
  return 0;
}

static int read_req(int fd, char* buf, int cap) {
  int n = 0;
  while (n < cap - 1) {
    int r = read(fd, buf + n, (size_t)(cap - 1 - n));
    if (r < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    if (r == 0) break;
    n += r;
    buf[n] = 0;
    if (strstr(buf, "\r\n\r\n") != NULL || strstr(buf, "\n\n") != NULL) break;
  }
  buf[n] = 0;
  return n;
}

static void copy_path(char* dst, int dstsz, const char* req) {
  const char* p;
  int i;
  if (dstsz <= 0) return;
  dst[0] = '/';
  dst[1] = 0;
  if (strncmp(req, "GET ", 4) != 0) return;
  p = req + 4;
  for (i = 0; i < dstsz - 1 && p[i] != 0 && p[i] != ' ' && p[i] != '\r' &&
              p[i] != '\n';
       i++) {
    dst[i] = p[i];
  }
  dst[i] = 0;
  if (dst[0] == 0) {
    dst[0] = '/';
    dst[1] = 0;
  }
}

static void reply(int fd, const char* req) {
  char path[96];
  char body[384];
  char msg[640];
  int blen;
  int mlen;

  copy_path(path, (int)sizeof(path), req);
  blen = snprintf(body, sizeof(body),
                  "<html><body><h1>YiYiYa</h1>"
                  "<p>lwIP HTTP ok</p><p>path %s</p></body></html>\n",
                  path);
  if (blen < 0 || blen >= (int)sizeof(body)) blen = (int)strlen(body);
  mlen = snprintf(msg, sizeof(msg),
                  "HTTP/1.0 200 OK\r\n"
                  "Content-Type: text/html\r\n"
                  "Connection: close\r\n"
                  "Content-Length: %d\r\n"
                  "\r\n"
                  "%s",
                  blen, body);
  if (mlen < 0 || mlen >= (int)sizeof(msg)) mlen = (int)strlen(msg);
  if (write_all(fd, msg, mlen) < 0) perror("write");
}

int main(void) {
  int srv;
  int on = 1;
  struct sockaddr_in addr;

  step("test-http: socket\n");
  srv = socket(AF_INET, SOCK_STREAM, 0);
  if (srv < 0) {
    perror("socket");
    return 1;
  }
  step("test-http: setsockopt\n");
  setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(HTTP_PORT);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  step("test-http: bind\n");
  if (bind(srv, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    perror("bind");
    close(srv);
    return 1;
  }
  step("test-http: listen\n");
  if (listen(srv, 2) < 0) {
    perror("listen");
    close(srv);
    return 1;
  }
  step("test-http: http://192.168.1.250:80/\n");
  step("test-http: do not run test-net, the stack owns /dev/net\n");
  /* shell 对每条命令都 waitpid，没有 &。accept 不返回的话提示符就没了。
   * 父进程退出把 shell 还给用户，子进程继续听端口。 */
  step("test-http: fork\n");
  {
    int pid = fork();
    if (pid < 0) {
      perror("fork");
    } else if (pid > 0) {
      step("test-http: parent exit\n");
      return 0;
    }
  }
  step("test-http: child accept\n");

  for (;;) {
    struct sockaddr_in peer;
    socklen_t plen = sizeof(peer);
    char req[1024];
    int c = accept(srv, (struct sockaddr*)&peer, &plen);
    unsigned char* ip;
    if (c < 0) {
      if (errno == EINTR) continue;
      perror("accept");
      break;
    }
    ip = (unsigned char*)&peer.sin_addr.s_addr;
    printf("test-http: client %u.%u.%u.%u\n", ip[0], ip[1], ip[2], ip[3]);
    if (read_req(c, req, (int)sizeof(req)) < 0) perror("read");
    else reply(c, req);
    close(c);
  }
  close(srv);
  return 1;
}
