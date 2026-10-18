#!/bin/bash
# LTP runner 两段式构建（符号本地化流程，详见 app/ltp/ya.py 头部注释）
# 用法: ./build_all.sh [平台，默认 raspi3] [模式，默认 debug]
set -e
ROOT=/Users/evil/dev/c/YiYiYa
cd "$ROOT"
PLAT="${1:-raspi3}"
MODE="${2:-debug}"
ARCH=armv8-a
BUILD=$ROOT/build/$PLAT/$ARCH/$MODE

python3 "$ROOT/app/ltp/gen.py"
/usr/local/bin/ya -m "$MODE" -p "$PLAT" -b ltpwrap
python3 "$ROOT/app/ltp/localize.py" "$BUILD/objs"
/usr/local/bin/ya -m "$MODE" -p "$PLAT" -b ltp
ls -la "$BUILD/ltp"
