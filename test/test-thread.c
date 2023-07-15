#include <stdio.h>
#include <pthread.h>

// 线程函数，打印一条消息
void* printMessage(void* arg) {
    printf("Hello from thread!\n");
    return NULL;
}

int main() {
    pthread_t tid; // 用于存储线程 ID 的变量

    // 创建一个新线程，并执行 printMessage 函数
    int result = pthread_create(&tid, NULL, printMessage, NULL);
    if (result != 0) {
        printf("Failed to create thread.\n");
        return 1;
    }

    // 等待新线程执行完毕
    result = pthread_join(tid, NULL);
    if (result != 0) {
        printf("Failed to join thread.\n");
        return 1;
    }

    // 打印主线程的消息
    printf("Hello from main thread!\n");

    return 0;
}
