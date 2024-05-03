#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
  FILE* file;
  int chr;
  int count;
  file = fopen("/conf/init.conf", "r");

  int size = 0;
  char* buff = NULL;
  int read = 0;

  if (file == NULL) {
    printf("%s: %s : %s\n", argv[0], argv[count], strerror(errno));
    return -1;
  }
  while ((read = getline(&buff, &size, file)) != -1) {
    // int s = strlen(buff);
    // buff[s - 1] = 0;
     buff[size - 1] = 0;
    if(buff[0]=='#'){
      continue;
    }

    const char* split = " ";
    char* ptr = strtok(buff, split);
    char* args[30];
    int i = 0;
    char* proc = ptr;

    while (ptr != NULL) {
      printf("arg %s\n", ptr);
      args[i++] = ptr;
      ptr = strtok(NULL, split);
    }
    args[i++] = NULL;

    char* arg = ptr;

    pid_t p1 = -1;
    p1 = fork();  // 返回2次
    if (p1 == 0) {
      sleep(20);
      execv(proc, args);
      wait();
    }
    if (p1 > 0) {
    }
  }

  fclose(file);

  return 0;
}