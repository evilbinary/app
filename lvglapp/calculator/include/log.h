#ifndef __YIYIYA_CALCULATOR_LOG__
#define __YIYIYA_CALCULATOR_LOG__

#include <stdio.h>

// YiYiYa 适配：简化日志实现
#define log_debug(format,args...) do { \
	printf("[D][calculator]: " format, ##args); \
} while(0)

#define log_info(format,args...) do { \
	printf("[I][calculator]: " format, ##args); \
} while(0)

#define log_warn(format,args...) do { \
	printf("[W][calculator]: " format, ##args); \
} while(0)

#define log_error(format,args...) do { \
	printf("[E][calculator]: " format, ##args); \
} while(0)

#define log_fatal(format,args...) do { \
	printf("[F][calculator]: " format, ##args); \
	while(1){} \
} while(0)

#endif /* __YIYIYA_CALCULATOR_LOG__ */
