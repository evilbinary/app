
target("qjsc")
set_type("cli")

add_deps("quickjs")

add_files(
    'qjsc.c'
) 


add_cflags('-Ieggs/libquickjs -UCONFIG_PRINTF_RNDN -D_GNU_SOURCE -DUSE_FILE32API  -Iapp/quickjs -DCONFIG_BIGNUM')

add_includedirs(
    './include',
    '.',
    'libquickjs'
)

target("qjs")
set_type("cli")
add_deps("quickjs")

add_files(
    'qjs.c',
    'qjscalc.c',
    'repl.c'
) 

add_cflags('-Ieggs/libquickjs -UCONFIG_PRINTF_RNDN -D_GNU_SOURCE -DUSE_FILE32API  -Iapp/quickjs -DCONFIG_BIGNUM')

add_includedirs(
    './include',
    '.',
    'libquickjs'
)
