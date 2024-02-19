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
    
    int s=strlen(buff);
    buff[s-1]=0;
    
    
    const char* split = " ";
    char* ptr = strtok(buff, split);
    char* proc = ptr;
    ptr = strtok(NULL, split);
    char* arg = ptr;

    pid_t p1 = -1;
    p1 = fork();  // 返回2次
    if (p1 == 0) {
      char* const args[] = {proc, NULL};
      sleep(1);
      if (arg == NULL) {
        arg = args;
      }
      execv(proc, arg);
      wait();
    }
    if (p1 > 0) {
    }
  }

  fclose(file);

  return 0;
}