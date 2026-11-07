@echo off
setlocal
if not exist bin mkdir bin
gcc -O2 -o bin\tml_ext.exe src\main.c 2>nul
if errorlevel 1 (
    cl /O2 /Fe:bin\tml_ext.exe src\main.c 2>nul
    if errorlevel 1 (
        echo 未找到 C 编译器（gcc/cl），请先安装 MinGW 或 Visual Studio
        exit /b 1
    )
)
python build.py
echo 完成：template-tml-extension-c.tmlext
