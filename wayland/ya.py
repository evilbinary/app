# coding:utf-8
# *******************************************************************
# * Copyright 2021-present evilbinary
# * 作者: evilbinary on 01/01/20
# * 邮箱: rootdebug@163.com
# *******************************************************************
# wayland 程序（库本身在 eggs/libwayland 与 eggs/weston，这里只放可执行文件）。
#
# 两套产物，靠 --wayland-desktop 分开，可以同时存在：
#   A 精简套（--wayland）            —— 全程零 cairo/pango
#     weston-kiosk / weston-headless / weston（通用）、kiosk-shell.so、
#     headless-backend.so、fbdev-backend.so、wl-smoke、wl-simple-shm
#   B 图形套（--wayland-desktop）    —— 额外一套基于 cairo 的桌面
#     weston-desktop / weston-ivi、desktop-shell.so、ivi-shell.so、
#     weston-desktop-shell、ivi-shell-user-interface
#
# 可执行文件（合成器）按"默认 shell + 默认后端"分成几个开箱即用的变体：
#   weston-kiosk    kiosk-shell  + fbdev       单窗口全屏
#   weston-headless kiosk-shell  + headless    无显示，合成/联调
#   weston-desktop  desktop-shell + fbdev      多窗口桌面
#   weston-ivi      ivi-shell    + fbdev       车机式布局
#   weston          不写死，靠 --shell=/--backend= 或 weston.ini（上游行为）
#
# 用法（合成器与客户端是两个进程，分开跑）：
#   /bin/weston-kiosk --socket=wayland-0 -- /bin/wl-simple-shm
#   /bin/weston --backend=headless-backend.so --shell=kiosk-shell.so \
#               --socket=wayland-0 -- /bin/wl-simple-shm
target("wl-smoke")
set_type("cli")
add_deps("wayland")
add_files(
    'wl-smoke.c',
)

# 上游自带的简单客户端：xdg-shell 顶层级 + wl_shm 缓冲。
target("wl-simple-shm")
set_type("cli")
add_deps("wayland")
# 只为 <linux/input.h>（KEY_F11 那个快捷键分支）
add_deps("evdev")
add_cflags('-include eggs/weston/weston-config.h')
# 【必须 -fPIC】下面编的 shared/os-compatibility.c 与 libweston.so（eggs/weston
# 的 target weston，shared 类型、-fPIC）是同一个源文件，而 ymake 的中间产物是
# 按"源文件路径"唯一命名的（build/.../objs/eggs/weston/shared/os-compatibility.o），
# 不带目标名。两处编译参数不一致时，后写的那份会覆盖前者：
# 可执行文件（无 -fPIC）后编 ⇒ libweston.so 链接时报
#   relocation R_AARCH64_ADR_PREL_PG_HI21 against symbol `stderr' ...
#   recompile with -fPIC
# （raspi5 首次全量构建时在 shared/config-parser.o 上炸过。）
# 统一按 -fPIC 编，可执行文件用 PIC 目标文件链接是非 PIE 也完全没问题。
add_cflags('-fPIC')
add_includedirs(
    '../../eggs/weston',          # simple-shm 里写的是 "shared/os-compatibility.h"
    '../../eggs/weston/shared',
    '../../eggs/weston/include',
    '../../eggs/wayland-protocols/gen',
)
add_files(
    '../../eggs/weston/clients/simple-shm.c',
    '../../eggs/weston/shared/os-compatibility.c',
    '../../eggs/wayland-protocols/gen/xdg-shell-protocol.c',
)

# ---- weston 可执行文件 ----
# 动态链接：可执行文件带 PT_INTERP=/lib/ld-musl-<arch>.so.1（按平台取名字，
# 见 xenv/support.py 的 musl_ldso_path），内核 loader 起 musl 的 ldso；
# 后端/shell 模块用 dlopen 加载，符号由本程序（-export-dynamic）提供。
#
# 前端源码只有一份，按"默认 shell + 默认后端"派生出开箱即用的固定组合变体：
# 每个变体是 weston_variants/<名字>.c —— 先 #define WESTON_DEFAULT_SHELL /
# WESTON_DEFAULT_BACKEND，再 #include frontend/main.c。默认值只是省参数，
# 命令行和 weston.ini 都能覆盖，功能上没有任何裁剪。
#
#   weston          —— 上游行为：不写死，默认 shell=desktop、后端自动选
#   weston-kiosk    —— kiosk-shell  + fbdev 后端    （A 套：单窗口全屏）
#   weston-headless —— kiosk-shell  + headless 后端  （A 套：无显示，合成/联调）
#   weston-desktop  —— desktop-shell + fbdev 后端    （B 套：多窗口桌面）
#   weston-ivi      —— ivi-shell     + fbdev 后端    （B 套：车机式布局）
def weston_exe(name, filename, main_src='../../eggs/weston/frontend/main.c'):
    target(name)
    set_type("cli", dynamic=True)
    set_filename(filename)
    add_deps("wayland", "weston", "pixman", "xkbcommon")
    # frontend/main.c 里 #include <libinput.h> / <libevdev/libevdev.h> /
    # <linux/input.h>（backend-drm 分支才用，这里只要头文件路径）
    add_deps("input", "evdev")
    # frontend 里到处 #include "config.h"，必须先把 weston 自己的配置顶进去
    add_cflags('-include eggs/weston/weston-config.h')
    add_includedirs(
        '../../eggs/weston/frontend',
        '../../eggs/weston/shared',
        '../../eggs/weston/include',
        '../../eggs/weston/compat',
    )
    add_files(
        # ---- frontend（上游 main 流程：命令行/配置/weston.ini/模块加载）----
        # 变体传的是 weston_variants/*.c（同样的 main.c，只是先定义了默认值宏）
        main_src,
        '../../eggs/weston/frontend/executable.c',
        '../../eggs/weston/frontend/config-helpers.c',
        '../../eggs/weston/frontend/weston-screenshooter.c',
        # desktop-shell 需要的输入法/文本面板后端：desktop-shell.so 里
        # input-panel.c 引用了 text_backend_init/destroy（模块符号从可执行文件
        # 解析）。它只依赖 input-method / text-input 两个协议对象（libweston.so
        # 里已经有），没有 cairo/pango —— 所以所有变体都编进来，A/B 套的
        # weston 本体保持同一份源码，换套不会漏符号。kiosk-shell 不走这条路径，
        # 多这几个符号无害。
        '../../eggs/weston/frontend/text-backend.c',
        '../../eggs/wayland-protocols/gen/text-input-unstable-v1-protocol.c',
        '../../eggs/wayland-protocols/gen/input-method-unstable-v1-protocol.c',
    )

# 目标名叫 weston-bin（库已经占了 "weston" 这个名字），产物文件名仍叫 weston
weston_exe("weston-bin", "weston")

# A 套：只依赖 kiosk-shell / headless / fbdev 三个模块，全程无 cairo
weston_exe("weston-kiosk", "weston-kiosk", './weston_variants/kiosk.c')
weston_exe("weston-headless", "weston-headless", './weston_variants/headless.c')

# ---- weston 模块（.so）：上游做法是运行期 dlopen 加载，这里照做 ----
# 本系统的 dlopen 来自 musl 的 ldso（libc.so），所以模块就是普通共享库：
# 只编 -fPIC 源码、链成 .so，符号（libweston / frontend 的）留给运行期解析，
# 由 weston 可执行文件的 -export-dynamic 导出。
def weston_module(name, files, includedirs=None):
    target(name)
    set_kind("shared")
    set_filename(name)
    add_deps("gcc")
    add_packages(get_config('default_libc'))
    # include 路径与 NEEDED 都来自这些依赖；libweston.so 就是模块的上游库
    add_deps("weston", "wayland", "pixman", "xkbcommon", "evdev")
    add_cflags(
        '-fPIC',
        '-DDUCK -DDLIBC_POSIX',
        '-D__LIB_MUSL__',
        '-nostdlib',
        '-nostdinc',
        '-include eggs/weston/weston-config.h',
    )
    # ymake 的 shared 分支直接用 ld，-shared 必须自己给
    add_ldflags('-shared')
    add_includedirs(
        '../../eggs/weston',
        '../../eggs/weston/frontend',
        '../../eggs/weston/shared',
        '../../eggs/weston/include',
        '../../eggs/weston/libweston',
        '../../eggs/weston/compat',
        '../../eggs/wayland-protocols/gen',
    )
    # 某些 shell 的私有头就在自己目录里（如 ivi-shell/ivi-layout-private.h），
    # 只能按模块补
    if includedirs:
        add_includedirs(*includedirs)
    add_files(*files)

weston_module(
    "kiosk-shell",
    [
        '../../eggs/weston/kiosk-shell/kiosk-shell.c',
        '../../eggs/weston/kiosk-shell/kiosk-shell-grab.c',
    ],
)

# YiYiYa 补丁：fbdev 后端。上游只有 drm/headless/x11/wayland/... 没有 fbdev，
# 我们的平台只有内核 /dev/fb0（无 DRM/KMS），所以补一个最小后端：
# pixman 直接把画面渲染进 /dev/fb0 的 mmap 内存（零拷贝）。
# 用法：weston --backend=fbdev-backend.so --shell=kiosk-shell.so ...
weston_module(
    "fbdev-backend",
    [
        '../../eggs/weston/compat/backend-fbdev.c',
    ],
)

# ---- 图形套（--wayland-desktop）：desktop-shell 模块 + 桌面客户端 ----
# 真桌面 shell（多窗口 + 背景/面板）：上游 desktop-shell/shell.c。
# 模块自身【不需要 cairo】——weston 16 的 shell.c 里 cairo 出现 0 次，窗口边框
# 由 libweston 的 shell-utils + renderer-borders 处理（pixman 直接画）。它归到
# 图形套，是因为配套的桌面客户端 weston-desktop-shell 要 cairo —— 没客户端时
# 这个 shell 只能管窗口、没有背景/面板，单独存在意义不大。
# 注意：cairo 库本身在 app 构建里是常备的（eggs/ya.py），这个选项只决定要不要
# 产出图形套这两件产物，不影响任何库能不能编、也不影响别的 app。
# 背景与面板由它 fork 出来的客户端提供（WESTON_SHELL_CLIENT=weston-desktop-shell，
# 见 weston-config.h 的 LIBEXECDIR），没有客户端时窗口管理照样工作。
if has_config("wayland-desktop"):
    weston_module(
        "desktop-shell",
        [
            '../../eggs/weston/desktop-shell/shell.c',
            '../../eggs/weston/desktop-shell/input-panel.c',
            '../../eggs/wayland-protocols/gen/weston-desktop-shell-protocol.c',
            '../../eggs/wayland-protocols/gen/input-method-unstable-v1-protocol.c',
        ],
    )
    weston_exe("weston-desktop", "weston-desktop", './weston_variants/desktop.c')

    # ivi-shell：车机式（In-Vehicle Infotainment）布局，图层由 hmi-controller
    # 按 id 摆放，客户端用 ivi_application 申请 surface id。模块自身无 cairo，
    # 私有头（ivi-layout-private.h / ivi-layout-shell.h）在 ivi-shell/ 目录里。
    weston_module(
        "ivi-shell",
        [
            '../../eggs/weston/ivi-shell/ivi-shell.c',
            '../../eggs/weston/ivi-shell/ivi-layout.c',
            '../../eggs/weston/ivi-shell/ivi-layout-transition.c',
            '../../eggs/weston/ivi-shell/hmi-controller.c',
            '../../eggs/wayland-protocols/gen/ivi-application-protocol.c',
            '../../eggs/wayland-protocols/gen/ivi-hmi-controller-protocol.c',
        ],
        ['../../eggs/weston/ivi-shell'],
    )
    weston_exe("weston-ivi", "weston-ivi", './weston_variants/ivi.c')

weston_module(
    "headless-backend",
    [
        '../../eggs/weston/libweston/backend-headless/headless.c',
        # decorated 模式下 headless 后端会引用 shared/frame.c 的窗口装饰函数，
        # 那套实现依赖 cairo。本系统 decorate=false，这些函数不会被调用，
        # 用同签名的空实现顶上（编进模块自身，免得可执行文件里没人引用而被
        # 链接器丢弃）。
        '../../eggs/weston/compat/frame-stubs.c',
    ],
)

# ---- 基于 toytoolkit 的桌面客户端（真正把 cairo 拖进构建的就是它们）----
# 它们都由对应的 shell 模块 fork 出来（wet_client_start，路径取
# WESTON_SHELL_CLIENT），所以必须放进镜像 bin 目录，并用 weston.ini 或
# WESTON_MODULE_MAP 指过去（LIBEXECDIR 默认 /usr/libexec）。
# 只依赖 cairo，不含 pango（cairo-util.c 里 pango 分支靠 HAVE_PANGO 关掉，
# 文字走 cairo toy API）。
def toytoolkit_client(name, main_src, extra_files=(), install_type="cli"):
    target(name)
    # install_type="libexec"：装到镜像 /usr/libexec（shell 按 LIBEXECDIR 绝对
    # 路径 fork 的辅助客户端），而不是 /bin —— 见 xenv/env.py 的 set_type()。
    set_type(install_type)
    # pixman：cairo 静态链已经带进来了，这里显式列出来只为拿到它的
    # include 路径（desktop-shell.c 的诊断探针直接调 pixman）。
    add_deps("wayland", "cairo", "pixman", "evdev", "xkbcommon")
    # 【必须 -fPIC】这里编的 shared/config-parser.c / file-util.c /
    # process-util.c / matrix.c 与 libweston.so 共用同一份 .o（见 wl-simple-shm
    # 处的说明）。不加 -fPIC 时这些 .o 会被本目标覆盖成非 PIC 版，下一次
    # libweston.so 链接就报 relocation R_AARCH64_ADR_PREL_PG_HI21 ... 需要
    # recompile with -fPIC（raspi5 首次全量构建就是这样炸的）。
    add_cflags('-fPIC', '-include eggs/weston/weston-config.h')
    # 注意【不要】加 '../../eggs/weston/compat'：那目录里有个 cairo.h 垫片
    # （给 renderer-borders 用的空声明），会把真的 <cairo.h> 顶掉，症状是
    # cairo_matrix_t / CAIRO_OPERATOR_OVER 全都"未声明"。evdev 依赖则提供
    # <linux/input.h>。
    add_includedirs(
        '../../eggs/weston',
        '../../eggs/weston/clients',
        '../../eggs/weston/shared',
        '../../eggs/weston/include',
        '../../eggs/wayland-protocols/gen',
    )
    add_files(
        main_src,
        *extra_files,
        # toytoolkit：这些客户端都是拿它搭的（window/widget/cairo 绘制）
        '../../eggs/weston/clients/window.c',
        '../../eggs/weston/shared/image-loader.c',
        '../../eggs/weston/shared/cairo-util.c',
        '../../eggs/weston/shared/file-util.c',
        '../../eggs/weston/shared/process-util.c',
        # weston_config_* 的实现：客户端不需要 libweston.so，直接静态编进来
        '../../eggs/weston/shared/config-parser.c',
        # 窗口装饰的 cairo 真实现（headless 后端用的是 compat/frame-stubs.c）
        '../../eggs/weston/shared/frame.c',
        '../../eggs/weston/shared/matrix.c',
        # 注：不要加 shared/os-compatibility.c —— libwayland.a 里已经有一份，
        # 两份同时链会 os_create_anonymous_file 重复定义。
        '../../eggs/wayland-protocols/gen/weston-desktop-shell-protocol.c',
        # toytoolkit(window.c) 与 libweston 头里引用的客户端协议对象
        '../../eggs/wayland-protocols/gen/xdg-shell-protocol.c',
        '../../eggs/wayland-protocols/gen/color-management-v1-protocol.c',
        '../../eggs/wayland-protocols/gen/viewporter-protocol.c',
        '../../eggs/wayland-protocols/gen/single-pixel-buffer-v1-protocol.c',
        '../../eggs/wayland-protocols/gen/presentation-time-protocol.c',
        '../../eggs/wayland-protocols/gen/fractional-scale-v1-protocol.c',
        '../../eggs/wayland-protocols/gen/pointer-constraints-unstable-v1-protocol.c',
        '../../eggs/wayland-protocols/gen/relative-pointer-unstable-v1-protocol.c',
        '../../eggs/wayland-protocols/gen/xdg-output-unstable-v1-protocol.c',
        '../../eggs/wayland-protocols/gen/linux-dmabuf-unstable-v1-protocol.c',
        '../../eggs/wayland-protocols/gen/text-input-unstable-v1-protocol.c',
        '../../eggs/wayland-protocols/gen/input-method-unstable-v1-protocol.c',
        '../../eggs/wayland-protocols/gen/tablet-unstable-v2-protocol.c',
        '../../eggs/wayland-protocols/gen/text-cursor-position-protocol.c',
    )

if has_config("wayland-desktop"):
    # 下面三个都是 shell 拉起的辅助客户端，必须装到镜像 /usr/libexec
    # （frontend 按 LIBEXECDIR 的绝对路径 fork 它们），所以 install_type
    # 用 "libexec" —— 不再往 /bin 里放一份。

    # 官方桌面客户端（desktop-shell 的视觉部分：背景、面板、时钟、启动器）
    toytoolkit_client(
        "weston-desktop-shell",
        '../../eggs/weston/clients/desktop-shell.c',
        install_type="libexec",
    )

    # 输入法面板：frontend 初始化 input-method 时会按 LIBEXECDIR 把它拉起来
    # （见 main.c 的 launch_input_method / wet_client_start），缺了它只会反复打
    # "Couldn't launch client '/usr/libexec/weston-keyboard'"，不影响桌面本体。
    toytoolkit_client(
        "weston-keyboard",
        '../../eggs/weston/clients/keyboard.c',
        install_type="libexec",
    )

    # ivi-shell 的配套 UI 客户端（背景 + 面板 + 应用启动器），
    # 额外带 ivi 的两个客户端协议
    toytoolkit_client(
        "ivi-shell-user-interface",
        '../../eggs/weston/clients/ivi-shell-user-interface.c',
        [
            '../../eggs/wayland-protocols/gen/ivi-application-protocol.c',
            '../../eggs/wayland-protocols/gen/ivi-hmi-controller-protocol.c',
        ],
        install_type="libexec",
    )

# ---- 运行时布局：把共享库/模块摆到镜像里 weston 期望的路径 ----
#   /lib/libweston.so                       （可执行文件与模块的 NEEDED）
#   /usr/lib/libweston-16/headless-backend.so   （LIBWESTON_MODULEDIR）
#   /usr/lib/weston/kiosk-shell.so              （MODULEDIR）
import os as _os
import shutil as _shutil

_bdir = _os.path.join('build', str(get_plat()), str(get_arch()),
                      str(get_config('mode')))
_deploy = [
    ('libweston.so', 'app/resource/lib/libweston.so'),
    ('libkiosk-shell.so', 'app/resource/usr/lib/weston/kiosk-shell.so'),
    ('libheadless-backend.so',
     'app/resource/usr/lib/libweston-16/headless-backend.so'),
    ('libfbdev-backend.so',
     'app/resource/usr/lib/libweston-16/fbdev-backend.so'),
]
if has_config("wayland-desktop"):
    # 图形套的模块与客户端：不开 --wayland-desktop 时连编译都没有，别去找文件
    # 辅助客户端不在这里：它们用 set_type("libexec") 自己装到 usr/libexec
    _deploy += [
        ('libdesktop-shell.so', 'app/resource/usr/lib/weston/desktop-shell.so'),
        ('libivi-shell.so', 'app/resource/usr/lib/weston/ivi-shell.so'),
    ]
# ---- weston 运行期数据文件（DATADIR=/usr/share）----
# shared/file-util.c 的 file_name_with_datadir() 拼的是
# "<DATADIR>/weston/<名>"，即镜像里的 /usr/share/weston/。这些 png 不是
# 编译产物，但运行时找不到就会出问题：
#   terminal.png      panel 没有 [launcher] 配置时的默认启动器图标 —— 缺了
#                     load_icon_or_fallback() 会画一个"方框叉"，看着像渲染坏了
#   pattern.png       background-color=0 时的背景图案
#   sign_*.png        客户端窗口装饰按钮（toytoolkit frame_draw 用）
#   wayland.png       窗口/任务栏图标
#   icon_ivi_*.png    ivi-shell-user-interface 的启动器图标
# 命中时走上面那条 file_name_with_datadir 路径，所以目录固定是 weston/。
_weston_data_src = _os.path.join('eggs', 'weston', 'data')
_weston_data_dst = _os.path.join('app', 'resource', 'usr', 'share', 'weston')

def _deploy_weston_data():
    """把 eggs/weston/data 下的 png 同步到镜像的 /usr/share/weston。

    只拷 .png（COPYING/meson.build/svg 运行时用不到）。幂等：源比目标新才拷。
    """
    if not _os.path.isdir(_weston_data_src):
        return
    for _name in _os.listdir(_weston_data_src):
        if not _name.endswith('.png'):
            continue
        _src = _os.path.join(_weston_data_src, _name)
        _dst = _os.path.join(_weston_data_dst, _name)
        if (not _os.path.exists(_dst) or
                _os.path.getmtime(_src) > _os.path.getmtime(_dst)):
            _os.makedirs(_weston_data_dst, exist_ok=True)
            _shutil.copyfile(_src, _dst)

def _deploy_runtime_layout(target=None):
    """把 _deploy 表里的产物同步到 app/resource（镜像里的运行时路径）。

    幂等：源比目标新才拷。既在配置阶段调一次（上次构建留下的产物立刻落地），
    也挂到 after_build（本轮刚编出来的当轮就落地，不用再跑一次构建）。
    """
    for _src_name, _dst in _deploy:
        _src = _os.path.join(_bdir, _src_name)
        if not _os.path.exists(_src):
            continue
        _os.makedirs(_os.path.dirname(_dst), exist_ok=True)
        if (not _os.path.exists(_dst) or
                _os.path.getmtime(_src) > _os.path.getmtime(_dst)):
            _shutil.copyfile(_src, _dst)
    _deploy_weston_data()

_deploy_runtime_layout()
after_build(_deploy_runtime_layout)
