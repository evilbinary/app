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

print("DEBUG: arch=%s, arch_type=%s, plat=%s" % (arch, arch_type, plat))

add_rules("kernel-objcopy")

# ARM platforms always need boot startup for isr_vector
if arch_type == 'arm':
    print("DEBUG: Adding boot files for ARM")
    add_files(
            '../../../boot/'+arch_type+'/boot-'+arch+'.s',
            '../../../boot/'+arch_type+'/init-'+arch+'.c',
            )

if has_config('single-kernel'):
    add_deps('boot-init.elf')
    add_defines('SINGLE_KERNEL')
    print("Building single kernel",arch_type)

    if arch_type in['riscv','xtensa']:
        add_files(
                '../../boot/'+arch_type+'/boot.s',
                '../../boot/'+arch_type+'/init.c',
                )
    elif arch_type in['x86']:
        add_files(
                '../../boot/'+arch_type+'/init.c',
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
    add_ldflags("-T"+path.join(os.scriptdir(), "../../xlinker/link-{plat}.ld"),  force = True)


add_rules("kernel-objcopy")


target('boot-config')
# add_deps('kernel.elf')

add_files(
    "{buildir}/kernel"
)
add_rules("kernel-gen")