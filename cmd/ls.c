#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>

int show_all = 0;
int long_mode = 0;
int show_nice = 0;
char buf[512];

void show_usage(int argc, char* argv[]) {
  printf(
      "ls - list files\n"
      "usage: %s [-lha] [path]\n"
      " -a     \033[3mlist all files (including . files)\033[0m\n"
      " -l     \033[3muse a long listing format\033[0m\n"
      " -h     \033[3mhuman-readable file sizes\033[0m\n"
      " -?     \033[3mshow this help text\033[0m\n"
      "\n",
      argv[0]);
}

static int ls_skip_entry(const struct dirent* ent) {
  if (ent == NULL || ent->d_name[0] == '\0') {
    return 1;
  }
  if (!show_all && ent->d_name[0] == '.' &&
      (ent->d_name[1] == '\0' ||
       (ent->d_name[1] == '.' && ent->d_name[2] == '\0'))) {
    return 1;
  }
  return 0;
}

void ls(char* path) {
  DIR* dir;
  struct dirent* ent;
  struct stat st;
  int col = 0;

  if (path == NULL) {
    return;
  }

  dir = opendir(path);
  if (dir == NULL) {
    printf("ls: cannot open %s\n", path);
    return;
  }

  while ((ent = readdir(dir)) != NULL) {
    if (ls_skip_entry(ent)) {
      continue;
    }

    if (long_mode) {
      printf("%-20s", ent->d_name);
      if (ent->d_type == DT_DIR) {
        printf("dir");
      } else if (ent->d_type == DT_REG) {
        printf("file");
      } else {
        printf("%x", ent->d_type);
      }
      sprintf(buf, "%s/%s", path, ent->d_name);
      if (stat(buf, &st) == 0) {
        printf(" %d", (int)st.st_size);
      }
      printf("\n");
    } else {
      if (col > 0 && (col % 6) == 0) {
        printf("\n");
      }
      printf("%-18s", ent->d_name);
      col++;
    }
  }

  printf("\n");
  closedir(dir);
}

extern char* optarg;
extern int optind, opterr, optopt;

int main(int argc, char* argv[]) {  
  printf("ls start\n");
  char* path = "/";
  if (getcwd(buf, sizeof(buf)) == buf) {
    path = buf;
  }
  if (argc > 1) {
    int c;
    while ((c = getopt(argc, argv, "ahl?")) != -1) {
      switch (c) {
        case 'a':
          show_all = 1;
          break;
        case 'h':
          show_nice = 1;
          break;
        case 'l':
          long_mode = 1;
          break;
        case '?':
          show_usage(argc, argv);
          return 0;
      }
    }
    if (optind < argc && argv[optind] != NULL) {
      path = argv[optind];
    }
  }
  ls(path);
  fflush(stdout);
  return 0;
}
