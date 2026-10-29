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
# 动态链接：可执行文件带 PT_INTERP=/lib/ld-musl-aarch64.so.1，内核 loader 起
# musl 的 ldso；后端/shell 模块用 dlopen 加载，符号由本程序（-rdynamic）提供。
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
    '../../eggs/weston/frontend/text-backend.c',
    '../../eggs/weston/frontend/weston-screenshooter.c',
    # ---- kiosk shell（上游源码，入口 wet_shell_init）----
    '../../eggs/weston/kiosk-shell/kiosk-shell.c',
    '../../eggs/weston/kiosk-shell/kiosk-shell-grab.c',
    # ---- 静态模块表：dlopen("kiosk-shell.so") 之类的进程内解析 ----
    '../../eggs/weston/ya-loader.c',
)
