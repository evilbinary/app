target("ymain")
set_type("cli")

add_deps("yui","jsmodule","sdl2")

add_files(
    'main.c',
) 

add_deps("yui","jsmodule","sdl2")

add_files(
    'main.c',
) 



# add_cflags('-UCONFIG_PRINTF_RNDN -D_GNU_SOURCE -DUSE_FILE32API  -Ieggs/libquickjs -DCONFIG_BIGNUM')

add_includedirs(
    './include',
    '.',
    'libyui/src'
)

target("ymario") 
(
    add_deps( "jsmodule-mario","yui", "mario",),
    set_type("cli"),
    add_files("main.c")
)


target("yqjs") 
(
    add_deps( "jsmodule-quickjs","yui", "quickjs",),
    set_type("cli"),
    add_files("main.c")
)
