target("testwm")
set_type("cli")
add_deps("sdl")
add_files('testwm.c')


target("testoverlay")
set_type("cli")
add_deps("sdl")
add_files('testoverlay.c')

target("testoverlay2")
set_type("cli")
add_deps("sdl")
add_files('testoverlay2.c')

target("testbitmap")
set_type("cli")
add_deps("sdl")
add_files('testbitmap.c')


target("testcursor")
set_type("cli")
add_deps("sdl")
add_files('testcursor.c')


target("testjoystick")
set_type("cli")
add_deps("sdl")
add_files('testjoystick.c')

target("testkeys")
set_type("cli")
add_deps("sdl")
add_files('testkeys.c')


target("showimage")
set_type("cli")
add_deps("sdl","sdl-image")
add_files('showimage.c')

