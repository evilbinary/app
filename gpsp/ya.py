target("gpsp")
set_type("app")
add_deps("sdl","sdl-image","gui","sdl-ttf")

add_files(
    'arm/arm_stub.c',
    'arm/warm.c',
    'arm/video_blend.S',

    'chip/*.c',
    'main.c',
    'cpu.c', 'memory.c', 'video.c', 'input.c', 'sound.c',
	'cpu_threaded.c', 'gui.c', 'cheats.c', 'zip.c',

    'yiyiya/rpi.c'
)

install_res('app','game_config.txt')

arch_type=get_arch_type()
if arch_type=='arm':
    add_files('arm/arm_stub.S')
    add_cflags('-DARM_ARCH')
elif arch_type=='x86':
    add_files('x86/x86_stub.S')
    add_cflags('-DPC_BUILD')
# add_cflags("-O2")

add_includedirs(
    './include',
    '.',
    './yiyiya'
)
add_cflags('-DIS_LITTLE_ENDIAN  -DIS_LINUX -D_GNU_SOURCE=1 -D_REENTRANT -DYIYIYA_BUILD ')

