target("kernel.elf")
set_kind("binary")

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


add_files("*.c") 

add_includedirs(
    '../../../duck/kernel',
    '../../../duck',
    '../../../duck/init',
    '../../../duck/libs/include'
    )

arch=get_arch()
arch_type=get_arch_type()
plat=get_plat()

add_rules("kernel-objcopy")

if has_config('single-kernel'):
    add_deps('boot-init.elf')
    add_defines('SINGLE_KERNEL')
    if arch_type in['riscv','xtensa']:
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