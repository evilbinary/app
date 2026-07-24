target("microui")
set_type("cli")

add_deps("gui")

add_files(
    'main.c',
    'microui.c',
    'renderer.c'
) 

