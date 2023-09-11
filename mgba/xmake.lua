target("mgba")
    set_type("cli")

    add_deps("sdl2","sdl2-image")

    add_files(
        'src/platform/sdl/main.c',
        'src/platform/sdl/sw-sdl2.c',
        'src/platform/sdl/sdl-events.c',
        'src/platform/sdl/sdl-audio.c',

        'src/version.c',

       'src/arm/*.c',
       'src/arm/*/*.c',
       'src/core/*.c',
       'src/debugger/*.c',
        --'src/feature/*.c',

        --'src/feature/gui/*.c',
       'src/feature/video-logger.c',
       'src/feature/commandline.c',

       'src/gb/*.c',
       'src/gb/*/*.c',
       'src/gba/*.c',
       'src/gba/*/*.c',
       'src/sm83/*.c',
       'src/sm83/debugger/*.c',

        --'src/util/gui/*.c',

       'src/util/vfs/*.c',
       'src/util/*.c',

       'src/libgba/*.c',

        'src/third-party/zlib/contrib/minizip/ioapi.c',
        'src/third-party/zlib/contrib/minizip/unzip.c',
        'src/third-party/zlib/contrib/minizip/zip.c',

        -- 'src/third-party/zlib/contrib/minizip/miniunz.c',
        -- #'src/third-party/zlib/contrib/minizip/minizip.c',
    

       'src/third-party/inih/*.c',

       'src/third-party/blip_buf/*.c'
    )

    add_includedirs(
        'include/mgba-util',
        'src/third-party/zlib/contrib/',
        'src/third-party/zlib/contrib/minizip/',
        'src',
        'include',
        '.',
        'sys/sdl2/',
        'src/libgba'
        
    )
    add_cflags('-DUSE_FILE32API ')

