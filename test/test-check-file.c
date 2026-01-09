#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

int main() {
    printf("=== File Existence Check ===\n\n");
    
    // 测试各种路径
    const char* test_paths[] = {
        "/font/Roboto-Regular.ttf",
        "/font",
        "/",
        "/dev/sda",
        "1:/font/Roboto-Regular.ttf",
        "1:/",
        NULL
    };
    
    for (int i = 0; test_paths[i] != NULL; i++) {
        const char* path = test_paths[i];
        printf("Testing path: %s\n", path);
        
        // 尝试 stat
        struct stat file_stat;
        memset(&file_stat, 0, sizeof(file_stat));
        
        int result = stat(path, &file_stat);
        if (result == 0) {
            printf("  stat: Success, size=%ld, mode=0x%08x\n", file_stat.st_size, file_stat.st_mode);
        } else {
            printf("  stat: Failed: %s (errno: %d)\n", strerror(errno), errno);
        }
        
        // 尝试打开
        int fd = open(path, O_RDONLY);
        if (fd >= 0) {
            printf("  open: Success, fd=%d\n", fd);
            close(fd);
        } else {
            printf("  open: Failed: %s (errno: %d)\n", strerror(errno), errno);
        }
        
        printf("\n");
    }
    
    // 列出根目录内容
    printf("=== Listing root directory ===\n");
    int root_fd = open("/", O_RDONLY);
    if (root_fd >= 0) {
        // 尝试读取目录内容
        char buffer[1024];
        int bytes = read(root_fd, buffer, sizeof(buffer));
        printf("Read %d bytes from root directory\n", bytes);
        if (bytes > 0) {
            printf("First few bytes: ");
            for (int j = 0; j < (bytes < 16 ? bytes : 16); j++) {
                printf("%02x ", (unsigned char)buffer[j]);
            }
            printf("\n");
        }
        close(root_fd);
    } else {
        printf("Failed to open root directory: %s (errno: %d)\n", strerror(errno), errno);
    }
    
    return 0;
}