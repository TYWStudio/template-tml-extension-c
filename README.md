<p align="right">
  <b>English</b> · <a href="README-CN.md">简体中文</a>
</p>

# TML Extension Template · C

A **C** extension template for the **TYW Launcher (TML)** extension API v1. It shows how to write a
launcher extension in dependency-free C: the host (launcher) spawns your compiled native executable
in a separate process, and the two sides exchange allow-listed methods over **NDJSON on
stdin/stdout** (one JSON object per line).

> License: **CC BY-NC 4.0** (non-commercial, attribution). See `LICENSE`.
>
> Part of the **TML** ecosystem · launcher repository: <https://github.com/TYWStudio/TML>

---

## How it runs

1. You import the packaged `*.tmlext` (essentially a zip) via **Settings → Other → Extensions** in the launcher.
2. The launcher unpacks it and starts the native executable inside the package (`entry` in `manifest.json`).
3. The launcher sends an `initialize` handshake first; the extension replies `{"ready":true}`.
4. After that the extension calls host capabilities with requests carrying `id≥2` (`log` / `host.info` / `storage` / `settings` …);
   the host broadcasts events to the extension as `event` lines and tells it to exit gracefully with `shutdown`.

Extensions are **disabled by default**, must be enabled manually by the user, and may only call the
capabilities declared in the manifest's `permissions` — calls that were not requested return no data.

## Protocol (minimal)

```
Host → Ext: {"id":1,"method":"initialize","params":{...}}    Ext → Host: {"id":1,"result":{"ready":true}}
Ext → Host: {"id":2,"method":"log","params":{"level":"info","message":"hi"}}  Host → Ext: {"id":2,"result":{}}
Host → Ext: {"method":"event","params":{"type":"game.launched","data":{...}}}
Host → Ext: {"method":"shutdown"}                                (the extension just exits)
```

`src/main.c` contains a **dependency-free** minimal JSON parser and I/O implementation, ready to use.
The host also passes configuration through environment variables (handy for native extensions):

| Variable | Meaning |
| --- | --- |
| `TML_EXT_ID` | Unique extension id |
| `TML_EXT_API` | API version (currently 1) |
| `TML_EXT_PLATFORM` | Platform (windows / linux / macos) |
| `TML_EXT_LAUNCHER_VERSION` | Launcher version |
| `TML_EXT_CHANNEL` | Channel (Release / Snapshot / Beta) |
| `TML_EXT_LANGUAGE` | UI language |
| `TML_EXT_PRIVATE` | Extension-private data directory |
| `TML_EXT_NONCE` | One-time nonce |
| `TML_EXT_PERMISSIONS` | Granted capabilities (comma-separated) |

---

## Build

Requires any C compiler: `gcc` / `clang` / MSVC (`cl`). Python 3 is used for packaging.

### Linux / macOS

```bash
./build.sh
# output: template-tml-extension-c.tmlext
```

### Windows

```bat
build.bat
 rem output: template-tml-extension-c.tmlext
```

The script compiles `src/main.c` into `bin/tml_ext` (`bin/tml_ext.exe` on Windows) with the compiler,
then `build.py` packs `manifest.json` + sources + the executable into the `.tmlext`.

## Install into the launcher

Copy the generated `template-tml-extension-c.tmlext` into the launcher's **Settings → Other → Extensions**
and click **Import**. It takes effect on the next launch, and you will see the template's debug logs in the console.

## manifest.json key fields

| Field | Description |
| --- | --- |
| `runtime` | Must be `c` (the host spawns it as a native executable) |
| `format` | Must be `tmlext` (matches the file extension) |
| `entry` | Package-relative path to the executable, e.g. `bin/tml_ext` (`.exe` is appended automatically on Windows) |
| `platforms` | The current platform must be included (`windows` / `linux` / `macos`) |
| `permissions` | Requested capability allow-list |
| `subscribe` | Event types to receive |

## Layout

```
template-tml-extension-c/
├── manifest.json        # Extension manifest
├── src/main.c           # Dependency-free C SDK + example logic
├── build.py             # Cross-platform packaging into .tmlext
├── build.sh / build.bat # Compile + package
├── README.md
└── LICENSE              # CC BY-NC 4.0
```
