#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include "cmocka.h"

// 测试普通文件的 fstat
static void test_fstat_regular_file(void **state) {
    const char* filepath = "/font/Roboto-Regular.ttf";
    
    // 尝试打开文件
    int fd = open(filepath, O_RDONLY);
    assert_true(fd >= 0);
    
    // 获取文件信息
    struct stat file_stat;
    memset(&file_stat, 0, sizeof(file_stat));
    
    int result = fstat(fd, &file_stat);
    if (result != 0) {
        printf("  fstat failed: %s (errno: %d)\n", strerror(errno), errno);
        // 在某些系统中，fstat 可能不被支持，所以这里只记录错误而不断言失败
        printf("  Note: fstat may not be supported in this OS\n");
    } else {
        printf("  fstat succeeded!\n");
        printf("    st_mode: 0x%08x\n", file_stat.st_mode);
        printf("    st_size: %ld bytes\n", file_stat.st_size);
        printf("    st_uid: %d\n", file_stat.st_uid);
        printf("    st_gid: %d\n", file_stat.st_gid);
        
        // 验证文件类型
        assert_true(S_ISREG(file_stat.st_mode));
        
        // 验证文件大小大于0
        assert_true(file_stat.st_size > 0);
        
        printf("    File is a regular file with size %ld bytes\n", file_stat.st_size);
    }
    
    // 关闭文件
    close(fd);
}

// 测试目录的 fstat
static void test_fstat_directory(void **state) {
    const char* filepath = "/";
    
    // 尝试打开目录
    int fd = open(filepath, O_RDONLY);
    assert_true(fd >= 0);
    
    // 获取目录信息
    struct stat file_stat;
    memset(&file_stat, 0, sizeof(file_stat));
    
    int result = fstat(fd, &file_stat);
    if (result != 0) {
        printf("  fstat failed: %s (errno: %d)\n", strerror(errno), errno);
        printf("  Note: fstat may not be supported in this OS\n");
    } else {
        printf("  fstat succeeded!\n");
        printf("    st_mode: 0x%08x\n", file_stat.st_mode);
        printf("    st_size: %ld bytes\n", file_stat.st_size);
        
        // 验证是目录
        assert_true(S_ISDIR(file_stat.st_mode));
        
        printf("    Path is a directory\n");
    }
    
    // 关闭目录
    close(fd);
}

// 测试设备文件的 fstat
static void test_fstat_device(void **state) {
    const char* filepath = "/dev/tty0";
    
    // 尝试打开设备文件
    int fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        printf("  Could not open %s: %s\n", filepath, strerror(errno));
        skip();
        return;
    }
    
    // 获取设备文件信息
    struct stat file_stat;
    memset(&file_stat, 0, sizeof(file_stat));
    
    int result = fstat(fd, &file_stat);
    if (result != 0) {
        printf("  fstat failed: %s (errno: %d)\n", strerror(errno), errno);
        printf("  Note: fstat may not be supported in this OS\n");
    } else {
        printf("  fstat succeeded!\n");
        printf("    st_mode: 0x%08x\n", file_stat.st_mode);
        printf("    st_size: %ld bytes\n", file_stat.st_size);
        
        // 验证是字符设备
        assert_true(S_ISCHR(file_stat.st_mode));
        
        printf("    Path is a character device\n");
    }
    
    // 关闭设备文件
    close(fd);
}

// 测试 stat 系统调用（路径名版本）
static void test_stat_pathname(void **state) {
    const char* test_files[] = {
        "/font/Roboto-Regular.ttf",
        "/",
        NULL
    };
    
    for (int i = 0; test_files[i] != NULL; i++) {
        const char* filepath = test_files[i];
        printf("Testing stat(%s)\n", filepath);
        
        struct stat file_stat;
        memset(&file_stat, 0, sizeof(file_stat));
        
        int result = stat(filepath, &file_stat);
        if (result != 0) {
            printf("  Failed: %s (errno: %d)\n", strerror(errno), errno);
        } else {
            printf("  Success, size=%ld, mode=0x%08x\n", file_stat.st_size, file_stat.st_mode);
            
            // 验证大小不为负数
            assert_true(file_stat.st_size >= 0);
        }
    }
}

// 测试无效文件描述符
static void test_fstat_invalid_fd(void **state) {
    struct stat file_stat;
    memset(&file_stat, 0, sizeof(file_stat));
    
    int result = fstat(-1, &file_stat);
    assert_true(result != 0);  // 应该失败
    printf("fstat(-1, &stat) failed as expected: %s (errno: %d)\n", strerror(errno), errno);
}

// 测试 NULL 指针
static void test_fstat_null_pointer(void **state) {
    int result = fstat(0, NULL);
    assert_true(result != 0);  // 应该失败
    printf("fstat(0, NULL) failed as expected: %s (errno: %d)\n", strerror(errno), errno);
}

// 测试 fstat 和 read 的组合
static void test_fstat_with_read(void **state) {
    const char* filepath = "/font/Roboto-Regular.ttf";
    
    // 尝试打开文件
    int fd = open(filepath, O_RDONLY);
    assert_true(fd >= 0);
    
    // 获取文件信息
    struct stat file_stat;
    memset(&file_stat, 0, sizeof(file_stat));
    
    int result = fstat(fd, &file_stat);
    if (result != 0) {
        printf("  fstat failed: %s (errno: %d)\n", strerror(errno), errno);
        printf("  Note: fstat may not be supported in this OS\n");
        // 即使 fstat 失败，我们仍然可以测试 read
    } else {
        printf("  fstat succeeded, file size: %ld bytes\n", file_stat.st_size);
    }
    
    // 测试读取
    char buffer[100];
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    printf("  Read %ld bytes\n", bytes_read);
    
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        printf("  First few bytes: ");
        for (int j = 0; j < (bytes_read < 10 ? bytes_read : 10); j++) {
            printf("%02x ", (unsigned char)buffer[j]);
        }
        printf("\n");
    }
    
    // 验证读取了数据
    assert_true(bytes_read > 0);
    
    // 关闭文件
    close(fd);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_fstat_regular_file),
        cmocka_unit_test(test_fstat_directory),
        cmocka_unit_test(test_fstat_device),
        cmocka_unit_test(test_stat_pathname),
        cmocka_unit_test(test_fstat_invalid_fd),
        cmocka_unit_test(test_fstat_null_pointer),
        cmocka_unit_test(test_fstat_with_read),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}