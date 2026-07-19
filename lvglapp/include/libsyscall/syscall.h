#ifndef __YIYIYA_LIBSYSCALL_SYSCALL_H__
#define __YIYIYA_LIBSYSCALL_SYSCALL_H__

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

// YiYiYa 适配：系统调用相关函数
void libsyscall_init(void);

// 时间相关系统调用
uint64_t OSSysCtrlGetMonoTime(void);
void OSSelfNanoSleep(uint64_t ns);

#ifdef __cplusplus
}
#endif

#endif /* __YIYIYA_LIBSYSCALL_SYSCALL_H__ */
