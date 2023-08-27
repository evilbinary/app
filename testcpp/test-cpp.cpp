#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#include "stdio.h"

using namespace std;


class A {
 public:
  A() { printf("A===>\n"); };
  ~A() { printf("~A===>\n"); }
};

A gaga_a;

void test_vector() {
  std::vector<int> numbers;
  numbers.push_back(10);
  numbers.push_back(20);
  numbers.push_back(30);

  printf("hello2 %d\n", numbers.size());

  numbers.clear();
  printf("size %d\n", numbers.size());

  cout << "hello c++" << endl;
}

void test_dir() {
  // string parentDir = "./gmenu2x/sections";
  // std::error_code ec;
  // for (const auto& entry : filesystem::directory_iterator(parentDir,ec)) {
  //   const auto filename = entry.path().filename().string();
  //   cout << "filename=>" << filename << endl;
  // }
}

int main() {
  test_dir();

  return 0;
}