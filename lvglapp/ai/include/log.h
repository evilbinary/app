#ifndef __TRANQUILOS_CORE_AI_LOG__
#define __TRANQUILOS_CORE_AI_LOG__

#include "stdio.h"
#include "stddef.h"
#include "stdint.h"
#include "stdarg.h"
#include "libkernel/capcall.h"

#define log_debug(format,args...) do { \
	printf("[%llu][%llu][D][ai]: "format, (unsigned long long)OSSysCtrlGetCpuId(), (unsigned long long)OSSysCtrlGetMonoTime(), ##args); \
} while(0);

#define log_info(format,args...) do { \
	printf("[%llu][%llu][I][ai]: "format, (unsigned long long)OSSysCtrlGetCpuId(), (unsigned long long)OSSysCtrlGetMonoTime(), ##args); \
} while(0);

#define log_warn(format,args...) do { \
	printf("[%llu][%llu][W][ai]: "format, (unsigned long long)OSSysCtrlGetCpuId(), (unsigned long long)OSSysCtrlGetMonoTime(), ##args); \
} while(0);

#define log_error(format,args...) do { \
	printf("[%llu][%llu][E][ai]: "format, (unsigned long long)OSSysCtrlGetCpuId(), (unsigned long long)OSSysCtrlGetMonoTime(), ##args); \
} while(0);

#define log_fatal(format,args...) do { \
	printf("[%llu][%llu][F][ai][%s(%d)]: "format, (unsigned long long)OSSysCtrlGetCpuId(), (unsigned long long)OSSysCtrlGetMonoTime(), __func__, __LINE__, ##args); \
	while(1){};															\
} while(0);

#endif /* __TRANQUILOS_CORE_AI_LOG__ */
