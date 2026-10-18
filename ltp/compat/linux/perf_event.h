/* 兼容垫片：lib/tst_fd.c 只用到 perf_event_attr 的少数常量与字段
 * （perf_event_open 在 YiYiYa 内核未实现，open 时返回错误走 TCONF 路径），
 * 这里给出最小布局兼容定义，完整 UAPI 头不必要。 */
#ifndef COMPAT_LINUX_PERF_EVENT_H
#define COMPAT_LINUX_PERF_EVENT_H

#define PERF_TYPE_HARDWARE		0
#define PERF_TYPE_SOFTWARE		1
#define PERF_TYPE_TRACEPOINT		2
#define PERF_TYPE_HW_CACHE		3
#define PERF_TYPE_RAW			4
#define PERF_TYPE_BREAKPOINT		5
#define PERF_TYPE_MAX			6

#define PERF_COUNT_SW_CPU_CLOCK		0
#define PERF_COUNT_SW_TASK_CLOCK	1
#define PERF_COUNT_SW_PAGE_FAULTS	2
#define PERF_COUNT_SW_CONTEXT_SWITCHES	3
#define PERF_COUNT_SW_CPU_MIGRATIONS	4
#define PERF_COUNT_SW_PAGE_FAULTS_MIN	5
#define PERF_COUNT_SW_PAGE_FAULTS_MAJ	6
#define PERF_COUNT_SW_ALIGNMENT_FAULTS	7
#define PERF_COUNT_SW_EMULATION_FAULTS	8

struct perf_event_attr {
	unsigned int type;
	unsigned int size;
	unsigned long long config;
	union {
		unsigned long long sample_period;
		unsigned long long sample_freq;
	};
	unsigned long long sample_type;
	unsigned long long read_format;
	unsigned long long disabled :  1,
			  inherit   :  1,
			  punctual  :  1,
			  precise_ip:  2,
			  exclude_user   : 1,
			  exclude_kernel : 1,
			  exclude_hv     : 1,
			  exclude_idle   : 1,
			  mmap           : 1,
			  comm           : 1,
			  freq           : 1,
			  inherit_stat   : 1,
			  enable_on_exec : 1,
			  task           : 1,
			  watermark      : 1,
			  reserved       : 48;
};

#endif /* COMPAT_LINUX_PERF_EVENT_H */
