#include <pthread.h>
#include <stdio.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include "cmocka.h"

// 线程函数，打印一条消息
void* thread_print_message(void* arg) {
  printf("Hello from thread! %x\n", arg);
  return NULL;
}

int test_thread_create() {
  pthread_t tid;  // 用于存储线程 ID 的变量
  printf("start %x\n", thread_print_message);
  // 创建一个新线程，并执行 print_message 函数
  int result = pthread_create(&tid, NULL, thread_print_message, 0x998);
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

pthread_key_t p_key;
void func1() {
  int* tmp = (int*)pthread_getspecific(p_key);
  printf("%d is runing in %s\n", *tmp, __func__);
}

void* thread_func(void* args) {
  pthread_setspecific(p_key, args);

  int* tmp = (int*)pthread_getspecific(p_key);  // 获得线程的私有空间
  printf("%d is runing in %s\n", *tmp, __func__);

  *tmp = (*tmp) * 100;  // 修改私有变量的值

  func1();

  return (void*)0;
}

void test_thread_key_create() {
  pthread_t pa, pb;
  int a = 1;
  int b = 2;
  pthread_key_create(&p_key, NULL);
  pthread_create(&pa, NULL, thread_func, &a);
  pthread_create(&pb, NULL, thread_func, &b);
  pthread_join(pa, NULL);
  pthread_join(pb, NULL);

}

int main(int argc, char* argv[]) {
  const struct CMUnitTest tests[] = {
      //   cmocka_unit_test(test_thread_create),
      cmocka_unit_test(test_thread_key_create),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}