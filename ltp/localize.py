#!/usr/bin/env python3
# LTP wrapper 符号本地化（两段式构建的中间步骤，见 ya.py 头部注释）。
#
# pass A（ya -b ltpwrap）已把 gen/wrappers/*.c 编译到
# build/<plat>/<arch>/<mode>/objs/app/ltp/gen/wrappers/<fid>.o。
# 本脚本对每个 wrapper .o 执行：
#   objcopy --keep-global-symbols=<keepfile> <in.o> gen/objs/<fid>.o
# keepfile 只保留 ltp_get_<fid> / ltp_run_<fid>（registry 引用），
# 其余全局符号（LTP 各测试独立二进制遗留的 run/setup/MSG...）全部本地化，
# 消除单 runner 链接时的 multiple definition。
# 之后 pass B（ya -b ltp）直接链接 gen/objs/*.o。
#
# 用法: python3 localize.py <build-objs-dir>

import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
GEN = os.path.join(HERE, "gen")
OBJS_OUT = os.path.join(GEN, "objs")

OBJCOPY = "aarch64-none-elf-objcopy"


def main():
    if len(sys.argv) < 2:
        print("usage: localize.py <build-objs-dir>")
        return 1
    wrap_dir = os.path.join(sys.argv[1], "app", "ltp", "gen", "wrappers")
    if not os.path.isdir(wrap_dir):
        print("no wrapper obj dir: " + wrap_dir)
        return 1
    # ymake 把 add_files 的 gen/objs/x.o 映射为
    # build/<plat>/<arch>/<mode>/objs/app/ltp/gen/objs/x.o —— 链接用的是
    # build 侧路径，本地化产物两处都要放（源侧给 is_modify 比对，build 侧给 ld）
    out_build_dir = os.path.join(sys.argv[1], "app", "ltp", "gen", "objs")

    os.makedirs(OBJS_OUT, exist_ok=True)
    os.makedirs(out_build_dir, exist_ok=True)
    n = 0
    for fn in sorted(os.listdir(wrap_dir)):
        if not fn.endswith(".o"):
            continue
        fid = fn[:-2]
        # 统一保留两个名字：struct 只有 ltp_get_x、oldmain 两者都有，
        # keep 列表里不存在的名字只是无效过滤项，无害
        keep = ["ltp_get_" + fid, "ltp_run_" + fid]
        kf = os.path.join(OBJS_OUT, ".keep_" + fid)
        with open(kf, "w") as fh:
            fh.write("\n".join(keep) + "\n")
        dst = os.path.join(OBJS_OUT, fid + ".o")
        src = os.path.join(wrap_dir, fn)
        # 输入没变化且输出已存在则跳过（ya 增量编译只重编修改过的 .o）
        if not (os.path.exists(dst) and os.path.getmtime(dst) >= os.path.getmtime(src)):
            subprocess.check_call([OBJCOPY, "--keep-global-symbols=" + kf, src, dst])
            n += 1
        os.remove(kf)
        # 同步到 build objs 目录（链接器实际取用的路径）
        bdst = os.path.join(out_build_dir, fid + ".o")
        if not os.path.exists(bdst) or os.path.getmtime(bdst) < os.path.getmtime(dst):
            import shutil
            shutil.copy2(dst, bdst)
    total = len([x for x in os.listdir(OBJS_OUT) if x.endswith(".o")])
    print("localized %d objs, total %d" % (n, total))
    return 0


if __name__ == "__main__":
    sys.exit(main())
