# coding:utf-8
# *******************************************************************
# * Copyright 2021-present evilbinary
# * 作者: evilbinary on 01/01/20
# * 邮箱: rootdebug@163.com
# *******************************************************************
# wayland 程序（库本身在 eggs/libwayland 与 eggs/weston，这里只放可执行文件）：
#   wl-smoke      —— 协议自检：server+client 同进程 fork，验证 unix socket 握手、
#                    wl_shm.create_pool 的 SCM_RIGHTS 传递、共享页像素一致。
#   wl-simple-shm —— 上游 clients/simple-shm.c，普通 wayland 客户端。
#   weston        —— 上游 weston 本体：frontend + kiosk-shell，源码不改，
#                    后端/shell 通过 eggs/weston/ya-loader.c 的模块表加载
#                    （本系统静态链接，没有动态加载器，dlopen 走进程内查表）。
#
# 用法（合成器与客户端是两个进程，分开跑）：
#   /bin/weston --backend=headless-backend.so --shell=kiosk-shell.so \
#               --socket=wayland-0 -- /bin/wl-simple-shm
target("wl-smoke")
set_type("cli")
add_deps("wayland")
add_files(
    'wl-smoke.c',
)

# 上游自带的简单客户端：xdg-shell 顶层级 + wl_shm 缓冲。
target("wl-simple-shm")
set_type("cli")
add_deps("wayland")
# 只为 <linux/input.h>（KEY_F11 那个快捷键分支）
add_deps("evdev")
add_cflags('-include eggs/weston/weston-config.h')
add_includedirs(
    '../../eggs/weston',          # simple-shm 里写的是 "shared/os-compatibility.h"
    '../../eggs/weston/shared',
    '../../eggs/weston/include',
    '../../eggs/wayland-protocols/gen',
)
add_files(
    '../../eggs/weston/clients/simple-shm.c',
    '../../eggs/weston/shared/os-compatibility.c',
    '../../eggs/wayland-protocols/gen/xdg-shell-protocol.c',
)

target("weston-bin")
# 动态链接：可执行文件带 PT_INTERP=/lib/ld-musl-<arch>.so.1（按平台取名字，
# 见 xenv/support.py 的 musl_ldso_path），内核 loader 起 musl 的 ldso；
# 后端/shell 模块用 dlopen 加载，符号由本程序（-export-dynamic）提供。
set_type("cli", dynamic=True)
# 目标名叫 weston-bin（库已经占了 "weston" 这个名字），产物文件名仍叫 weston
set_filename("weston")
add_deps("wayland", "weston", "pixman", "xkbcommon")
# frontend/main.c 里 #include <libinput.h> / <libevdev/libevdev.h> /
# <linux/input.h>（backend-drm 分支才用，这里只要头文件路径）
add_deps("input", "evdev")
# frontend 里到处 #include "config.h"，必须先把 weston 自己的配置顶进去
add_cflags('-include eggs/weston/weston-config.h')
add_includedirs(
    '../../eggs/weston/frontend',
    '../../eggs/weston/shared',
    '../../eggs/weston/include',
    '../../eggs/weston/compat',
)
add_files(
    # ---- frontend（上游 main 流程：命令行/配置/weston.ini/模块加载）----
    '../../eggs/weston/frontend/main.c',
    '../../eggs/weston/frontend/executable.c',
    '../../eggs/weston/frontend/config-helpers.c',
    '../../eggs/weston/frontend/weston-screenshooter.c',
    # 注：frontend/text-backend.c（desktop-shell 的输入法/文本面板后端）没编进来，
    # 它依赖 input-method 协议对象，而 kiosk-shell 不用这条路径。
)

# ---- weston 模块（.so）：上游做法是运行期 dlopen 加载，这里照做 ----
# 本系统的 dlopen 来自 musl 的 ldso（libc.so），所以模块就是普通共享库：
# 只编 -fPIC 源码、链成 .so，符号（libweston / frontend 的）留给运行期解析，
# 由 weston 可执行文件的 -export-dynamic 导出。
def weston_module(name, files):
    target(name)
    set_kind("shared")
    set_filename(name)
    add_deps("gcc")
    add_packages(get_config('default_libc'))
    # include 路径与 NEEDED 都来自这些依赖；libweston.so 就是模块的上游库
    add_deps("weston", "wayland", "pixman", "xkbcommon", "evdev")
    add_cflags(
        '-fPIC',
        '-DDUCK -DDLIBC_POSIX',
        '-D__LIB_MUSL__',
        '-nostdlib',
        '-nostdinc',
        '-include eggs/weston/weston-config.h',
    )
    # ymake 的 shared 分支直接用 ld，-shared 必须自己给
    add_ldflags('-shared')
    add_includedirs(
        '../../eggs/weston',
        '../../eggs/weston/frontend',
        '../../eggs/weston/shared',
        '../../eggs/weston/include',
        '../../eggs/weston/libweston',
        '../../eggs/weston/compat',
        '../../eggs/wayland-protocols/gen',
    )
    add_files(*files)

weston_module(
    "kiosk-shell",
    [
        '../../eggs/weston/kiosk-shell/kiosk-shell.c',
        '../../eggs/weston/kiosk-shell/kiosk-shell-grab.c',
    ],
)

weston_module(
    "headless-backend",
    [
        '../../eggs/weston/libweston/backend-headless/headless.c',
        # decorated 模式下 headless 后端会引用 shared/frame.c 的窗口装饰函数，
        # 那套实现依赖 cairo。本系统 decorate=false，这些函数不会被调用，
        # 用同签名的空实现顶上（编进模块自身，免得可执行文件里没人引用而被
        # 链接器丢弃）。
        '../../eggs/weston/compat/frame-stubs.c',
    ],
)

# ---- 运行时布局：把共享库/模块摆到镜像里 weston 期望的路径 ----
#   /lib/libweston.so                       （可执行文件与模块的 NEEDED）
#   /usr/lib/libweston-16/headless-backend.so   （LIBWESTON_MODULEDIR）
#   /usr/lib/weston/kiosk-shell.so              （MODULEDIR）
import os as _os
import shutil as _shutil

_bdir = _os.path.join('build', str(get_plat()), str(get_arch()),
                      str(get_config('mode')))
_deploy = [
    ('libweston.so', 'app/resource/lib/libweston.so'),
    ('libkiosk-shell.so', 'app/resource/usr/lib/weston/kiosk-shell.so'),
    ('libheadless-backend.so',
     'app/resource/usr/lib/libweston-16/headless-backend.so'),
]
for _src_name, _dst in _deploy:
    _src = _os.path.join(_bdir, _src_name)
    if not _os.path.exists(_src):
        continue
    _os.makedirs(_os.path.dirname(_dst), exist_ok=True)
    if (not _os.path.exists(_dst) or
            _os.path.getmtime(_src) > _os.path.getmtime(_dst)):
        _shutil.copyfile(_src, _dst)
