target("toybox")
set_type("cli")


add_files(
    'main.c',
    'toys/example/hello.c',
    'toys/posix/ls.c',
    'lib/*.c'
)

add_cflags('-D__linux__ -DCFG_TOYBOX')
