target("quickjs")
set_type("lib")


add_files(
    'quickjs.c',
    'libregexp.c',
    'libunicode.c',
    'cutils.c',
    'quickjs-libc.c',
    'libbf.c'
) 

add_cflags(' -UCONFIG_PRINTF_RNDN -D_GNU_SOURCE -DUSE_FILE32API  -Iapp/quickjs -DCONFIG_BIGNUM')


target("qjsc")
set_type("cli")

add_deps("quickjs")

add_files(
    'qjsc.c'
) 

add_cflags(' -UCONFIG_PRINTF_RNDN -D_GNU_SOURCE -DUSE_FILE32API  -Iapp/quickjs -DCONFIG_BIGNUM')


target("qjs")
set_type("cli")
add_deps("quickjs")

add_files(
    'qjs.c',
    'qjscalc.c',
    'repl.c'
) 

add_cflags(' -UCONFIG_PRINTF_RNDN -D_GNU_SOURCE -DUSE_FILE32API  -Iapp/quickjs -DCONFIG_BIGNUM')

