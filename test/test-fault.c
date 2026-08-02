#include "stdio.h"

int fault_npe(void) {
  int *p = (int *)0;
  return *p;
}

int fault_call(void) { return fault_npe(); }

int main(int argc, char* argv[]) {
  printf("fault app start\n");
  fault_call();
  printf("fault app end\n");
  return 0;
}
