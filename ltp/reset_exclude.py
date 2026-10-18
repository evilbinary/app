import os
os.chdir("/Users/evil/dev/c/YiYiYa/app/ltp")
os.rename("gen/exclude.txt", "gen/exclude.txt.bak2")
open("gen/exclude.txt", "w").write("""lib/tlibio.c
lib/tst_device.c
lib/tst_fd.c
lib/tst_rtctime.c
lib/tst_af_alg.c
lib/tst_crypto.c
lib/tst_netlink.c
lib/tst_wallclock.c
lib/tst_ioctl.c
lib/tst_netdevice.c
testcases/kernel/syscalls/nftw/test_func64.c
testcases/kernel/syscalls/nftw/tools64.c
testcases/kernel/syscalls/nftw/test_func.c
testcases/kernel/syscalls/nftw/tools.c
testcases/kernel/syscalls/nftw/lib.c
testcases/kernel/syscalls/nftw/lib64.c
testcases/kernel/syscalls/nftw/test.c
testcases/kernel/syscalls/nftw/test64.c
testcases/kernel/syscalls/init_module/init_module.c
testcases/kernel/syscalls/finit_module/finit_module.c
testcases/kernel/syscalls/delete_module/dummy_del_mod.c
testcases/kernel/syscalls/delete_module/dummy_del_mod_dep.c
testcases/kernel/syscalls/move_pages/move_pages_support.c
testcases/kernel/syscalls/setrlimit/setrlimit06.c
""")
print("reset done")
