target("emapp_lcd")

set_kind("static")

add_files("*.c") 
add_includedirs(
    '../../../duck/kernel',
    '../../../duck',
    '../../../duck/init',
    '../../../duck/libs/include'
    )