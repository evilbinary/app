#include <dirent.h>
#include <fcntl.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/uio.h>
#include <time.h>
#include <errno.h>

#include "cmocka.h"
#include "stdio.h"

// 定义读取缓冲区大小
#define SMALL_BUFFER_SIZE 1024      // 1KB
#define MEDIUM_BUFFER_SIZE 8192     // 8KB
#define LARGE_BUFFER_SIZE 65536     // 64KB
#define VERY_LARGE_BUFFER_SIZE 262144 // 256KB

// 测试字体文件路径
#define FONT_PATH "/font/Roboto-Regular.ttf"

// 记录读取操作的性能数据
typedef struct {
    size_t buffer_size;
    clock_t start_time;
    clock_t end_time;
    size_t total_bytes_read;
    int read_operations;
} perf_data_t;

// 验证字体文件是否可以正常打开
static void test_font_open(void **state) {
    FILE *fp = fopen(FONT_PATH, "rb");
    assert_non_null(fp);
    
    // 检查文件大小
    int ret = fseek(fp, 0, SEEK_END);
    assert_int_equal(ret, 0);
    
    long file_size = ftell(fp);
    assert_true(file_size > 0);  // 文件应该有内容
    printf("Font file size: %ld bytes\n", file_size);
    
    fclose(fp);
}

// 测试使用小缓冲区(1KB)读取整个字体文件
static void test_font_read_small_buffer(void **state) {
    FILE *fp = fopen(FONT_PATH, "rb");
    assert_non_null(fp);
    
    char buffer[SMALL_BUFFER_SIZE];
    size_t total_read = 0;
    size_t bytes_read;
    int read_count = 0;
    
    perf_data_t perf = {
        .buffer_size = SMALL_BUFFER_SIZE,
        .start_time = clock(),
        .total_bytes_read = 0,
        .read_operations = 0
    };
    
    do {
        bytes_read = fread(buffer, 1, SMALL_BUFFER_SIZE, fp);
        total_read += bytes_read;
        read_count++;
        
        if (bytes_read > 0) {
            // 验证读取的数据不为全零（确保实际读取了有效内容）
            int has_data = 0;
            for (size_t i = 0; i < bytes_read; i++) {
                if (buffer[i] != 0) {
                    has_data = 1;
                    break;
                }
            }
            assert_true(has_data);
        }
    } while (bytes_read > 0);
    
    perf.end_time = clock();
    perf.total_bytes_read = total_read;
    perf.read_operations = read_count;
    
    double elapsed_time = ((double)(perf.end_time - perf.start_time)) / CLOCKS_PER_SEC;
    printf("Small buffer test: %zu bytes in %d reads, %.4f seconds, %.2f KB/s\n",
           total_read, read_count, elapsed_time, 
           (total_read / 1024.0) / elapsed_time);
    
    // 验证读取的字节数与文件大小一致
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    assert_int_equal(total_read, file_size);
    
    fclose(fp);
}

// 测试使用中等缓冲区(8KB)读取整个字体文件
static void test_font_read_medium_buffer(void **state) {
    FILE *fp = fopen(FONT_PATH, "rb");
    assert_non_null(fp);
    
    char buffer[MEDIUM_BUFFER_SIZE];
    size_t total_read = 0;
    size_t bytes_read;
    int read_count = 0;
    
    perf_data_t perf = {
        .buffer_size = MEDIUM_BUFFER_SIZE,
        .start_time = clock(),
        .total_bytes_read = 0,
        .read_operations = 0
    };
    
    do {
        bytes_read = fread(buffer, 1, MEDIUM_BUFFER_SIZE, fp);
        total_read += bytes_read;
        read_count++;
        
        if (bytes_read > 0) {
            // 验证读取的数据不为全零
            int has_data = 0;
            for (size_t i = 0; i < bytes_read; i++) {
                if (buffer[i] != 0) {
                    has_data = 1;
                    break;
                }
            }
            assert_true(has_data);
        }
    } while (bytes_read > 0);
    
    perf.end_time = clock();
    perf.total_bytes_read = total_read;
    perf.read_operations = read_count;
    
    double elapsed_time = ((double)(perf.end_time - perf.start_time)) / CLOCKS_PER_SEC;
    printf("Medium buffer test: %zu bytes in %d reads, %.4f seconds, %.2f KB/s\n",
           total_read, read_count, elapsed_time, 
           (total_read / 1024.0) / elapsed_time);
    
    // 验证读取的字节数与文件大小一致
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    assert_int_equal(total_read, file_size);
    
    fclose(fp);
}

// 测试使用大缓冲区(64KB)读取整个字体文件
static void test_font_read_large_buffer(void **state) {
    FILE *fp = fopen(FONT_PATH, "rb");
    assert_non_null(fp);
    
    char buffer[LARGE_BUFFER_SIZE];
    size_t total_read = 0;
    size_t bytes_read;
    int read_count = 0;
    
    perf_data_t perf = {
        .buffer_size = LARGE_BUFFER_SIZE,
        .start_time = clock(),
        .total_bytes_read = 0,
        .read_operations = 0
    };
    
    do {
        bytes_read = fread(buffer, 1, LARGE_BUFFER_SIZE, fp);
        total_read += bytes_read;
        read_count++;
        
        if (bytes_read > 0) {
            // 验证读取的数据不为全零
            int has_data = 0;
            for (size_t i = 0; i < bytes_read; i++) {
                if (buffer[i] != 0) {
                    has_data = 1;
                    break;
                }
            }
            assert_true(has_data);
        }
    } while (bytes_read > 0);
    
    perf.end_time = clock();
    perf.total_bytes_read = total_read;
    perf.read_operations = read_count;
    
    double elapsed_time = ((double)(perf.end_time - perf.start_time)) / CLOCKS_PER_SEC;
    printf("Large buffer test: %zu bytes in %d reads, %.4f seconds, %.2f KB/s\n",
           total_read, read_count, elapsed_time, 
           (total_read / 1024.0) / elapsed_time);
    
    // 验证读取的字节数与文件大小一致
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    assert_int_equal(total_read, file_size);
    
    fclose(fp);
}

// 测试使用超大缓冲区(256KB)读取整个字体文件
static void test_font_read_very_large_buffer(void **state) {
    FILE *fp = fopen(FONT_PATH, "rb");
    assert_non_null(fp);
    
    char buffer[VERY_LARGE_BUFFER_SIZE];
    size_t total_read = 0;
    size_t bytes_read;
    int read_count = 0;
    
    perf_data_t perf = {
        .buffer_size = VERY_LARGE_BUFFER_SIZE,
        .start_time = clock(),
        .total_bytes_read = 0,
        .read_operations = 0
    };
    
    do {
        bytes_read = fread(buffer, 1, VERY_LARGE_BUFFER_SIZE, fp);
        total_read += bytes_read;
        read_count++;
        
        if (bytes_read > 0) {
            // 验证读取的数据不为全零
            int has_data = 0;
            for (size_t i = 0; i < bytes_read; i++) {
                if (buffer[i] != 0) {
                    has_data = 1;
                    break;
                }
            }
            assert_true(has_data);
        }
    } while (bytes_read > 0);
    
    perf.end_time = clock();
    perf.total_bytes_read = total_read;
    perf.read_operations = read_count;
    
    double elapsed_time = ((double)(perf.end_time - perf.start_time)) / CLOCKS_PER_SEC;
    printf("Very large buffer test: %zu bytes in %d reads, %.4f seconds, %.2f KB/s\n",
           total_read, read_count, elapsed_time, 
           (total_read / 1024.0) / elapsed_time);
    
    // 验证读取的字节数与文件大小一致
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    assert_int_equal(total_read, file_size);
    
    fclose(fp);
}

// 测试使用系统调用read直接读取字体文件
static void test_font_read_system_call(void **state) {
    int fd = open(FONT_PATH, O_RDONLY);
    assert_true(fd >= 0);
    
    char buffer[8192];
    size_t total_read = 0;
    ssize_t bytes_read;
    int read_count = 0;
    
    perf_data_t perf = {
        .buffer_size = 8192,
        .start_time = clock(),
        .total_bytes_read = 0,
        .read_operations = 0
    };
    
    do {
        bytes_read = read(fd, buffer, 8192);
        if (bytes_read < 0) {
            printf("Read error: %s\n", strerror(errno));
            assert_true(bytes_read >= 0);
            break;
        }
        
        total_read += bytes_read;
        read_count++;
        
        if (bytes_read > 0) {
            // 验证读取的数据不为全零
            int has_data = 0;
            for (size_t i = 0; i < bytes_read; i++) {
                if (buffer[i] != 0) {
                    has_data = 1;
                    break;
                }
            }
            assert_true(has_data);
        }
    } while (bytes_read > 0);
    
    perf.end_time = clock();
    perf.total_bytes_read = total_read;
    perf.read_operations = read_count;
    
    double elapsed_time = ((double)(perf.end_time - perf.start_time)) / CLOCKS_PER_SEC;
    printf("System call test: %zu bytes in %d reads, %.4f seconds, %.2f KB/s\n",
           total_read, read_count, elapsed_time, 
           (total_read / 1024.0) / elapsed_time);
    
    // 验证读取的字节数与文件大小一致
    struct stat st;
    fstat(fd, &st);
    assert_int_equal(total_read, st.st_size);
    
    close(fd);
}

// 测试随机访问字体文件的不同部分
static void test_font_random_access(void **state) {
    FILE *fp = fopen(FONT_PATH, "rb");
    assert_non_null(fp);
    
    // 获取文件大小
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    printf("Font file size: %ld bytes\n", file_size);
    
    // 测试随机访问文件的不同位置
    long positions[] = {0, file_size/4, file_size/2, 3*file_size/4, file_size-1024};
    char buffer[1024];
    
    for (int i = 0; i < 5; i++) {
        long pos = positions[i];
        int ret = fseek(fp, pos, SEEK_SET);
        assert_int_equal(ret, 0);
        
        // 读取1KB数据
        size_t bytes_read = fread(buffer, 1, 1024, fp);
        if (pos == file_size - 1024) {
            // 在文件末尾附近，可能读取不到完整的1KB
            assert_true(bytes_read > 0 && bytes_read <= 1024);
        } else {
            assert_int_equal(bytes_read, 1024);
        }
        
        // 验证读取的数据不为全零
        int has_data = 0;
        for (size_t j = 0; j < bytes_read; j++) {
            if (buffer[j] != 0) {
                has_data = 1;
                break;
            }
        }
        assert_true(has_data);
        
        printf("Random access at position %ld: read %zu bytes\n", pos, bytes_read);
    }
    
    fclose(fp);
}

// 测试使用readv系统调用读取字体文件
static void test_font_readv(void **state) {
    int fd = open(FONT_PATH, O_RDONLY);
    assert_true(fd >= 0);
    
    // 准备多个缓冲区
    char buffer1[4096];
    char buffer2[4096];
    char buffer3[4096];
    
    struct iovec iov[3];
    iov[0].iov_base = buffer1;
    iov[0].iov_len = sizeof(buffer1);
    iov[1].iov_base = buffer2;
    iov[1].iov_len = sizeof(buffer2);
    iov[2].iov_base = buffer3;
    iov[2].iov_len = sizeof(buffer3);
    
    perf_data_t perf = {
        .buffer_size = 4096 * 3,
        .start_time = clock(),
        .total_bytes_read = 0,
        .read_operations = 1
    };
    
    ssize_t result = readv(fd, iov, 3);
    assert_true(result > 0);
    
    perf.end_time = clock();
    perf.total_bytes_read = result;
    
    double elapsed_time = ((double)(perf.end_time - perf.start_time)) / CLOCKS_PER_SEC;
    printf("readv test: %zd bytes in 1 readv call, %.4f seconds, %.2f KB/s\n",
           result, elapsed_time, 
           (result / 1024.0) / elapsed_time);
    
    // 验证读取的数据不为全零
    int has_data = 0;
    if (result > 0) {
        for (int i = 0; i < 3; i++) {
            size_t bytes_to_check = (result > (i+1)*4096) ? 4096 : (result % 4096);
            for (size_t j = 0; j < bytes_to_check; j++) {
                if (iov[i].iov_base && ((char*)iov[i].iov_base)[j] != 0) {
                    has_data = 1;
                    break;
                }
            }
            if (has_data) break;
        }
    }
    assert_true(has_data);
    
    close(fd);
}

int main(int argc, char* argv[]) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_font_open),
        cmocka_unit_test(test_font_read_small_buffer),
        cmocka_unit_test(test_font_read_medium_buffer),
        cmocka_unit_test(test_font_read_large_buffer),
        cmocka_unit_test(test_font_read_very_large_buffer),
        cmocka_unit_test(test_font_read_system_call),
        cmocka_unit_test(test_font_random_access),
        cmocka_unit_test(test_font_readv),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}