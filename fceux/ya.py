# coding:utf-8
# *******************************************************************
# * fceux NES 模拟器，使用上游 SDL 驱动（SDL2）
# *
# * 源码来自上游 fceux 2.2.3，自带 Makefile/SConstruct/SConscript
# * 都是给 PC/dingux 用的，这里按 ya 的方式重新组织一遍。
# ********************************************************************

target("fceux")
set_type("cli")
add_deps("cxx","cxxabi","sdl2","zlib","lua",get_config('default_libc'))

add_files(
    # ---- 核心 core（对应上游 Makefile.pc 的 CORE_OBJS）----
    # lua-engine.cpp 走 -D_S9XLUA_H 打开，Lua 本体用 eggs/liblua（5.4），
    # 不编译上游自带的 src/lua（5.1 源码）；5.1/5.4 的 API 差异在
    # lua-engine.cpp 顶部的兼容层里补齐。
    'src/asm.cpp',
    'src/cart.cpp',
    'src/cheat.cpp',
    'src/conddebug.cpp',
    'src/config.cpp',
    'src/debug.cpp',
    'src/drawing.cpp',
    'src/emufile.cpp',
    'src/fceu.cpp',
    'src/fds.cpp',
    'src/file.cpp',
    'src/filter.cpp',
    'src/ines.cpp',
    'src/input.cpp',
    'src/lua-engine.cpp',
    'src/movie.cpp',
    'src/netplay.cpp',
    'src/nsf.cpp',
    'src/oldmovie.cpp',
    'src/palette.cpp',
    'src/ppu.cpp',
    'src/sound.cpp',
    'src/state.cpp',
    'src/unif.cpp',
    'src/video.cpp',
    'src/vsuni.cpp',
    'src/wave.cpp',
    'src/x6502.cpp',

    # ---- mapper / 手柄 / 工具 ----
    'src/boards/*.cpp',
    'src/boards/*.c',
    'src/input/*.cpp',
    'src/utils/*.cpp',
    'src/utils/*.c',      # ConvertUTF.c（unzip/ioapi 需要 zlib）

    # ---- 驱动公共部分 ----
    'src/drivers/common/*.cpp',
    'src/drivers/common/*.c',   # nes_ntsc.c

    # ---- SDL 驱动（上游 SConscript 的 source_list）----
    # 不纳入：
    #   drivers/sdl/gui.cpp        依赖 GTK
    #   drivers/sdl/sdl-opengl.cpp 依赖 OpenGL
    #   drivers/sdl/sdl-netplay.cpp 依赖 SDL_net
    # 它们在上游也是按 GTK/GL 条件加入的，相关调用点均有 #ifdef 保护。
    'src/drivers/sdl/config.cpp',
    'src/drivers/sdl/input.cpp',
    'src/drivers/sdl/sdl.cpp',        # main()
    'src/drivers/sdl/sdl-joystick.cpp',
    'src/drivers/sdl/sdl-sound.cpp',
    'src/drivers/sdl/sdl-throttle.cpp',
    'src/drivers/sdl/sdl-video.cpp',
    'src/drivers/sdl/unix-netplay.cpp',
)

add_includedirs(
    'src',
    '.',
    # set_type("cli") 会给编译加 -nostdinc（xenv/env.py），GCC 内置目录被禁，
    # libcxx 的 <cstddef> 用 #include_next <stddef.h> 取编译器头时就只能在
    # -I 序列里找（报：致命错误：stddef.h：No such file or directory）。
    # 这两处都备有 stddef.h，显式补进序列（重复 -I 无害）。
    '../../eggs/libcxx/include',
    '../../eggs/libmusl/include',
)

# 上游 Makefile.pc 的 CFLAGS 定义，C/C++ 两边都要
fceux_defines = (
    ' -DLSB_FIRST'          # ARM 小端
    ' -DPSS_STYLE=1'
    ' -DHAVE_ASPRINTF'      # musl 提供 asprintf
    ' -DFRAMESKIP'
    ' -D_GNU_SOURCE=1'
    ' -D_REENTRANT'
    # 打开 Lua 支持（上游 LUA=1 时才定义的宏）。fceu.cpp/input.cpp/
    # state.cpp 等处的 fceulua.h 引用全在 #ifdef _S9XLUA_H 内。
    ' -D_S9XLUA_H'
    # fceux 借 libstdc++ 的 _GLIBCXX_USE_WCHAR_T 探测「工具链有没有
    # wchar_t」，上游 Linux 构建恒为真、走宽字符分支。libc++ 不定义这个
    # libstdc++ 专属宏，types.h:144 于是会 `#define wstring string`：
    #   1) 该宏泄漏进 <locale>，把 `const wstring* __weeks()` 改写成
    #      `const string*`，签名与主模板不符，GCC 报「模板标识符
    #      __weeks<> 不匹配任何模板声明 / 需要 2 个 template<>」；
    #   2) 含 types.h 的 TU 与 xstring.cpp 对 mbstowcs 返回类型理解不一
    #      致（返回类型不参与 mangling，能链接上却按 string 解 wstring，
    #      运行期内存破坏）；
    #   3) drivers/sdl/input.cpp:599 的 `L""` 实参编不过。
    # 本工具链 wchar_t 可用，按上游 Linux 的方式定义，5 处判断统一走宽分支。
    ' -D_GLIBCXX_USE_WCHAR_T'
)

# YiYiYa: 原先没有任何 -O（gcc 默认 -O0），模拟核心 + 8bit→32bit 缩放拖到
# 40ms/帧（约 21fps）。这里显式开 -O2；fceux 是老代码（有类型双关），
# 同时关掉严格别名以防 -O2 下出问题。
add_cflags(fceux_defines + ' -O2 -fno-strict-aliasing -g -ffreestanding -fpic')

# 合并成单个字符串传：add_cxxflags 分多个参数时后段会被静默丢掉，
# 而 -D_LIBCPP_BUILDING_LIBRARY 一丢，libcxx 的 <__availability> 里
# `#if defined(_LIBCPP_BUILDING_LIBRARY) || ... !__has_feature(...)` 就
# 求值到了 __has_feature（clang 专用），GCC 报
# "missing binary operator before token ("，随后的
# "vendor availability markup" #error 也跟着触发。
add_cxxflags(
    fceux_defines
    # 2.2.3 是老代码（src 下有 34 处 register），停在 c++11
    # -O2/-fno-strict-aliasing：见上面对 add_cflags 的说明
    + ' -std=c++11 -O2 -fno-strict-aliasing -g -fpermissive'
    + ' -Wno-write-strings -Wno-sign-compare'
    + ' -ffreestanding -fpic'
    + ' -fno-use-cxa-atexit -fno-threadsafe-statics -D_LIBCPP_HAS_MUSL_LIBC -D_LIBCPP_HAS_NO_LIBRARY_ALIGNED_ALLOCATION -D_LIBCPP_BUILDING_LIBRARY -D_POSIX_C_SOURCE -D_GNU_SOURCE -D_LIBCPP_HAS_NO_MONOTONIC_CLOCK -D_LIBCPP_HAS_NO_THREADS -D_LIBCXXABI_HAS_NO_THREADS '
)
