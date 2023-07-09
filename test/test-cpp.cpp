#include <iostream>
#include <vector>

#include "stdio.h"

using namespace std;

class A{
public:
  A(){
    printf("A===>\n");
  };
 ~A(){
    printf("~A===>\n");
 }
};

A gaga_a;

int main() {

  std::vector<int> numbers;
  numbers.push_back(10);
  numbers.push_back(20);
  numbers.push_back(30);

  printf("hello2 %d\n", numbers.size());

  numbers.clear();
  printf("size %d\n", numbers.size());

  cout<<"hello c++"<<endl;

  return 0;
}