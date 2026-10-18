# coding:utf-8
# LTP (Linux Test Project) 集成
# LTP 源码树（ltp/）原样入仓零修改；所有适配在外围新增文件：
#   gen.py            扫描测试源码，生成 wrapper/注册表/文件清单
#   gen_syscalls.py   生成 compat/lapi/syscalls.h（LTP 构建期产物）
#   config.h          手工顶替 ./configure 产物（-include 注入）
#   runner.c          统一 runner：每测试 fork 隔离执行 + 汇总
#   compat/           musl 缺失的内核头垫片 + 被排除 lib 文件的等价实现
# 重新生成：python3 gen.py（编译失败文件加进 gen/exclude.txt 后重跑）
# 注意：import os as _pyos —— 不能直接 import os，会遮蔽 ymake 注入的
# os.scriptdir。
import os.path as _p

_here = _p.abspath(os.scriptdir())

# ---------- libltp 静态库 ----------
target("ltpcore")
set_kind("static")
add_packages(get_config("default_libc"))

_lib_files = []
_flib = _p.join(_here, "gen", "files-lib.txt")
if _p.exists(_flib):
    with open(_flib) as fh:
        _lib_files = [x.strip() for x in fh if x.strip()]
if not _lib_files:
    _lib_files = ["ltp/lib/tst_res.c"]

_batch = []
for f in _lib_files:
    _batch.append(f)
    if len(_batch) >= 50:
        add_files(*_batch)
        _batch = []
if _batch:
    add_files(*_batch)

# include 用绝对路径：相对路径会在构建目录里和输出二进制 ltp 撞名
add_includedirs(_here, _p.join(_here, "ltp/include"),
                _p.join(_here, "ltp/include/old"),
                _p.join(_here, "ltp/lib"),
                _p.join(_here, "compat"))
add_cflags("-include", "%s/config.h" % _here,
           "-Wno-unused-parameter", "-Wno-sign-compare",
           "-Wno-implicit-function-declaration")

# ---------- 测试 runner ----------
# 两段式构建（符号本地化，见 localize.py）：
#   pass A: ya -b ltpwrap   —— 编译所有 wrapper .c 成静态库目标
#   localize.py             —— objcopy 把每个 wrapper .o 除 ltp_get_*/ltp_run_*
#                              外全部本地化 → gen/objs/<fid>.o
#   pass B: ya -b ltp       —— 链接 gen/objs/*.o + helpers + registry + runner
# 原因：LTP 各测试本是独立二进制，run/setup 等全局符号在单 runner 里撞名。
target("ltpwrap")
set_kind("static")
add_packages(get_config("default_libc"))

_wrap_files = []
_fwrap = _p.join(_here, "gen", "files-wrappers.txt")
if _p.exists(_fwrap):
    with open(_fwrap) as fh:
        _wrap_files = [x.strip() for x in fh if x.strip()]
if not _wrap_files:
    _wrap_files = ["gen/wrappers/getpid_getpid01.c"]

_batch = []
for f in _wrap_files:
    _batch.append(f)
    if len(_batch) >= 50:
        add_files(*_batch)
        _batch = []
if _batch:
    add_files(*_batch)

add_includedirs(_here, _p.join(_here, "ltp/include"),
                _p.join(_here, "ltp/include/old"),
                _p.join(_here, "ltp/lib"),
                _p.join(_here, "compat"))
add_cflags("-include", "%s/config.h" % _here,
           "-Wno-unused-parameter", "-Wno-sign-compare",
           "-Wno-implicit-function-declaration")

target("ltp")
set_type("cli")
add_deps("ltpcore")

_test_files = []
_ftest = _p.join(_here, "gen", "files-tests.txt")
if _p.exists(_ftest):
    with open(_ftest) as fh:
        _test_files = [x.strip() for x in fh if x.strip()]

_batch = []
for f in _test_files:
    _batch.append(f)
    if len(_batch) >= 50:
        add_files(*_batch)
        _batch = []
if _batch:
    add_files(*_batch)

add_files("runner.c")
add_includedirs(_here, _p.join(_here, "ltp/include"),
                _p.join(_here, "ltp/include/old"),
                _p.join(_here, "ltp/lib"),
                _p.join(_here, "compat"))
add_cflags("-include", "%s/config.h" % _here,
           "-Wno-unused-parameter", "-Wno-sign-compare",
           "-Wno-implicit-function-declaration")
