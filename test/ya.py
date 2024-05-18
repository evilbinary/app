target("test")
set_type("cli")

add_deps("cmocka")


add_files(
   'test.c'
)

add_cflags('-DHAVE_SIGNAL_H')

add_includedirs("../../eggs/include/")


target("test-musl")
set_type("cli")
add_deps("cmocka")
add_files(
   'test-musl.c'
)
add_cflags('-DHAVE_SIGNAL_H')


target("test-file")
set_type("cli")
add_deps("cmocka")
add_files(
   'test-file.c'
)
add_cflags('-DHAVE_SIGNAL_H -Wno-implicit-function-declaration')

target("test-mem")
set_type("cli")
add_deps("cmocka")
add_files(
   'test-mem.c'
)
add_cflags('-DHAVE_SIGNAL_H')

target("test-uncompress")
set_type("cli")
add_deps("cmocka","zlib")
add_files(
   'test-uncompress.c'
)
add_cflags('-DHAVE_SIGNAL_H')

target("test-string")
set_type("cli")
add_deps("cmocka")
add_files(
   'test-string.c'
)
add_cflags('-DHAVE_SIGNAL_H')

target("test-stdio")
set_type("cli")
add_deps("cmocka")
add_files(
   'test-stdio.c'
)
add_cflags('-DHAVE_SIGNAL_H')


target("test-fork")
set_type("cli")
add_deps("cmocka")
add_files(
   'test-fork.c'
)
add_cflags('-DHAVE_SIGNAL_H')
add_includedirs("../../eggs/include/")

target("test-free")
set_type("cli")
add_deps("cmocka")
add_files(
   'test-free.c'
)
add_cflags('-DHAVE_SIGNAL_H')

target("test-sound")
set_type("cli")
add_deps("cmocka")
add_files(
   'test-sound.c'
)
add_cflags('-DHAVE_SIGNAL_H')

target("test-sys")
set_type("cli")
add_deps("cmocka")
add_files(
   'test-sys.c'
)
add_cflags('-DHAVE_SIGNAL_H')

target("test-thread")
set_type("cli")
add_deps("cmocka")
add_files(
   'test-thread.c'
)
add_cflags('-DHAVE_SIGNAL_H')



target("test-time")
set_type("cli")
add_deps("cmocka")
add_files(
   'test-time.c'
)
add_cflags('-DHAVE_SIGNAL_H')