#!/usr/bin/env python3
# LTP 集成生成器：扫描 LTP 原始源码树（不改任何 LTP 文件），
# 生成 wrapper TU + 注册表 + 文件清单，供 ya.py 的 add_files 使用。
#
# 分类规则：
#   struct 文件（新 API）：含 "static struct tst_test test" —— 用
#     -DTST_NO_DEFAULT_MAIN 屏蔽 LTP 自带 main，wrapper #include 原文件
#     后暴露 getter 返回 &test；
#   main 文件（旧 API）：含 "int main(" —— wrapper 里 #define main 改名，
#     暴露 run 入口；
#   helper：其余 .c（被同目录测试引用的辅助 TU），直接作为普通对象编入。
#
# 输出（全部在 app/ltp/gen/ 下，可随时重新生成）：
#   gen/wrappers/<id>.c    每个测试一个 wrapper
#   gen/registry.c         {name, getter} 表（按名字排序）
#   gen/files-lib.txt      ltp-lib  target 的源文件清单（lib/*.c）
#   gen/files-tests.txt    ltp-tests target 的源文件清单（wrappers + helpers）
#   gen/exclude.txt        编译失败排除表（自动累积，重新生成时保留）

import os
import re
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
LTP = os.path.join(ROOT, "ltp")
SYSCALLS = os.path.join(LTP, "testcases", "kernel", "syscalls")
GEN = os.path.join(ROOT, "gen")
WRAPPERS = os.path.join(GEN, "wrappers")

# gen/exclude.txt：之前构建失败的文件（相对 syscalls 的路径），重新生成时跳过
EXCLUDE_FILE = os.path.join(GEN, "exclude.txt")


def load_exclude():
    if not os.path.exists(EXCLUDE_FILE):
        return set()
    ex = set()
    with open(EXCLUDE_FILE) as f:
        for x in f:
            x = x.strip()
            if not x:
                continue
            ex.add(x)
            # 兼容带 testcases/kernel/syscalls/ 前缀的写法
            pre = "testcases/kernel/syscalls/"
            if x.startswith(pre):
                ex.add(x[len(pre):])
    return ex


def main():
    os.makedirs(WRAPPERS, exist_ok=True)
    excluded = load_exclude()

    # 不删除旧 wrapper（批量删除受保护）：只覆盖写。被排除测试的过期
    # wrapper 不进 files-tests.txt 就不会被编译，磁盘残留无害。

    struct_files = []  # (id, relpath)
    main_files = []    # (id, relpath)
    helpers = []       # relpath

    # 扫描根：(目录, id 前缀)。syscalls 是历史主目录（无前缀）；
    # pty 来自 LTP full 包迁移（testcases/kernel/pty，id 前缀 pty_）。
    # 扫描 testcases/kernel 下【全部】测试目录：syscalls 用裸名（历史 id），
    # 其它目录用目录名做前缀避免重名（fs_readahead01、mem_oom01 …）。
    # include/ 只有头文件；lib/ 是测试私有辅助库（无测试入口）。
    # 整目录跳过：
    #   kvm / mce-test   —— x86 专属（vmx/svm、机器检查），aarch64 上不可运行；
    #   sound            —— 需要 <sound/asound.h>（alsa-lib 开发包）
    # 子目录跳过：device-drivers/v4l（V4L2 大 uapi + 真实摄像头）、fs/xfs（xfsprogs 头）
    SKIP_ROOT_NAMES = ("kvm", "mce-test", "sound")
    #   include/         —— device-drivers/include 下是"被 #include 的片段"，
    #                       不是独立 TU（单独编译必然语法错）
    SKIP_REL_PREFIXES = ("v4l/", "xfs/", "include/")

    scan_roots = [(SYSCALLS, "")]
    kernel_dir = os.path.join(LTP, "testcases", "kernel")
    for name in sorted(os.listdir(kernel_dir)):
        sub = os.path.join(kernel_dir, name)
        if not os.path.isdir(sub) or name in ("syscalls", "include", "lib"):
            continue
        if name in SKIP_ROOT_NAMES:
            continue
        scan_roots.append((sub, name))

    for scan_root, scan_prefix in scan_roots:
        if not os.path.isdir(scan_root):
            continue
        for dirpath, dirnames, filenames in os.walk(scan_root):
            dirnames.sort()
            for fn in sorted(filenames):
                if not fn.endswith(".c"):
                    continue
                full = os.path.join(dirpath, fn)
                rel = os.path.relpath(full, LTP)
                rel_root = os.path.relpath(full, scan_root)
                if rel_root.startswith(SKIP_REL_PREFIXES):
                    continue
                key = (scan_prefix + "/" + rel_root) if scan_prefix else rel_root
                if key in excluded:
                    continue
                with open(full, "r", errors="ignore") as fh:
                    src = fh.read()
                has_struct = "static struct tst_test test" in src
                has_main = re.search(r"\bint\s+main\s*\(", src) is not None
                # 标识符只保留 [A-Za-z0-9_]：LTP 里有 cpuctl_fj_cpu-hog.c
                # 这类带 '-' 的文件名，直接拼进 C 标识符会语法错。
                fid = re.sub(r"[^A-Za-z0-9_]+", "_", key[:-2])
                if has_struct:
                    struct_files.append((fid, rel))
                elif has_main:
                    main_files.append((fid, rel))
                else:
                    helpers.append(rel)

    # ---- 生成 wrappers ----
    entries = []
    for fid, rel in struct_files:
        w = os.path.join(WRAPPERS, fid + ".c")
        with open(w, "w") as fh:
            fh.write(
                "/* 自动生成：勿改。壳 TU 暴露 %s 的 tst_test 结构 */\n"
                "#define TST_NO_DEFAULT_MAIN\n"
                '#include "ltp/%s"\n'
                "#include <tst_test.h>\n"
                "struct tst_test *ltp_get_%s(void);\n"
                "struct tst_test *ltp_get_%s(void) { return &test; }\n"
                % (fid, rel, fid, fid)
            )
        entries.append((fid, "struct", rel))

    for fid, rel in main_files:
        w = os.path.join(WRAPPERS, fid + ".c")
        with open(w, "w") as fh:
            fh.write(
                "/* 自动生成：勿改。旧 API 测试，宏改名 main 并暴露 run 入口 */\n"
                "#define main LTP_OLD_MAIN_%s\n"
                '#include "ltp/%s"\n'
                "#undef main\n"
                "int LTP_OLD_MAIN_%s();\n"
                "int ltp_run_%s(int argc, char **argv) {\n"
                "  return ((int (*)(int, char **))LTP_OLD_MAIN_%s)(argc, argv);\n"
                "}\n"
                "struct tst_test *ltp_get_%s(void);\n"
                "struct tst_test *ltp_get_%s(void) { return (void *)0; }\n"
                % (fid, rel, fid, fid, fid, fid, fid)
            )
        entries.append((fid, "oldmain", rel))

    # ---- registry.c ----
    entries.sort(key=lambda e: e[0])
    # id -> 源文件映射（排除循环用）
    with open(os.path.join(GEN, "idmap.txt"), "w") as fh:
        for fid, kind, rel in entries:
            fh.write("%s %s\n" % (fid, rel))
    with open(os.path.join(GEN, "registry.c"), "w") as fh:
        fh.write("/* 自动生成：勿改。LTP 测试注册表 */\n")
        fh.write("#define TST_NO_DEFAULT_MAIN\n")
        fh.write('#include <tst_test.h>\n#include "ltp_runner.h"\n\n')
        for fid, kind, rel in entries:
            if kind == "struct":
                fh.write("struct tst_test *ltp_get_%s(void);\n" % fid)
            else:
                fh.write("int ltp_run_%s(int argc, char **argv);\n" % fid)
        fh.write("\nconst ltp_entry_t ltp_tests[] = {\n")
        for fid, kind, rel in entries:
            if kind == "struct":
                fh.write('  {"%s", ltp_get_%s, 0},\n' % (fid, fid))
            else:
                fh.write('  {"%s", 0, ltp_run_%s},\n' % (fid, fid))
        fh.write("};\n")
        fh.write("const int ltp_test_count = %d;\n" % len(entries))

    # ---- idmap（wrapper id -> 源文件相对路径），供排除循环反查 ----
    with open(os.path.join(GEN, "idmap.txt"), "w") as fh:
        for fid, kind, rel in sorted(entries):
            fh.write("%s %s\n" % (fid, rel))

    # ---- 文件清单 ----
    with open(os.path.join(GEN, "files-lib.txt"), "w") as fh:
        for fn in sorted(os.listdir(os.path.join(LTP, "lib"))):
            if fn.endswith(".c") and ("lib/" + fn) not in excluded:
                fh.write("ltp/lib/" + fn + "\n")
        # per-dir 辅助库（libs/<name>/*.c）：LTP 的 tse_*.h 的实现体，
        # 例如 libs/uinput 提供 open_uinput()/create_input_device()。
        libs_dir = os.path.join(LTP, "libs")
        if os.path.isdir(libs_dir):
            for name in sorted(os.listdir(libs_dir)):
                sub = os.path.join(libs_dir, name)
                if not os.path.isdir(sub):
                    continue
                for fn in sorted(os.listdir(sub)):
                    if not fn.endswith(".c"):
                        continue
                    rel = "libs/" + name + "/" + fn
                    if rel not in excluded:
                        fh.write("ltp/" + rel + "\n")
        # 构建期生成物/被排除文件的兼容实现
        for c in ("compat/tst_umount.c", "compat/tst_syscall.c",
                  "compat/tst_device_shim.c", "compat/tst_wallclock_shim.c",
                  "compat/tst_missing.c"):
            fh.write(c + "\n")
    with open(os.path.join(GEN, "files-wrappers.txt"), "w") as fh:
        # wrapper .c 由 ltpwrap 静态库目标编译（pass A），随后 localize.py
        # 用 objcopy 把各 wrapper 内部符号本地化成 gen/objs/<fid>.o
        for fid, kind, rel in entries:
            fh.write("gen/wrappers/" + fid + ".c\n")
    with open(os.path.join(GEN, "files-tests.txt"), "w") as fh:
        # ltp 目标直接吃本地化后的 .o（ymake 对 .o 跳过编译直接进链接），
        # 解决各测试独立二进制遗留的非 static run/setup 等全局符号撞名
        for fid, kind, rel in entries:
            fh.write("gen/objs/" + fid + ".o\n")
        for rel in sorted(helpers):
            fh.write("ltp/" + rel + "\n")
        fh.write("gen/registry.c\n")

    print("struct=%d oldmain=%d helpers=%d total_tests=%d"
          % (len(struct_files), len(main_files), len(helpers), len(entries)))


if __name__ == "__main__":
    main()
