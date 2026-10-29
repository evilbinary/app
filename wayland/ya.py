# coding:utf-8
# *******************************************************************
# * Copyright 2021-present evilbinary
# * 作者: evilbinary on 01/01/20
# * 邮箱: rootdebug@163.com
# ********************************************************************
# wayland 集成层：
#   wl-smoke  —— 阶段 1：server+client 同进程 fork，验证 unix socket 握手、
#                wl_shm.create_pool 的 SCM_RIGHTS fd 传递、共享页像素一致。
#   wl-weston —— 阶段 2：把 libweston 拉起来（pixman 渲染器 + headless 后端），
#                验证事件循环、输出 enable、协议 global 注册、client 像素进
#                output 影子缓冲。
#   weston    —— 阶段 3：上游 weston 本体（frontend + kiosk-shell），静态链接
#                进 libweston，不经 dlopen 加载后端/shell。
# 库本身在 eggs/libwayland（核心库）与 eggs/weston（libweston），这里只放我们的代码。
target("wl-smoke")
set_type("cli")
add_deps("wayland")
add_files(
    'wl-smoke.c',
)

target("wl-weston")
set_type("cli")
add_deps("wayland", "weston")
add_files(
    'wl-weston.c',
)

# 上游自带的简单客户端：xdg-shell 顶层级 + wl_shm 缓冲，用来验证
# "客户端 surface 经 kiosk-shell 上屏"（weston 用 `-- /bin/wl-simple-shm` 托管启动）。
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
set_type("cli")
add_deps("wayland", "weston", "pixman", "xkbcommon")
# frontend/main.c 里 #include <libinput.h>（backend-drm 分支才用，这里只要头）
add_deps("input")
# frontend 里到处 #include "config.h"，必须先把 weston 自己的配置顶进去
add_cflags('-include eggs/weston/weston-config.h')
add_includedirs(
    '../../eggs/weston/frontend',
    '../../eggs/weston/shared',
    '../../eggs/weston/include',
    '../../eggs/weston/compat',
)
add_files(
    '../../eggs/weston/frontend/main.c',
    '../../eggs/weston/frontend/executable.c',
    '../../eggs/weston/frontend/config-helpers.c',
    '../../eggs/weston/frontend/text-backend.c',
    '../../eggs/weston/frontend/weston-screenshooter.c',
    '../../eggs/weston/kiosk-shell/kiosk-shell.c',
    '../../eggs/weston/kiosk-shell/kiosk-shell-grab.c',
)
