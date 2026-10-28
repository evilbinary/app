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
#                验证事件循环、输出 enable、协议 global 注册。
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
