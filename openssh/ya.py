# coding:utf-8
# *******************************************************************
# Copyright 2021-present evilbinary
# 作者: evilbinary on 01/01/20
# 邮箱: rootdebug@163.com
# ********************************************************************
# OpenSSH 10.0p1 的 sshd（裸机 aarch64 + musl + 我们自己的 libcrypto）。
#
# 和 libopenssl 同一套 automake 模式：
#   set_sourcedir(<out-of-tree>) + automake('sshd') + configure(...)
# 产物：<build>/<plat>/<arch>/<mode>/openssh/lib/sbin/sshd
#   —— 用 filename='sbin/sshd' 对齐 OpenSSH 的 install 路径（prefix/sbin/sshd），
#      否则 ymake 会认为「没构建过」而每次重跑 configure。
import os as _pyos

target("openssh")
# OpenSSH 产出的是程序（sshd/ssh/scp/sftp…），不是 .a。
# set_type("cli") 使 kind 为空 → get_build_target 拼产物时不加 lib 前缀/.a 后缀，
# 产物名完全由下面的 automake(name, filename=...) 决定。
set_type("cli")

add_packages(get_config('default_libc'))
add_deps("crypto")          # libcrypto.a（eggs/libopenssl）

_ssh_src = _pyos.path.abspath(os.scriptdir())
_ssh_oot = _pyos.path.abspath(_pyos.path.join(
    'build', str(get_plat()), str(get_arch()), str(get_config('mode')),
    'openssh-oot'))
_pyos.makedirs(_ssh_oot, exist_ok=True)

# libcrypto 的安装前缀（--with-ssl-dir 指向它：include/openssl + lib/libcrypto.a）
_crypto_prefix = _pyos.path.abspath(_pyos.path.join(
    'build', str(get_plat()), str(get_arch()), str(get_config('mode')),
    'crypto', 'lib'))

# configure 包装脚本：补上裸机裁剪项。
#   --without-privsep   我们没有 chroot(2)，privsep 的监控进程用不了
#   --without-pam       没有 PAM，走 /etc/passwd（后续再补 session_check）
#   --without-zlib      先不要压缩，少一个库
#   --without-libedit   sftp 交互编辑用不到
#   --without-selinux / audit  Linux 专有
#   --disable-utmp/wtmp/btmp/lastlog 没有这些记账文件
_cfg_args = (
    '--host=%s '
    '--with-ssl-dir="%s" '
    # 【全放 bin】OpenSSH 默认把 sshd 装到 sbin（FHS 的「系统守护进程」约定），
    # 但我们是单用户系统、PATH 就几个目录，没必要再分一级。
    # 单引号是关键：让 configure 保存成字面量 ${prefix}/bin，
    # 用双引号会被 shell 当场展开成 /bin（写到宿主机上去）。
    "--sbindir='${prefix}/bin' "
    "--libexecdir='${prefix}/bin' "
    '--without-pam --without-privsep --without-zlib --without-libedit '
    '--without-selinux --without-audit --without-kerberos5 '
    '--disable-strip --disable-etc-default-login '
    '--disable-utmp --disable-wtmp --disable-btmp --disable-lastlog'
    % (get_toolchain(), _crypto_prefix))

_configure_wrapper = (
    '#!/bin/sh\n'
    '# 【已配置就跳过】ymake 只用产物 sbin/sshd 判断「构建过没有」：只要\n'
    '# make 还没产出 sshd（哪怕 configure 早就成功了），每次 ya 都会再调一次\n'
    '# 本脚本。这里用 config.status/Makefile 挡住，避免反复 configure。\n'
    '# 裁剪项变化时，ya.py 的指纹逻辑会删掉 config.status，自动重配。\n'
    'if [ -f config.status ] && [ -f Makefile ]; then\n'
    '    echo "openssh: already configured, skip"\n'
    '    exit 0\n'
    'fi\n'
    + (('"%s/configure" ' + _cfg_args + ' "$@" || exit $?\n') % _ssh_src)
    +
    # 【交叉编译补 config.h】autoconf 的 AC_CHECK_SIZEOF / AC_CHECK_TYPES
    # 都靠「编译再解析报错」或链接测试，交叉编译下拿不到值，结果
    # SIZEOF_SHORT_INT / HAVE_STRUCT_TIMESPEC / HAVE_INTMAX_T 全是 undef，
    # 于是 defines.h 和 bsd-misc.h 自己 typedef，和 musl 撞车：
    #   #error "16 bit int type not found."
    #   redefinition of 'struct timespec'
    #   conflicting types for 'intmax_t'
    # 这里直接 append 覆盖（config.h 里原本是注释掉的 #undef，不冲突）。
    # 注意：HAVE_U_INTXX_T / HAVE_U_INT64_T 不要加 —— musl 没有这些类型，
    # 得让 OpenSSH 自己 typedef。
    'cat >> config.h << \'OPENSSH_FOOT_EOF\'\n'
    '#define SIZEOF_SHORT_INT 2\n'
    '#define SIZEOF_INT 4\n'
    '#define SIZEOF_LONG_INT 8\n'
    '#define SIZEOF_LONG_LONG_INT 8\n'
    '#define SIZEOF_CHAR_P 8\n'
    '#define SIZEOF_DOUBLE 8\n'
    '#define SIZEOF_LONG_DOUBLE 16\n'
    '#define SIZEOF_TIME_T 8\n'
    '#define SIZEOF_OFF_T 8\n'
    '#define SIZEOF_PID_T 4\n'
    '#define HAVE_STRUCT_TIMESPEC 1\n'
    '#define HAVE_INTMAX_T 1\n'
    '#define HAVE_UINTMAX_T 1\n'
    '#define HAVE_INTXX_T 1\n'
    '#define HAVE_U_INT 1\n'
    '#define HAVE_U_CHAR 1\n'
    '#define HAVE_U_SHORT 1\n'
    '#define HAVE_U_LONG 1\n'
    '#define HAVE_LONG_LONG_INT 1\n'
    '#define HAVE_SIZE_T 1\n'
    '#define HAVE_SSIZE_T 1\n'
    '#define HAVE_MODE_T 1\n'
    '#define HAVE_OFF_T 1\n'
    '#define HAVE_PID_T 1\n'
    '#define HAVE_SA_FAMILY_T 1\n'
    '#define HAVE_IN_ADDR_T 1\n'
    '#define HAVE_BLKCNT_T 1\n'
    '#define HAVE_BLK_SIZE_T 1\n'
    '#define HAVE_SIG_ATOMIC_T 1\n'
    '#define HAVE_STRUCT_SOCKADDR_STORAGE 1\n'
    'OPENSSH_FOOT_EOF\n'
    'echo "openssh: patched config.h (sizeof/types)"\n'
    # install 依赖 host-key / check-config，这两个目标要【运行】交叉编译出来的
    # ssh-keygen（在 macOS 上必然 126），所以从 install 依赖里摘掉。
    'sed -i "" "s/^install: \\(.*\\) host-key check-config$/install: \\1/" Makefile\n'
    'exit 0\n')
_cfg_path = _pyos.path.join(_ssh_oot, 'configure')
try:
    _old = open(_cfg_path, 'r').read()
except Exception:
    _old = None
if _old != _configure_wrapper:
    _w = open(_cfg_path, 'w')
    _w.write(_configure_wrapper)
    _w.close()
    _pyos.chmod(_cfg_path, 0o755)

# 配置指纹（同 libopenssl：裁剪项/编译链接参数变了才强制重配）。
# 真正的比较放在 configure() 之前（那时 _cflags/_ldflags/_libs 都已算好）。
_stamp = _pyos.path.join(_ssh_oot, '.configure.fingerprint')
try:
    _old_stamp = open(_stamp, 'r').read()
except Exception:
    _old_stamp = None

set_sourcedir(_ssh_oot)

add_includedirs(
    _pyos.path.join(_crypto_prefix, 'include'),
    public=True)

add_deps("gcc")
_tool = get_toolchain()

_ssh_prefix = _pyos.path.abspath(_pyos.path.join(
    'build', str(get_plat()), str(get_arch()), str(get_config('mode')),
    'openssh'))

_SSHD_CONFIG = '''# YiYiYa 默认 sshd 配置（公钥登录；musl 没有 libcrypt，密码认证关掉）
Port 22
ListenAddress 0.0.0.0
HostKey /etc/ssh/ssh_host_ed25519_key
PermitRootLogin yes
PubkeyAuthentication yes
PasswordAuthentication no
PermitEmptyPasswords no
UsePAM no
X11Forwarding no
AllowTcpForwarding yes
Subsystem sftp internal-sftp
'''


def _deploy_sshd(_target):
    """把 sshd 装进镜像资源目录，并补 host key / 配置。

    镜像里的路径约定：app/resource/bin ⇒ 运行时的 /bin。
    host key 用【宿主机的】ssh-keygen 生成 —— 交叉编译出来的 ssh-keygen
    在 macOS 上跑不了（install 的 host-key 目标就是因此失败的）。"""
    import glob as _glob
    import shutil as _shutil
    import subprocess as _sp
    _res = _pyos.path.join(_pyos.path.abspath(os.projectdir()),
                           'app', 'resource')
    _src = _pyos.path.join(_ssh_prefix, 'bin', 'sshd')
    if not _pyos.path.exists(_src):
        return
    _bin = _pyos.path.join(_res, 'bin')
    _pyos.makedirs(_bin, exist_ok=True)
    _dst = _pyos.path.join(_bin, 'sshd')
    if not _pyos.path.exists(_dst) or \
            _pyos.path.getsize(_dst) != _pyos.path.getsize(_src):
        _shutil.copyfile(_src, _dst)
        _pyos.chmod(_dst, 0o755)
        print('openssh: deployed -> {}'.format(_dst))

    _ssh_etc = _pyos.path.join(_res, 'etc', 'ssh')
    _pyos.makedirs(_ssh_etc, exist_ok=True)
    _cfg = _pyos.path.join(_ssh_etc, 'sshd_config')
    if not _pyos.path.exists(_cfg):
        _f = open(_cfg, 'w')
        _f.write(_SSHD_CONFIG)
        _f.close()
        print('openssh: wrote {}'.format(_cfg))

    _key = _pyos.path.join(_ssh_etc, 'ssh_host_ed25519_key')
    if not _pyos.path.exists(_key):
        try:
            _sp.check_call(['ssh-keygen', '-t', 'ed25519', '-N', '',
                            '-C', 'root@yiyiya', '-f', _key])
            _pyos.chmod(_key, 0o600)
            print('openssh: generated host key')
        except Exception as _e:
            print('openssh: host key generation failed:', _e)

    # /etc/passwd：sshd 要用 getpwnam("root") 才能切用户/取 home。
    # 单用户系统，写一条就够（不需要 shadow —— 已关密码认证）。
    _pw = _pyos.path.join(_res, 'etc', 'passwd')
    if not _pyos.path.exists(_pw):
        _f = open(_pw, 'w')
        _f.write('root:x:0:0:root:/root:/bin/sh\n'
                 'sshd:x:22:22:sshd:/var/empty:/bin/false\n')
        _f.close()
        print('openssh: wrote /etc/passwd')

    # authorized_keys：从宿主机的 ~/.ssh/*.pub 收进来（公钥认证，不用密码）
    _ak_dir = _pyos.path.join(_res, 'root', '.ssh')
    _ak = _pyos.path.join(_ak_dir, 'authorized_keys')
    if not _pyos.path.exists(_ak):
        _pubs = sorted(_glob.glob(_pyos.path.expanduser('~/.ssh/*.pub')))
        if _pubs:
            _pyos.makedirs(_ak_dir, exist_ok=True)
            _out = open(_ak, 'w')
            for _p in _pubs:
                _out.write(open(_p, 'r').read().strip() + '\n')
            _out.close()
            _pyos.chmod(_ak, 0o600)
            print('openssh: authorized_keys <- {}'.format(
                ', '.join(_pyos.path.basename(_p) for _p in _pubs)))
        else:
            print('openssh: 没有找到 ~/.ssh/*.pub，先留空（需要公钥才能登录）')


after_build(_deploy_sshd)

# filename 是【相对 build-dir】的产物路径（build-dir = <build>/openssh/lib）。
# OpenSSH 的 --prefix 实际是 <build>/openssh，sshd 落在 <build>/openssh/bin/sshd，
# 比 build-dir 少一层 lib —— 用 ../ 回退一层直接指到真实位置，省掉一次拷贝。
automake("sshd", filename="../bin/sshd")

_musl_root = _pyos.path.abspath(_pyos.path.join(
    'build', str(get_plat()), str(get_arch()), str(get_config('mode')), 'musl'))
_musl_inc = _pyos.path.join(_musl_root, 'include')
_musl_lib = _pyos.path.join(_musl_root, 'lib')

# 【裸机交叉编译的三件套】照 eggs/libffmpeg 的经验：
#   configure 的 test_ld 只带 $LDFLAGS（不带 $CFLAGS），所以 LDFLAGS 里必须
#   同时给 -nostdlib 和 crt1.o/crti.o，否则 gcc 会去塞 newlib 的 crt0 并失败
#   （报 “C compiler cannot create executables”）。
_cflags = '-I' + _musl_inc + ' -O2 -fno-strict-aliasing'
_ldflags = ' '.join([
    '-nostdlib', '-static',
    # libcrypto 的 -L 必须在 LDFLAGS 里：LIBS 最前面就有 -lcrypto，而 configure
    # 最早的「C compiler can create executables」测试还没轮到 --with-ssl-dir
    # 生效，缺这个 -L 会直接报 cannot create executables。
    '-L' + _crypto_prefix,   # 注意：_crypto_prefix 已经是 .../crypto/lib
    '-L' + _musl_lib,
    _pyos.path.join(_musl_lib, 'crt1.o'),
    _pyos.path.join(_musl_lib, 'crti.o'),
])
# 【顺序】-lcrypto 必须在 -lc 之前：configure 会把 --with-ssl-dir 推出的
# -lcrypto 追加到 LIBS 末尾，而静态链接是单向解析的，排在 -lc 之后就会让
# libcrypto 里引用的 accept/gethostbyname 全部变成 undefined。
_libs = ' '.join([
    '-lcrypto',
    '-L' + _musl_lib, '-lc', '-lgcc',
    _pyos.path.join(_musl_lib, 'crtn.o'),
])

# 指纹 = 裁剪参数 + 全部编译/链接参数 + 手写版本号。
# 后者在「configure 参数有改动但没进上面几个变量」时手动递增，强制重配。
_fp = '|'.join([_cfg_args, _cflags, _ldflags, _libs, 'ac-cache-v1'])
if _old_stamp != _fp:
    for _f in ('config.status', 'Makefile'):
        _p = _pyos.path.join(_ssh_oot, _f)
        if _pyos.path.exists(_p):
            _pyos.remove(_p)
    _s = open(_stamp, 'w')
    _s.write(_fp)
    _s.close()

# 【源码补丁】musl 的 TIMESPEC_TO_TIMEVAL / TIMEVAL_TO_TIMESPEC 是表达式
# （末尾是 `(void)0`），而 OpenSSH 按 glibc 习惯把它们当语句用、后面不写
# 分号 → 展开后和下一行粘成 `(void)0 (void)gettimeofday(...)` 编译失败。
# 这里幂等地补上分号（只改本地副本）。
_bsc = _pyos.path.join(_ssh_src, 'openbsd-compat/bsd-misc.c')
try:
    _s = open(_bsc, 'r').read()
    _n = 0
    for _old, _new in (
            ('\tTIMESPEC_TO_TIMEVAL(&time2wait, req)\n',
             '\tTIMESPEC_TO_TIMEVAL(&time2wait, req);\n'),
            ('\t\tTIMEVAL_TO_TIMESPEC(&tremain, rem)\n',
             '\t\tTIMEVAL_TO_TIMESPEC(&tremain, rem);\n')):
        if _old in _s:
            _s = _s.replace(_old, _new)
            _n += 1
    if _n:
        open(_bsc, 'w').write(_s)
        print('openssh: patched bsd-misc.c ({} semicolons)'.format(_n))
except Exception as _e:
    print('openssh: patch bsd-misc.c failed:', _e)

configure(
    'CC=' + _tool + '-gcc',
    'AR=' + _tool + '-ar',
    'RANLIB=' + _tool + '-ranlib',
    'CFLAGS=' + _cflags,
    'CPPFLAGS=-I' + _musl_inc,
    'LDFLAGS=' + _ldflags,
    # 注：SIZEOF_* 不在这里传 ac_cv_*，而是在 wrapper 里 configure 成功后
    # 直接 patch config.h（见上面的 wrapper 脚本）。
    'LIBS=' + _libs,
)
