<p align="right">
  <a href="README.md">English</a> · <b>简体中文</b>
</p>

> 本仓库属于 **TML** 扩展生态；启动器仓库：<https://github.com/TYWStudio/TML>

# TML 扩展模板 · C 语言（template-tml-extension-c）

适用于 **TYW Launcher（TML）扩展 API v1** 的 **C 语言** 扩展模板。本模板演示如何
用零依赖的 C 代码编写启动器扩展：宿主（启动器）在独立进程里拉起你编译好的
原生可执行文件，双方仅通过 **stdin / stdout 的 NDJSON（每行一个 JSON）** 交换
白名单方法。

> 许可：**CC BY-NC 4.0**（禁止商业使用，署名）。见 `LICENSE`。

---

## 它是怎么跑起来的

1. 你在启动器「设置 → 其他 → 拓展」里导入打包好的 `*.tmlext`（本质是一个 zip）。
2. 启动器解压并启动包里的原生可执行文件（`manifest.json` 的 `entry`）。
3. 启动器先发一条 `initialize` 握手，扩展回 `{"ready":true}`。
4. 之后扩展通过 `id≥2` 的请求调用宿主能力（`log` / `host.info` / `storage` / `settings` …），
   宿主通过 `event` 把事件广播给扩展，并通过 `shutdown` 通知扩展优雅退出。

扩展**默认全部禁用**、需用户手动启用，且只能调用 `manifest.json` 的 `permissions`
中申请过的能力——未申请的调用宿主不会返回数据。

## 协议（极简）

```
宿主 → 扩展: {"id":1,"method":"initialize","params":{...}}    扩展 → 宿主: {"id":1,"result":{"ready":true}}
扩展 → 宿主: {"id":2,"method":"log","params":{"level":"info","message":"hi"}}  宿主 → 扩展: {"id":2,"result":{}}
宿主 → 扩展: {"method":"event","params":{"type":"game.launched","data":{...}}}
宿主 → 扩展: {"method":"shutdown"}                                （扩展直接退出）
```

`src/main.c` 内含一个**零依赖**的极简 JSON 解析与收发实现，开箱即用；
宿主还会通过环境变量下发配置（便于原生扩展读取）：

| 变量 | 含义 |
| --- | --- |
| `TML_EXT_ID` | 扩展唯一 id |
| `TML_EXT_API` | API 版本（当前为 1） |
| `TML_EXT_PLATFORM` | 平台（windows / linux / macos） |
| `TML_EXT_LAUNCHER_VERSION` | 启动器版本 |
| `TML_EXT_CHANNEL` | 通道（Release / Snapshot / Beta） |
| `TML_EXT_LANGUAGE` | 界面语言 |
| `TML_EXT_PRIVATE` | 扩展私有数据目录 |
| `TML_EXT_NONCE` | 一次性随机数 |
| `TML_EXT_PERMISSIONS` | 已授予的能力（逗号分隔） |

---

## 构建

需要任意一个 C 编译器：`gcc` / `clang` / MSVC（`cl`）。Python3 用于打包。

### Linux / macOS

```bash
./build.sh
# 产物：template-tml-extension-c.tmlext
```

### Windows

```bat
build.bat
 rem 产物：template-tml-extension-c.tmlext
```

脚本会：用编译器把 `src/main.c` 编成 `bin/tml_ext`（Windows 下为 `bin/tml_ext.exe`），
随后 `build.py` 把 `manifest.json` + 源码 + 可执行文件打进 `.tmlext`。

## 安装到启动器

把生成的 `template-tml-extension-c.tmlext` 复制到启动器「设置 → 其他 → 拓展」里
点击「导入」即可。启用后会在下次启动生效，并在控制台看到本模板打印的调试日志。

## manifest.json 关键字段

| 字段 | 说明 |
| --- | --- |
| `runtime` | 必须为 `c`（宿主按原生可执行文件拉起） |
| `format` | 必须为 `tmlext`（与文件后缀一致） |
| `entry` | 包内可执行文件相对路径，如 `bin/tml_ext`（Windows 自动补 `.exe`） |
| `platforms` | 支持的当前平台需包含 `windows` / `linux` / `macos` |
| `permissions` | 申请的能力白名单 |
| `subscribe` | 需要接收的事件类型 |

## 目录结构

```
template-tml-extension-c/
├── manifest.json        # 扩展清单
├── src/main.c           # 零依赖 C SDK + 示例逻辑
├── build.py             # 跨平台打包为 .tmlext
├── build.sh / build.bat # 编译 + 打包
├── README.md
└── LICENSE              # CC BY-NC 4.0
```
