#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""将源码与编译好的原生可执行打包为 .tmlext（zip 格式，宿主按 zip 解压）。"""
import os
import glob
import zipfile

OUT = "template-tml-extension-c.tmlext"
FILES = ["manifest.json", "README.md", "LICENSE", "src/main.c"]


def main():
    with zipfile.ZipFile(OUT, "w", zipfile.ZIP_DEFLATED) as z:
        for f in FILES:
            if os.path.exists(f):
                z.write(f)
        for b in glob.glob("bin/tml_ext") + glob.glob("bin/tml_ext.exe"):
            if os.path.exists(b):
                z.write(b, os.path.join("bin", os.path.basename(b)))
    print("built", OUT)


if __name__ == "__main__":
    main()
