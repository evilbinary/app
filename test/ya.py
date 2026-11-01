target("test")
set_type("cli")

add_deps("cmocka")


add_files(
   'test.c'
)

add_cflags('-DHAVE_SIGNAL_H')

add_includedirs("../../eggs/include/")


target("test-musl")
set_type("cli")
add_deps("cmocka")
add_files(
   'test-musl.c'
)
add_cflags('-DHAVE_SIGNAL_H')


target("test-file")
set_type("cli")
add_deps("cmocka")
add_files(
   'test-file.c'
)
add_cflags('-DHAVE_SIGNAL_H -Wno-implicit-function-declaration')

target("test-mem")
set_type("cli")
add_deps("cmocka")
add_files(
   'test-mem.c'
)
add_cflags('-DHAVE_SIGNAL_H')

target("test-mmap-file")
set_type("cli")
add_deps("cmocka")
add_files(
   'test-mmap-file.c'
)
add_cflags('-DHAVE_SIGNAL_H')

target("test-swap")
set_type("cli")
add_deps("cmocka")
add_files(
   'test-swap.c'
)
add_cflags('-DHAVE_SIGNAL_H')

target("test-uncompress")
set_type("cli")
add_deps("cmocka","zlib")
add_files(
   'test-uncompress.c'
)
add_cflags('-DHAVE_SIGNAL_H')

target("test-string")
set_type("cli")
add_deps("cmocka")
add_files(
   'test-string.c'
)
add_cflags('-DHAVE_SIGNAL_H')

target("test-stdio")
set_type("cli")
add_deps("cmocka")
add_files(
   'test-stdio.c'
)
add_cflags('-DHAVE_SIGNAL_H')


target("test-fork")
set_type("cli")
add_deps("cmocka")
add_files(
   'test-fork.c'
)
add_cflags('-DHAVE_SIGNAL_H')
add_includedirs("../../eggs/include/")

target("test-free")
set_type("cli")
add_deps("cmocka")
add_files(
   'test-free.c'
)
add_cflags('-DHAVE_SIGNAL_H')

target("test-sound")
set_type("cli")
add_deps("cmocka")
add_files(
   'test-sound.c'
)
add_cflags('-DHAVE_SIGNAL_H')

target("test-sys")
set_type("cli")
add_deps("cmocka")
add_files(
   'test-sys.c'
)
add_cflags('-DHAVE_SIGNAL_H')

target("test-thread")
set_type("cli")
add_deps("cmocka")
add_files(
   'test-thread.c'
)
add_cflags('-DHAVE_SIGNAL_H')



target("test-time")
set_type("cli")
add_deps("cmocka")
add_files(
   'test-time.c'
)
add_cflags('-DHAVE_SIGNAL_H')


target("test-font")
set_type("cli")
add_deps("cmocka")
add_files(
   'test-font.c'
)
add_cflags('-DHAVE_SIGNAL_H')



target("test-fstat")
set_type("cli")
add_deps("cmocka")
add_files(
   'test-fstat.c'
)
add_cflags('-DHAVE_SIGNAL_H')


target("test-check-file")
set_type("cli")
add_deps("cmocka")
add_files(
   'test-check-file.c'
)
add_cflags('-DHAVE_SIGNAL_H')

target("test-fault")
set_type("cli")
add_files(
   'test-fault.c'
)


target("test-net")
set_type("cli")
add_files(
   'test-net.c'
)
add_cflags('-DHAVE_SIGNAL_H')

target("test-http")
set_type("cli")
add_files(
   'test-http.c'
)
add_cflags('-DHAVE_SIGNAL_H')

target("test-vnc")
set_type("cli")
add_files(
   'test-vnc.c'
)
add_cflags('-DHAVE_SIGNAL_H')

target("test-system")
set_type("cli")
add_files(
   'test-system.c'
)

# AF_UNIX 闭环测试（bind/listen/connect/poll|select/accept/read/write）
target("test-unixsock")
set_type("cli")
add_files(
   'test-unixsock.c'
)

# SCM_RIGHTS fd 传递端到端验证（wayland 地基）：socketpair + fork +
# sendmsg(SCM_RIGHTS) + 子进程 recvmsg 收 fd 后 read。输出 LTP 风格
# TPASS/TFAIL + Summary，可用 tools/ltp_batch.py /bin/test-scmrights 跑。
target("test-scmrights")
set_type("cli")
add_files(
   'test-scmrights.c'
)

# memfd_create + mmap 共享 + SCM_RIGHTS 传递：wl_shm 全链路验收。见 test-memfd.c。
target("test-memfd")
set_type("cli")
add_files(
   'test-memfd.c'
)

# libffi 移植验收（wayland 地基）：ffi_prep_cif + ffi_call 基本正确性。
target("test-ffi")
set_type("cli")
add_deps("ffi")
add_files(
   'test-ffi.c'
)

# libinput 移植验收（path 后端）：stat 设备节点 → libudev(libudev-zero)
# 按 rdev 反查 /sys/dev/char → 读 sysfs capabilities → libevdev 读能力位
# → libinput 识别设备。见 libinput-test.c。
target("input-test")
set_type("cli")
add_packages(get_config('default_libc'))
add_deps("input")
add_files(
   'libinput-test.c'
)

# /dev/fb0 标准 fbdev 兼容层验收（linux/fb.h ABI + ioctl + mmap）。
# 跑法：python3 tools/ltp_batch.py --timeout 30 /bin/fbtest
target("fbtest")
set_type("cli")
add_files(
   'fbtest.c'
)

# foot 服务框架回归测试（注册/发现、往返参数、连续调用、超时、超时后恢复）。
# 需要 /dev/foot（内核 foot 模块）。跑法：
#   python3 tools/ltp_batch.py --timeout 60 /bin/test-foot
target("test-foot")
set_type("cli")
add_deps("foot")
add_files(
   'test-foot.c'
)

# 阻塞唤醒返回值回归（fork / unix socket / pty / poll 短超时）。
# 复现"唤醒发生在登记→切出窗口里，返回值写错帧"导致 syscall 返回参数值。
# 跑法：python3 tools/ltp_batch.py --timeout 180 /bin/test-wake
target("test-wake")
set_type("cli")
add_files(
   'test-wake.c'
)

# foot capability（阶段 4）回归：窗口申请、越权拒绝、派生只减权、撤销失效。
# 跑法：python3 tools/ltp_batch.py --timeout 60 /bin/test-cap
target("test-cap")
set_type("cli")
add_deps("foot")
add_files(
   'test-cap.c'
)

# GPU 服务（gpud）客户端回归：QUERY / FILL / BO / SUBMIT_TFU。
# 自包含：fork + exec /bin/gpud 启动服务端。跑法：
#   python3 tools/ltp_batch.py --timeout 90 /bin/test-gpu
target("test-gpu")
set_type("cli")
add_deps("foot")
add_files(
   'test-gpu.c'
)
