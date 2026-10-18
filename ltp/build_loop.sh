#!/bin/bash
# 错误驱动排除循环 v4：只认真正的 gcc/ld 错误行（进度行不算）
cd /Users/evil/dev/c/YiYiYa
MAX=${1:-15}
for i in $(seq 1 $MAX); do
  cd app/ltp && python3 gen.py > /dev/null && cd ../..
  OUT=$(ya -m debug -p raspi3 -b ltp 2>&1)
  # 编译错误：一行里同时有 文件路径 和 "error:"
  echo "$OUT" | grep -B40 -E "error:" | grep -oE "gen/wrappers/[a-zA-Z0-9_]+\.c" | sed 's|gen/wrappers/||; s|\.c$||' | sort -u > /tmp/ltp_failed.txt
  # 链接错误：undefined reference 前面的 .o 路径
  echo "$OUT" | grep "undefined reference" | grep -oE "objs/app/ltp/gen/wrappers/[a-zA-Z0-9_]+\.o" | sed 's|objs/app/ltp/gen/wrappers/||; s|\.o$||' | sort -u >> /tmp/ltp_failed.txt
  sort -u /tmp/ltp_failed.txt -o /tmp/ltp_failed.txt
  N=$(grep -c . /tmp/ltp_failed.txt || true)
  if [ "$N" = "0" ]; then
    echo "ROUND $i: CLEAN"
    break
  fi
  ADDED=0
  for id in $(cat /tmp/ltp_failed.txt); do
    SRC=$(grep "^$id " app/ltp/gen/idmap.txt | awk '{print $2}')
    if [ -n "$SRC" ] && ! grep -qxF "$SRC" app/ltp/gen/exclude.txt; then
      echo "$SRC" >> app/ltp/gen/exclude.txt
      ADDED=$((ADDED+1))
    fi
  done
  echo "ROUND $i: failing=$N excluded_now=$ADDED"
  if [ $ADDED -eq 0 ]; then
    echo "no progress, stop"
    echo "$OUT" | grep -E "error:" | head -3
    break
  fi
done
echo "=== final ==="
cd app/ltp && python3 gen.py > /dev/null && cd ../..
ya -m debug -p raspi3 -b ltp 2>&1 | tail -1
ls -l build/raspi3/armv8-a/debug/ltp
