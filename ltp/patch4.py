import os, re
os.chdir("/Users/evil/dev/c/YiYiYa/app/ltp")

# config.h: musl 已有/可遮蔽的结构探测宏
s = open("config.h").read()
add = """#define HAVE_STRUCT_IOVEC 1
#define HAVE_EPOLL_PWAIT 1
#define HAVE_STRUCT_FANOTIFY_EVENT_INFO_HEADER 1
#define HAVE_STRUCT_FANOTIFY_EVENT_INFO_FID 1
#define HAVE_STRUCT_SCHED_ATTR 1
#define HAVE_GETRANDOM 1
"""
if "HAVE_STRUCT_IOVEC" not in s:
    s = s.replace("#define HAVE_SETNS 1", add + "#define HAVE_SETNS 1")
    open("config.h", "w").write(s)

# types.h: __kernel_* 补全（ipcbuf.h 等）
t = open("compat/linux/types.h").read()
extra = """typedef unsigned int __kernel_key_t;
typedef unsigned int __kernel_uid32_t;
typedef unsigned int __kernel_gid32_t;
typedef unsigned int __kernel_mode_t;
typedef unsigned long __kernel_ino_t;
typedef unsigned int __kernel_dev_t;
typedef unsigned int __kernel_nlink_t;
typedef long __kernel_daddr_t2;
typedef unsigned short __kernel_ipc_pid_t;
typedef long __kernel_time64_t;
"""
if "__kernel_key_t" not in t:
    t = t.replace("#endif\n", extra + "#endif\n")
    open("compat/linux/types.h", "w").write(t)

# 旧 main 调用改成经函数指针（兼容无参 main）
g = open("gen.py").read()
if "ltp_run_%s(int argc, char **argv) {\\n" in g and "(int(*)(int, char **))" not in g:
    g = g.replace('"  return LTP_OLD_MAIN_%s(argc, argv);\\n"',
                  '"  return ((int (*)(int, char **))LTP_OLD_MAIN_%s)(argc, argv);\\n"')
    open("gen.py", "w").write(g)
print("patched")
