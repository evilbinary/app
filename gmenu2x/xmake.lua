target("gmenu")
    set_type("cli")
    add_deps("cxx","cxxabi","sdl-ttf","sdl-image")

    add_files(
        'src/*.cpp'
    ) 
    add_includedirs(  
        'src',
        '.'
        )

    add_cxxflags(' -DIS_LINUX  -D_REENTRANT  -DHAVE_STD_STRING_VIEW -DLOG_LEVEL=4 -DENABLE_CLOCK ')