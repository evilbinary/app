target("showimage2")
set_type("cli")

add_deps("sdl2","sdl2-image")

add_files('showimage.c')


target("showfont")
set_type("cli")

add_deps("sdl2","sdl2-ttf")

add_files('showfont.c')

target("sdl2.elf")
set_type("cli")

set_filename("sdl2")

add_deps("sdl2")

add_files('main.c')


# target("player")
#     set_type("cli")

#     add_deps("sdl2")
#     add_packages("ffmpeg")

#     add_files('player.c')
