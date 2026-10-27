# coding:utf-8
# *******************************************************************
# * Copyright 2021-present evilbinary
# * 作者: evilbinary on 01/01/20
# * 邮箱: rootdebug@163.com
# ********************************************************************
# 自研极简 X11 桌面组件（不依赖 libX11/GTK/libfm），每个 target 都复用
# x11.c 的协议层，产物装到 app/resource/bin ⇒ 镜像里的 /bin。
#
#   wm       窗口管理器：reparenting + 标题栏拖动/关闭（替代 openbox）
#   wall     桌面背景（替代 feh/hsetroot/pcmanfm 的背景职责）
#   panel    顶部任务栏：Menu 弹出菜单 + 时钟（替代 lxpanel）
#   dock     底部 dock：启动器 + 运行中窗口按钮（wxbar/plank 的角色）
#   session  会话启动器：拉起 wm + wall + panel + dock 并守护（替代 lxsession）
#   xeyes    跟随指针的眼睛（替代 Xorg 的 xeyes）
#   xcalc    计算器（替代 Xorg 的 xcalc）

target("wm")
set_type("cli")
add_files("x11.c", "wm.c")

target("wall")
set_type("cli")
add_files("x11.c", "wall.c")

target("panel")
set_type("cli")
add_files("x11.c", "panel.c")

target("dock")
set_type("cli")
add_files("x11.c", "dock.c")

target("session")
set_type("cli")
add_files("session.c")

target("xeyes")
set_type("cli")
add_files("x11.c", "xeyes.c")

target("xcalc")
set_type("cli")
add_files("x11.c", "xcalc.c")

target("xcalc")
set_type("cli")
add_files("x11.c","xcalc.c")

target("xterm")
set_type("cli")
add_files("x11.c", "xterm.c")

target("xeyes")
set_type("cli")
add_files("x11.c","xeyes.c")
