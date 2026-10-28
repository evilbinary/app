# coding:utf-8
# *******************************************************************
# * Copyright 2021-present evilbinary
# * 作者: evilbinary on 01/01/20
# * 邮箱: rootdebug@163.com
# ********************************************************************
# wayland 集成层（阶段 1）：冒烟测试用一个二进制跑 server + client 两个进程，
# 验证 unix socket 握手 → wl_shm.create_pool 的 SCM_RIGHTS → 共享页像素一致。
# 库本身在 eggs/libwayland（上游源码 + 预生成协议），这里只放我们的代码。
target("wl-smoke")
set_type("cli")
add_deps("wayland")
add_files(
    'wl-smoke.c',
)
