#!/usr/bin/env bash
# 构建 C 扩展并打包为 .tmlext
set -e
mkdir -p bin
CC="${CC:-gcc}"
"$CC" -O2 -Wall -o bin/tml_ext src/main.c
chmod +x bin/tml_ext
python3 build.py
echo "完成：template-tml-extension-c.tmlext"
