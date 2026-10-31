# coding:utf-8
# *******************************************************************
# * Copyright 2021-present evilbinary
# * 作者: evilbinary on 01/01/20
# * 邮箱: rootdebug@163.com
# ********************************************************************
import sys

target("kernel.elf")
set_kind("binary")

# 【内核禁用浮点/SIMD】与 duck/ya.py 同一条理由：用户线程的 q0-q31 现场在
# 线程切换时才搬运（thread_fp.h / cpu_fp_switch），内核一旦自己用 SIMD 就会
# 把用户现场冲掉。本目录的 pre_launch()/test_ioctl() 实测都被 GCC 生成了
# q 寄存器访问（pre_launch 直接 ldp q0,q1 + stp q0,q1 做 32 字节拷贝）。
add_cflags('-mgeneral-regs-only')

arch=get_arch()
arch_type=get_arch_type()
plat=get_plat()

add_deps(
    'modules',
    'kernel',
    'arch',
    'platform',
    'archcommon',
    'kernelcommon',
    'algorithm',
    'gcc'
)


add_files(
    'module.c',
    'init.c',
    'monitor.c',
    'test.c',
    'cmd.c',
    'main.c',
    'xwin-demo.c'
)

modules =get_config('modules')
# XWIN 模块支持
if 'xwin' in modules:
    add_defines('XWIN_MODULE')

add_rules("kernel-objcopy")

if has_config('single-kernel'):
    add_deps('boot-init.elf')
    add_defines('SINGLE_KERNEL')
    if arch_type == 'xtensa':
        add_cflags("-mlongcalls", "-mtext-section-literals", "-mabi=call0")
        add_files(
                '../../boot/'+arch_type+'/boot.s',
                '../../boot/'+arch_type+'/init.c',
                )
    elif arch_type == 'riscv':
        add_files(
                '../../boot/'+arch_type+'/boot.s',
                '../../boot/'+arch_type+'/init.c',
                )
    elif arch_type in['x86']:
        add_files(
                '../../boot/'+arch_type+'/init.c',
                )
    else:
        add_files(
                '../../boot/'+arch_type+'/boot-'+arch+'.s',
                '../../boot/'+arch_type+'/init-'+arch+'.c',
                )



add_includedirs(
    '../platform/{plat}',
    '../libs/include',
    '../',
    '../libs/include/archcommon'
)




def_arch=arch.replace("-", "_").upper()


def_arch_type=arch_type.replace( "-", "_").upper()

add_defines(def_arch)
add_defines(def_arch_type)

if arch_type=='general' and sys.platform in ['darwin']:
    add_ldflags('-arch    i386 ')
    pass
else:
    add_ldflags("-T"+path.join(os.scriptdir(), "../xlinker/link-{plat}.ld"),  force = True)


add_rules("kernel-objcopy")


target('boot-config')
# add_deps('kernel.elf')

add_files(
    "{buildir}/kernel"
)
add_rules("kernel-gen")
