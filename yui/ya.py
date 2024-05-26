
target("yui")
set_type("cli")

add_deps("quickjs")

add_files(
    'duck.c'
) 


add_cflags('-UCONFIG_PRINTF_RNDN -D_GNU_SOURCE -DUSE_FILE32API  -Ieggs/libquickjs -DCONFIG_BIGNUM')

add_includedirs(
    './include',
    '.',
    'eggs/libquickjs'
)