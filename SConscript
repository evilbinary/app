# coding:utf-8
# *******************************************************************
# * Copyright 2021-2080 evilbinary
# * 作者: evilbinary on 01/01/20
# * 邮箱: rootdebug@163.com
# ********************************************************************
import os
import platform
import copy

Import('env')

plt = platform.system()

def check_exit(apps):
    new_list = copy.deepcopy(apps)
    for app in new_list:
        if os.path.exists(app) == False:
            print('ignore app '+app)
            apps.remove(app)


returns=[]

build_app=[]

if env.get('APP') and len(env.get('APP'))>0 :
    build_app = env.get('APP')
    all = SConscript(dirs=build_app, exports='env')
    resource_file=  Glob('resource/*')

    app_cpp_file=[
        'test/test-cpp',
        'gmenu2x/gmenu',
        'monogui/monogui',
        'xtrack/xtrack',
        # 'monogui/demo/asc12.bin',
        # 'monogui/demo/gb12song',
        # 'monogui/demo/gb16song',
        # 'monogui/demo/KeyMap.txt',
        # 'monogui/demo/lx.db',
        # 'monogui/demo/lx.idx',
        # 'monogui/demo/ex.db',
        # 'monogui/demo/ex.idx',
        # 'monogui/demo/py.db',
        # 'monogui/demo/py.idx',
    ]
    apps_file = [
        'hello/hello',
        'gui/gui',
        'microui/microui',
        'etk/etk',
        'test/test',
        'test/test-file',
        'test/test-mem',
        'test/test-uncompress',
        'test/test-string',
        'test/test-stdlib',
        'test/test-stdio',
        'test/test-fork',
        'test/test-free',
        'test/test-sound',
        'test/test-sys',
        'rust/test/test-rs',
        'test/test-thread',
        'test/libtest-so.so',
        'test/test-call-so',
        'test/test-dl',



        'cmd/ls',
        'cmd/echo',
        'cmd/cat',
        'cmd/hexdump',
        'cmd/touch',
        'cmd/date',
        'cmd/kill',
        'cmd/shell',

        'lvgl/lvgl',
        'track/track',
        'launcher/launcher',
        'infones/infones',
        'sdl2/sdl2',
        'sdl2/player',
        'sdl2/showimage',
        'sdl2/showfont',
        'mgba/mgba',
        'mgba/miniunz',
        'gnuboy/gnuboy',

        'lua/lua',
        'lua/luat',
        'lua/hello.lua',
        'scheme/scheme',
        'scheme/petite.boot',
        'scheme/scheme.boot',

        'quickjs/qjs',
        'quickjs/qjsc',
        # 'quickjs/repl.js',
        # 'quickjs/qjscalc.js',
        # # 'quickjs/tests/test_builtin.js',
        # 'quickjs/examples/hello.js',
        'watch/watch',

       
        # 'sdlmine/sdlmine',
        'sdl/testwm',
        'sdl/testoverlay2',
        'sdl/testbitmap',
        'sdl/testoverlay',

        'unitest/pngtest',
        '../foot/service/servicea',
        '../foot/service/serviceb',
        '../foot/service/systemd',
        '../foot/service/vfsd',
        '../foot/driver/tinyusb/tinyusb',
  
    ]
    if env.get('CPP_APP') and len(env.get('CPP_APP'))>0:
        apps_file+=app_cpp_file

    if env.get('DEFAULT_LIBC') == 'libmusl':
        SConscript(dirs=['toybox'], exports='env')
        # SConscript(dirs=['meui'], exports='env')

        apps_file += [
            'test/test-musl',
            'toybox/toybox',
            # 'meui/packages/examples/dist/index.js',
            # 'meui/meui'
        ]

    if plt == 'Darwin':
        env.Command('copyapp',
                    apps_file,
                    [
                    'cp ${SOURCES} app/resource/bin/ ' \
                    '&& hdid  image/disk.img '\
                    '&& cp -r app/resource/ /Volumes/NO\ NAME/ ' \
                    '&& hdiutil eject /Volumes/NO\ NAME/'
                     ])
        pass
    elif plt == 'Linux':
        env.Command('copyapp',
                    apps_file,
                    ['cp ${SOURCES} app/resource/bin/ '\
                     '&& sudo losetup /dev/loop10 image/disk.img ' \
                     '&& sudo mount /dev/loop10 /mnt '\
                     '&& sudo cp -r app/resource/* /mnt '\
                     '&& sudo umount /mnt ' \
                     '&& sudo losetup -d /dev/loop10'
                     ])
    elif plt == 'Windows':
        try:
            ret = env.Command('copyapp',
                              apps_file,
                              [
                                  'cp ${SOURCES} app/resource/bin/ & mcopy.exe -nmov  -i image/disk.img app/resource/* ::'
                              ])
        except:
            print('please manual copy %s files to image/disk.img' % (apps_file))
        pass
else:
    pass

Return('returns')
