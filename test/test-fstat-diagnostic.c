#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

int main() {
    printf("Testing fstat vs stat...\n");
    
    // Test with a file that should exist
    const char* test_file = "/test.txt";
    
    // First test stat
    struct stat stat_buf;
    printf("Testing stat() for %s\n", test_file);
    int stat_ret = stat(test_file, &stat_buf);
    if (stat_ret == 0) {
        printf("stat() succeeded\n");
        printf("  Mode: %o\n", stat_buf.st_mode);
        printf("  Size: %ld\n", stat_buf.st_size);
        printf("  Is directory: %s\n", S_ISDIR(stat_buf.st_mode) ? "yes" : "no");
        printf("  Is regular file: %s\n", S_ISREG(stat_buf.st_mode) ? "yes" : "no");
    } else {
        printf("stat() failed with errno %d: %s\n", errno, strerror(errno));
    }
    
    // Then test fstat
    int fd = open(test_file, O_RDONLY);
    if (fd >= 0) {
        printf("\nTesting fstat() for %s (fd=%d)\n", test_file, fd);
        int fstat_ret = fstat(fd, &stat_buf);
        if (fstat_ret == 0) {
            printf("fstat() succeeded\n");
            printf("  Mode: %o\n", stat_buf.st_mode);
            printf("  Size: %ld\n", stat_buf.st_size);
            printf("  Is directory: %s\n", S_ISDIR(stat_buf.st_mode) ? "yes" : "no");
            printf("  Is regular file: %s\n", S_ISREG(stat_buf.st_mode) ? "yes" : "no");
        } else {
            printf("fstat() failed with errno %d: %s\n", errno, strerror(errno));
        }
        close(fd);
    } else {
        printf("\nFailed to open %s for fstat test with errno %d: %s\n", 
               test_file, errno, strerror(errno));
    }
    
    // Test with a directory
    const char* test_dir = "/";
    printf("\n\nTesting directory %s\n", test_dir);
    
    // First test stat on directory
    stat_ret = stat(test_dir, &stat_buf);
    if (stat_ret == 0) {
        printf("stat() on directory succeeded\n");
        printf("  Mode: %o\n", stat_buf.st_mode);
        printf("  Is directory: %s\n", S_ISDIR(stat_buf.st_mode) ? "yes" : "no");
        printf("  Is regular file: %s\n", S_ISREG(stat_buf.st_mode) ? "yes" : "no");
    } else {
        printf("stat() on directory failed with errno %d: %s\n", errno, strerror(errno));
    }
    
    // Then test fstat on directory
    fd = open(test_dir, O_RDONLY);
    if (fd >= 0) {
        printf("\nTesting fstat() on directory (fd=%d)\n", fd);
        int fstat_ret = fstat(fd, &stat_buf);
        if (fstat_ret == 0) {
            printf("fstat() on directory succeeded\n");
            printf("  Mode: %o\n", stat_buf.st_mode);
            printf("  Is directory: %s\n", S_ISDIR(stat_buf.st_mode) ? "yes" : "no");
            printf("  Is regular file: %s\n", S_ISREG(stat_buf.st_mode) ? "yes" : "no");
        } else {
            printf("fstat() on directory failed with errno %d: %s\n", errno, strerror(errno));
        }
        close(fd);
    } else {
        printf("\nFailed to open directory for fstat test with errno %d: %s\n", 
               errno, strerror(errno));
    }
    
    return 0;
}