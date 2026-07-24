#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {

  if(argc<=1){

    return 0;
  }
  int pid = atoi(argv[1]);
  kill(pid,0);

  return 0;
}