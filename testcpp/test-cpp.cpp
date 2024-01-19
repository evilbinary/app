#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#include "stdio.h"
#include "dirent.h"


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

void test_readdir_readline(string const& linkfile){
  string line;
  string file= linkfile;
	ifstream infile (file.c_str(), ios_base::in);
	while (getline(infile, line, '\n')) {
    cout<<"line===>"<<line<<endl;
  }
  infile.close();
}

void test_readdir(){
  string path="gmenu2x/sections/applications";

  DIR *dirp = opendir(path.c_str());
	if (!dirp) return;

	while (struct dirent *dptr = readdir(dirp)) {
		if (dptr->d_type != DT_REG) continue;
		string linkfile = path + "/" + dptr->d_name;

    cout<<"dir==>"<<linkfile<<endl;
    test_readdir_readline(linkfile);

  }
}

void test_filesystem(){
  string path="/gmenu2x/sections";
  printf("readSections start %s\n",path.c_str());
  std::error_code ec;
	for (const auto& entry : std::filesystem::directory_iterator(path, ec))
	{
		const auto filename = entry.path().filename().string();
    printf("readSections %s\n",filename);
		// if (filename[0] != '.')
			// sectionNamed(filename);
	}
	printf("readSections end\n");
}

int main() {
  // test_dir();
  // test_readline();
  // test_readdir();
  test_filesystem();

  return 0;
}