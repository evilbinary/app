#ifndef __TRANQUILOS_CORE_BOOT_ANIMATION_LOG__
#define __TRANQUILOS_CORE_BOOT_ANIMATION_LOG__

#include "stdio.h"
#include "stdarg.h"
#include "stddef.h"
#include "stdint.h"
#include "libkernel/capcall.h"

#define log_debug(format,args...) do { \
	printf("[%llu][%llu][D][boot_animation]: "format, (unsigned long long)OSSysCtrlGetCpuId(), (unsigned long long)OSSysCtrlGetMonoTime(), ##args); \
} while(0);

#define log_info(format,args...) do { \
	printf("[%llu][%llu][I][boot_animation]: "format, (unsigned long long)OSSysCtrlGetCpuId(), (unsigned long long)OSSysCtrlGetMonoTime(), ##args); \
} while(0);

#define log_warn(format,args...) do { \
	printf("[%llu][%llu][W][boot_animation]: "format, (unsigned long long)OSSysCtrlGetCpuId(), (unsigned long long)OSSysCtrlGetMonoTime(), ##args); \
} while(0);

#define log_error(format,args...) do { \
	printf("[%llu][%llu][E][boot_animation]: "format, (unsigned long long)OSSysCtrlGetCpuId(), (unsigned long long)OSSysCtrlGetMonoTime(), ##args); \
} while(0);

#define log_fatal(format,args...) do { \
	printf("[%llu][%llu][F][boot_animation][%s(%d)]: "format, (unsigned long long)OSSysCtrlGetCpuId(), (unsigned long long)OSSysCtrlGetMonoTime(), __func__, __LINE__, ##args); \
	while(1){}; \
} while(0);

#endif /* __TRANQUILOS_CORE_BOOT_ANIMATION_LOG__ */
