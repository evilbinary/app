#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(){
  printf("TM start\n"); fflush(stdout);
  for(int r=0;r<3;r++){
    void* p[8];
    for(int i=0;i<8;i++){ p[i]=malloc(64*(i+1)); if(!p[i]){printf("TM null %d\n",i);return 1;} }
    for(int i=0;i<8;i++) free(p[i]);
    printf("TM round %d ok\n", r); fflush(stdout);
  }
  void* big=malloc(1<<20);
  if(!big){printf("TM big null\n");return 1;}
  for(int i=0;i<1;i++){ ((char*)big)[0]=1; ((char*)big)[1048575]=2; }
  free(big);
  printf("TM big ok\n"); fflush(stdout);
  return 0;
}
