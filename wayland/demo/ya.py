# coding:utf-8
# mesa-demos 的 Wayland/GLES2 重写版（teapot/gears/toyball/glxinfo/
# fslight/fplight/arbfslight/arbfplight/fire）。
#
# 说明：本板 EGL 只有 OpenGL_ES（没有 GLX/桌面 GL），所以原始
# mesa-demos 的 GLUT/固定管线不能直接编；这些 demo 全部改用
# Wayland + EGL device + GLES2 着色器，效果对齐原 demo。

def wl_demo(name, src, extra=()):
    target(name)
    set_type("cli")
    add_deps("wayland", "EGL", "GLESv2", "gpuclient", "foot",
             "cairo", "png", "pixman")
    add_cflags('-D__unix__', '-include', 'eggs/weston/weston-config.h')
    add_includedirs(
        '../../../eggs/libmesa/include',
        '../../../eggs/libgpu/include',
        '../../../eggs/libdrm/include',
        '../../../eggs/wayland-protocols/gen',
        '../../../eggs/weston',
        '../../../eggs/weston/shared',
        '.',
    )
    add_files(
        src,
        *extra,
        'demo.c',
        # eventdemo 同款 CSD
        '../../../eggs/weston/shared/frame.c',
        '../../../eggs/weston/shared/cairo-util.c',
        '../../../eggs/weston/shared/image-loader.c',
        '../../../eggs/weston/shared/file-util.c',
        '../../../eggs/wayland-protocols/gen/xdg-shell-protocol.c',
    )

wl_demo("wl-teapot", "teapot.c")
wl_demo("wl-gears", "gears.c")
wl_demo("wl-toyball", "toyball.c")
wl_demo("wl-fire", "fire.c")
wl_demo("wl-fslight", "fslight.c", ["lighting_common.c"])
wl_demo("wl-fplight", "fplight.c", ["lighting_common.c"])
wl_demo("wl-arbfslight", "arbfslight.c", ["lighting_common.c"])
wl_demo("wl-arbfplight", "arbfplight.c", ["lighting_common.c"])

# glxinfo 不需要窗口/合成器，只查询 EGL/GL 信息。
target("wl-glxinfo")
set_type("cli")
add_deps("EGL", "GLESv2", "gpuclient", "foot")
add_cflags('-D__unix__')
add_includedirs(
    '../../../eggs/libmesa/include',
    '../../../eggs/libgpu/include',
    '../../../eggs/libdrm/include',
)
add_files('glxinfo.c')

# libgui SCREEN_MODE_WAYLAND 对接验证：应用只调 screen API，
# Wayland 窗口/wl_shm/提交全部在 libgui 内部完成。
target("wl-gui-host")
set_type("cli")
add_deps("gui", "wayland")
add_cflags('-D__unix__')
add_files('gui-host.c')
