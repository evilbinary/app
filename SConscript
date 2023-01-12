# coding:utf-8
# *******************************************************************
# * Copyright 2021-2080 evilbinary
# * 作者: evilbinary on 01/01/20
# * 邮箱: rootdebug@163.com
# ********************************************************************
import os
import platform
import copy
from xenv.env import add_libc

plt = platform.system()

Import('appEnv')
cliEnv = appEnv.Clone()
Export('cliEnv')

env = appEnv


env['CPPPATH'] += [
    '../../eggs/',
    '.',
    '../libs/include/',
    '../include/',
    '../../duck/libs/include'
]

env['LIBPATH'] += [
    '../../eggs/',
]

add_libc(env)
add_libc(cliEnv)

cliEnv['LIBS'] += cliEnv['LIBC']
cliEnv['CFLAGS'] += cliEnv['LIBCFLAGS']
cliEnv['LINKFLAGS'] += cliEnv['USER']
if cliEnv.get('MYLIB'):
    cliEnv['LIBS'].append(cliEnv.get('MYLIB'))

env['CPPPCOMMON'] = [
    '../../eggs/libgui',
    '../../eggs/libjpeg',
    '../../eggs/libzlib',
    '../../eggs/libpng',
    '../../eggs/libetk',
    '../../eggs/libcmocka/include',
    '../../eggs/liblvgl',
    '../../eggs/libsdl2/include',
]

env['LIBCOMMON'] = [
    '../../eggs/libgui',
    '../../eggs/libimage',
    '../../eggs/libjpeg',
    '../../eggs/libpng',
    '../../eggs/libzlib',
    '../../eggs/libetk',
    '../../eggs/libcmocka',
    '../../eggs/liblz4',
    '../../eggs/libuuid',
    '../../eggs/liblvgl',
    '../../eggs/liblvqrcode',
]

env['LIBPATH'] += env['LIBCOMMON']
env['CPPPATH'] += env['CPPPCOMMON']
env['LIBS'] += env['LIBC']
env['CFLAGS'] += env['LIBCFLAGS']
env['LINKFLAGS'] += env['USER']

if env.get('MYLIB'):
    env['LIBS'].append(env.get('MYLIB'))


def check_exit(apps):
    new_list = copy.deepcopy(apps)
    for app in new_list:
        if os.path.exists(app) == False:
            print('ignore app '+app)
            apps.remove(app)


returns=[]


if env.get('APP'):
    build_app = ['hello', 'gui', 'microui', 'test', 'etk', 'cmd', 'lvgl', 'track',
                 'sdl2', 'infones', 'launcher', 'mgba', 'lua', 'scheme', 'quickjs', 'gnuboy']

    all = SConscript(dirs=build_app, exports='env')
    
    for i in filter(None, all):
        for j in i:
            returns+=j

    apps_file = [
        # 'hello/hello',
        # 'gui/gui',
        # 'microui/microui',
        # 'etk/etk',
        # 'test/test',
        # 'test/test-file',
        # 'test/test-mem',
        # 'test/test-uncompress',
        # 'test/test-string',
        # 'test/test-stdlib',
        # 'test/test-stdio',
        # 'test/test-fork',
        # 'test/test-free',

        # 'rust/test/test-rs',
        'cmd/ls',
        'cmd/echo',
        'cmd/cat',
        'cmd/hexdump',
        'cmd/touch',
        'cmd/date',

        # 'lvgl/lvgl',
        # 'track/track',
        # 'launcher/launcher',
        # 'infones/infones',
        # 'sdl2/sdl2',
        # 'sdl2/player',
        # 'sdl2/showimage',
        # 'sdl2/showfont',
        # 'mgba/mgba',
        # 'mgba/miniunz',
        # 'gnuboy/gnuboy',

        # 'lua/lua',
        # 'lua/luat',
        # 'lua/hello.lua',
        # 'scheme/scheme',
        # 'scheme/petite.boot',
        # 'scheme/scheme.boot',

        # 'quickjs/qjs',
        # 'quickjs/qjsc',
        # 'quickjs/repl.js',
        # 'quickjs/qjscalc.js',
        # # 'quickjs/tests/test_builtin.js',
        # 'quickjs/examples/hello.js',

    ]
    # apps += Glob('resource/*')
    if env.get('DEFAULT_LIBC') == 'libmusl':
        SConscript(dirs=['toybox'], exports='env')
        SConscript(dirs=['meui'], exports='env')

        apps_file += [
            # 'test/test-musl',
            'toybox/toybox',
            'meui/packages/examples/dist/index.js',
            'meui/meui'
        ]

    if plt == 'Darwin':
        env.Command('copyapp',
                    apps_file,
                    ['hdid  image/disk.img &&  cp ${SOURCES} /Volumes/NO\ NAME/ && hdiutil eject /Volumes/NO\ NAME/'
                     ])
        pass
    elif plt == 'Linux':
        env.Command('copyapp',
                    apps_file,
                    ['sudo losetup /dev/loop10 image/disk.img && sudo mount /dev/loop10 /mnt && sudo cp  ${SOURCES} /mnt && sudo umount /mnt && sudo losetup -d /dev/loop10'
                     ])
    elif plt == 'Windows':
        try:
            ret = env.Command('copyapp',
                              apps_file,
                              [
                                  'cp ${SOURCES} app/bin/ & mcopy.exe -nmov  -i image/disk.img app/bin/* ::'
                              ])
        except:
            print('please manual copy %s files to image/disk.img' % (apps_file))
        pass
else:
    pass

Return('returns')
