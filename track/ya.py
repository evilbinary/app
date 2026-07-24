target("track")
set_type("cli")

add_deps("lvgl-8.0.0")

add_files(
    '*.c'
) 


add_cflags('-DLV_LVGL_H_INCLUDE_SIMPLE=1')
