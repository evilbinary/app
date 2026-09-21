# yui 资源同步：构建成功后，把 app/yui/ 下除 .c/.md/.py 之外的文件
# 按原目录结构拷贝到 app/resource/app/（运行时按 app/watch-os/app.json 这类路径加载）
def copy_yui_res(target):
    import os as posix_os
    import shutil as shutil_mod

    src_dir = os.projectdir() + "/" + (target.get('file-path') if target.get('file-path') else 'app/yui')
    dst_dir = os.projectdir() + "/app/resource/app"

    if not posix_os.path.isdir(src_dir):
        print('yui: resource dir not found ' + src_dir)
        return

    # .pyc/__pycache__ 是构建/运行产生的中间文件，一并排除
    exclude_exts = ('.c', '.md', '.py', '.pyc')
    exclude_dirs = ('__pycache__', '.git')

    count = 0
    updated = 0
    for dirpath, dirnames, filenames in posix_os.walk(src_dir):
        dirnames[:] = [d for d in dirnames if d not in exclude_dirs]
        rel = posix_os.path.relpath(dirpath, src_dir)
        dst_path = dst_dir if rel == '.' else posix_os.path.join(dst_dir, rel)
        if not posix_os.path.isdir(dst_path):
            posix_os.makedirs(dst_path)

        for name in filenames:
            if name.endswith(exclude_exts):
                continue
            count = count + 1
            src_file = posix_os.path.join(dirpath, name)
            dst_file = posix_os.path.join(dst_path, name)
            # 没变化就跳过写盘（仍计入同步总数），避免每次构建都重复拷贝
            if posix_os.path.exists(dst_file):
                try:
                    if (posix_os.path.getsize(src_file) == posix_os.path.getsize(dst_file)
                            and posix_os.path.getmtime(src_file) <= posix_os.path.getmtime(dst_file)):
                        continue
                except OSError:
                    pass
            shutil_mod.copy2(src_file, dst_file)
            updated = updated + 1

    print('yui: sync {} resource file(s) -> {} (updated {})'.format(count, dst_dir, updated))


target("ymain")
set_type("cli")

add_deps("socket","yui","quickjs","jsmodule-quickjs","yaml2json"),

add_files(
    'main.c',
) 
add_cflags(' -DSDL_DISABLE_ARM_NEON_H ')

# add_cflags('-UCONFIG_PRINTF_RNDN -D_GNU_SOURCE -DUSE_FILE32API  -Ieggs/libquickjs -DCONFIG_BIGNUM')

add_includedirs(
    './include',
    '.',
    'libyui/src'
)

after_build(copy_yui_res)

target("ymario") 
(
    add_deps( "jsmodule-mario","yui", "mario",),
    set_type("cli"),
    add_files("main.c"),
    after_build(copy_yui_res)
)


target("yqjs") 
(
    add_deps( "jsmodule-quickjs","yui", "quickjs",),
    set_type("cli"),
    add_files("main.c"),
    after_build(copy_yui_res)
)
