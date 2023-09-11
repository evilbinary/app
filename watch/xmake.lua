target("watch")
    set_type("cli")

    add_deps("lvgl")

    add_files(
        '*.c',
        'page/*.c'
    ) 

    add_includedirs(
        ".",
        "page"
    )

    add_cflags('-DLV_LVGL_H_INCLUDE_SIMPLE=1')
