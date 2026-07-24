#ifndef __TRANQUILOS_APPS_BOUNCE_LOG_H__
#define __TRANQUILOS_APPS_BOUNCE_LOG_H__

#include "stdio.h"
#include "libkernel/capcall.h"

#define log_debug(format,args...) do { \
	printf("[%llu][%llu][D][bounce]: " format, \
	       (unsigned long long)OSSysCtrlGetCpuId(), \
	       (unsigned long long)OSSysCtrlGetMonoTime(), ##args); \
} while (0)

#define log_info(format,args...) do { \
	printf("[%llu][%llu][I][bounce]: " format, \
	       (unsigned long long)OSSysCtrlGetCpuId(), \
	       (unsigned long long)OSSysCtrlGetMonoTime(), ##args); \
} while (0)

#define log_warn(format,args...) do { \
	printf("[%llu][%llu][W][bounce]: " format, \
	       (unsigned long long)OSSysCtrlGetCpuId(), \
	       (unsigned long long)OSSysCtrlGetMonoTime(), ##args); \
} while (0)

#define log_error(format,args...) do { \
	printf("[%llu][%llu][E][bounce]: " format, \
	       (unsigned long long)OSSysCtrlGetCpuId(), \
	       (unsigned long long)OSSysCtrlGetMonoTime(), ##args); \
} while (0)

#endif /* __TRANQUILOS_APPS_BOUNCE_LOG_H__ */
