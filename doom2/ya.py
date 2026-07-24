target("doom2")
set_type("app")
add_deps("sdl","sdl-image")
add_cflags('-Dlinux -D_THREAD_SAFE')
add_files('*.c')

