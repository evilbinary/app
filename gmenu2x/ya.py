target("gmenu")
set_type("cli")
add_deps("cxx","cxxabi","sdl-ttf","sdl-image","musl")

add_files(
    'src/*.cpp'
) 
add_includedirs(  
    'src',
    '.'
    )

add_cxxflags(' -DIS_LINUX  -D_REENTRANT  -DHAVE_STD_STRING_VIEW -DLOG_LEVEL=4 -DENABLE_CLOCK ')

add_cxxflags(
'-ffreestanding -fpic',
        '-fpermissive',
            '-fno-use-cxa-atexit -fno-threadsafe-statics -D_LIBCPP_HAS_MUSL_LIBC -D_LIBCPP_HAS_NO_LIBRARY_ALIGNED_ALLOCATION    -D_LIBCPP_BUILDING_LIBRARY -D_POSIX_C_SOURCE  -D_GNU_SOURCE -D_LIBCPP_HAS_NO_MONOTONIC_CLOCK  -D_LIBCPP_HAS_NO_THREADS -D_LIBCXXABI_HAS_NO_THREADS '
)