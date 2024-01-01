target("gnuboy")
set_type("cli")

add_deps("sdl2","sdl2-image","gui")

add_files(
    'src/*.c',
    'sys/sdl2/*.c',
    'lib/gz/*.c',
    'lib/xz/*.c',
    'sys/yiyiya/*.c'
)

add_includedirs(
    'sys/sdl2/',
    './include',
    '.',
)
add_cflags('-DIS_LITTLE_ENDIAN  -DIS_LINUX -D_GNU_SOURCE=1 -D_REENTRANT ')

