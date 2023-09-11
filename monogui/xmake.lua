target("monogui")
    set_type("cli")
    add_deps("cxx","cxxabi","sdl2-ttf","sdl2-image")

    add_files(
        'tgui/*.cpp',
        'demo/*.cpp'
    ) 
    add_includedirs(  
        '.'
        )

    add_cxxflags(' -DLIBYC -DIS_LITTLE_ENDIAN  -DIS_LINUX  -D_REENTRANT  -DHAVE_STD_STRING_VIEW -fpermissive -DRUN_ENVIRONMENT_LINUX -DCHINESE_SUPPORT -DMOUSE_SUPPORT -DRESMGT_SUPPORT -DMODE_DEBUG ')