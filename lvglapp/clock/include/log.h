#ifndef __TRANQUILOS_CORE_CLOCK_LOG__
#define __TRANQUILOS_CORE_CLOCK_LOG__

#include "stdio.h"
#include "stdarg.h"
#include "stddef.h"
#include "stdint.h"
#include "libkernel/capcall.h"

#define log_debug(format,args...) do { \
	printf("[%llu][%llu][D][clock]: "format, (unsigned long long)OSSysCtrlGetCpuId(), (unsigned long long)OSSysCtrlGetMonoTime(), ##args); \
} while(0);

#define log_info(format,args...) do { \
	printf("[%llu][%llu][I][clock]: "format, (unsigned long long)OSSysCtrlGetCpuId(), (unsigned long long)OSSysCtrlGetMonoTime(), ##args); \
} while(0);

#define log_warn(format,args...) do { \
	printf("[%llu][%llu][W][clock]: "format, (unsigned long long)OSSysCtrlGetCpuId(), (unsigned long long)OSSysCtrlGetMonoTime(), ##args); \
} while(0);

#define log_error(format,args...) do { \
	printf("[%llu][%llu][E][clock]: "format, (unsigned long long)OSSysCtrlGetCpuId(), (unsigned long long)OSSysCtrlGetMonoTime(), ##args); \
} while(0);

#define log_fatal(format,args...) do { \
	printf("[%llu][%llu][F][clock][%s(%d)]: "format, (unsigned long long)OSSysCtrlGetCpuId(), (unsigned long long)OSSysCtrlGetMonoTime(), __func__, __LINE__, ##args); \
	while(1){}; \
} while(0);

#endif /* __TRANQUILOS_CORE_CLOCK_LOG__ */
