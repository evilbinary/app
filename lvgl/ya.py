target("lvgl.elf")
set_type("cli")
set_filename("lvgl")

add_deps("lvgl")

add_files(
    'lvgl.c',
    'lv_demo_widgets.c',
    'assets/img_lvgl_logo.c',
    'assets/img_demo_widgets_avatar.c',
    'assets/img_clothes.c',
    'lv_demo_benchmark.c',
    'assets/lv_font_montserrat_16_compr_az.c',
    'assets/img_benchmark_cogwheel_argb.c',
    'assets/lv_font_montserrat_12_compr_az.c',
    'assets/lv_font_montserrat_28_compr_az.c',
    'assets/img_benchmark_cogwheel_rgb.c',

    'assets/img_benchmark_cogwheel_indexed16.c',
    'assets/img_benchmark_cogwheel_chroma_keyed.c',
    'assets/img_benchmark_cogwheel_alpha16.c'

) 


add_cflags('-DLV_LVGL_H_INCLUDE_SIMPLE=1')
