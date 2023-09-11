-- target("xtrack")
--     set_type("cli")
--     add_deps("cxx","cxxabi","lvgl","gui")

--     add_files(

--     'main.cpp',
--     'APP/Pages/*.cpp',
--     'APP/**/**/*.cpp',
--     'APP/**/**/*.c',

--     'App/Utils/lv_img_png/PNGdec/src/*.cpp',
--     'App/Utils/MapConv/GPS_Transform/*.c',
--     'App/Utils/MapConv/TileSystem/*.cpp',
--     'App/Utils/DataCenter/PingPongBuffer/*.c',

--     'App/*.cpp',
--     'App/Resource/*.cpp',


--     'HAL/*.cpp',
--     'MillisTaskManager/*.cpp'


--     ) 

--     add_cflags(' -D_POSIX_C_SOURCE -DLV_LVGL_H_INCLUDE_SIMPLE=1 ')
--     add_cxxflags(' -D_POSIX_C_SOURCE -DLV_LVGL_H_INCLUDE_SIMPLE=1 ')

--     -- add_cxflags(' -D_GLIBCPP_USE_C99 -DLIBYC -DIS_LITTLE_ENDIAN  -DIS_LINUX  -D_REENTRANT  -DHAVE_STD_STRING_VIEW -fpermissive -DRUN_ENVIRONMENT_LINUX -DCHINESE_SUPPORT -DMOUSE_SUPPORT -DRESMGT_SUPPORT -DMODE_DEBUG -DENABLE_CLOCK ')

--     add_includedirs(
--         'APP',
--         'APP/Utils/Time/',
--         'App/Utils/ArduinoJson',
--         'App/Utils/ArduinoJson/src',
--         'App/Utils/GPX_Parser/'
--     )