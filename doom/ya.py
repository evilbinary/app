target("doom")
set_type("app")
add_deps("sdl","sdl-image")
add_cflags('-Dlinux')
add_files('*.c')

