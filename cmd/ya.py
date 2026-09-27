# coding:utf-8
# *******************************************************************
# * Copyright 2021-present evilbinary
# * 作者: evilbinary on 01/01/20
# * 邮箱: rootdebug@163.com
# ********************************************************************
target("cat")
set_type("cli")

add_files("cat.c") 

target("date")
set_type("cli")
add_files("date.c") 

target("echo")
set_type("cli")
add_files("echo.c") 

target("hexdump")
set_type("cli")
add_files("hexdump.c") 


target("kill")
set_type("cli")
add_files("kill.c") 

target("ls")
set_type("cli")
add_files("ls.c") 

target("shell")
set_type("cli")
add_files("shell.c") 

# musl 的 system()/popen() 硬编码 exec "/bin/sh"
# （eggs/libmusl/src/process/system.c：posix_spawn(&pid, "/bin/sh", ...,
#   {"sh", "-c", cmd, 0})）。shell.c 的 main 已实现 `sh -c` 分支（注释即写明
# "供 system()/popen 等使用"），但上面的 target 名是 shell ⇒ 产物只有
# bin/shell，镜像里没有 /bin/sh ⇒ YUI.spawn()（backend_spawn→system()）必失败：
#   not found file sh in bin code 4
#   sys open file /bin/sh error / sys exec file not found /bin/sh
# 这里用同一份源码再出一个 POSIX 标准名 sh，bin/shell 保留（init.c 注释里
# 用的是它，交互启动也用它）。二者编译选项相同，共用 objs/app/cmd/shell.o。
target("sh")
set_type("cli")
add_files("shell.c") 


target("touch")
set_type("cli")
add_files("touch.c") 

target("config")
set_type("cli")
add_files("config.c") 