/* pthread_once / futex 探针。
 *
 * 为什么需要：OpenSSL 3.5 的 OPENSSL_init_crypto() 第一步就是
 *   CRYPTO_THREAD_run_once(&base, ossl_lib_ctx_run_once)
 * 在 no-threads 下它是空实现（全局 libctx 永不初始化 → 段错误）；改成带
 * 线程后崩在 pthread_once 内部，所以要单独确认 pthread_once 能不能用。
 *
 * 判定：
 *   打印 done counter=1  → pthread_once 正常，问题在别处（PIC/重定位）
 *   崩在 before once 之后 → futex/pthread_once 的内核侧问题
 */
#include <pthread.h>
#include <stdio.h>

static pthread_once_t once = PTHREAD_ONCE_INIT;
static int counter = 0;

static void init_fn(void) {
    counter++;
    printf("  init_fn ran (counter=%d)\n", counter);
}

static void *worker(void *arg) {
    (void)arg;
    pthread_once(&once, init_fn);
    return NULL;
}

int main(void) {
    pthread_t t1, t2;

    printf("test-once: start\n");

    printf("  main: before once\n");
    pthread_once(&once, init_fn);
    printf("  main: after once, counter=%d\n", counter);

    printf("  main: create 2 threads\n");
    if (pthread_create(&t1, NULL, worker, NULL) != 0) {
        printf("  pthread_create t1 failed\n");
        return 1;
    }
    if (pthread_create(&t2, NULL, worker, NULL) != 0) {
        printf("  pthread_create t2 failed\n");
        return 1;
    }
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("test-once: done counter=%d (expect 1)\n", counter);
    return 0;
}
