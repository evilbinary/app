/* OpenSSL 随机数探针：定位 sshd 的 seed_rng() 为什么失败。
 *
 * 背景：sshd 在 main() 里 seed_rng() -> RAND_bytes() 失败就 fatal 退出，
 * 而 fatal 的输出走 stderr（裸机上往往看不到），表现就是「sshd 跑了但
 * 零输出、端口不监听」。这个程序把 RAND_bytes 的返回值和 errno 直接打到
 * stdout，一眼就能看出是「调用失败」还是「拿到的不是随机数」。 */
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <openssl/rand.h>
#include <openssl/opensslv.h>
#include <openssl/crypto.h>

int main(void) {
    unsigned char buf[32];
    int r;

    printf("test-rand: enter\n");
    printf("openssl=%s\n", OpenSSL_version(OPENSSL_VERSION));
    /* 【关键】先显式跑一次全局初始化：no-threads 下 OpenSSL 的
     * CRYPTO_THREAD_run_once 是空实现，自动初始化不会发生，
     * 直接访问 RAND_status() 会踩空指针崩掉。 */
    printf("init=%d\n",
           OPENSSL_init_crypto(OPENSSL_INIT_LOAD_CRYPTO_STRINGS, NULL));
    printf("RAND_status=%d\n", RAND_status());

    errno = 0;
    r = RAND_bytes(buf, (int)sizeof(buf));
    printf("RAND_bytes=%d errno=%d(%s)\n", r, errno, strerror(errno));
    printf("RAND_status=%d\n", RAND_status());
    printf("first4=%02x%02x%02x%02x\n", buf[0], buf[1], buf[2], buf[3]);
    printf("test-rand: done\n");
    return 0;
}
