# libretro_sample_core

A minimal libretro core to use as a starting point for porting emulators to libretro, built with a
VS Code devcontainer.

It runs without content ("Start Core" in RetroArch): it shows a square you move with the D-Pad,
outputs audio (silence or a test tone) and supports core options and save states.

Based on [libretro_hello_world_core](https://github.com/Lightnet/libretro_hello_world_core) by
Lightnet.

## Layout

| Path | Contents |
|---|---|
| `src/sample_libretro.c` | The core: implementation of the libretro API |
| `src/font.h` | 8x8 font used to draw text |
| `libretro-common/include/libretro.h` | libretro API header (copied from libretro-common) |
| `Makefile` | Per-platform build (`platform=win`, `platform=unix`) |
| `Makefile.common` | Sources and include paths, shared by all platforms |
| `link.T` | Exports only the `retro_*` symbols (Linux) |
| `sample_libretro.info` | Core metadata for RetroArch |
| `.devcontainer/` | Ubuntu 24.04 image with MinGW-w64 and quasi-msys2 |
| `.vscode/tasks.json` | Build and check tasks |

## Building

Open the folder in VS Code and choose **Reopen in Container**. Inside the container:

```bash
make -j$(nproc) platform=win     # -> sample_libretro.dll (Windows x86_64)
make -j$(nproc) platform=unix    # -> sample_libretro.so  (Linux x86_64)
make DEBUG=1 platform=win        # no optimization, with debug symbols
make clean platform=win
```

These are also available as VS Code tasks (`Ctrl+Shift+B` builds for Windows).

The devcontainer sets `platform=win`, so `make` with no arguments builds for Windows. Object files
for each platform go in `obj/<platform>/`, so there is no need to run `make clean` when switching
platforms.

### Checking the result

```bash
file sample_libretro.dll
x86_64-w64-mingw32-objdump -p sample_libretro.dll | grep "DLL Name"
x86_64-w64-mingw32-objdump -p sample_libretro.dll | grep -oE "retro_[a-z_]+" | sort -u
```

The DLL must depend only on system DLLs (`KERNEL32.dll`, `msvcrt.dll`) and export the 25
`retro_*` functions. For Linux, use the **Check Linux .so** task, which also shows the highest glibc
version the core requires.

### glibc compatibility on Linux

A `.so` runs on systems with the **same or a newer** glibc version than the one it requires. Each
glibc function has a version, and the core requires the highest one among the functions it uses.

- The container is Ubuntu 24.04 (glibc 2.39). That is the **ceiling**: a core built here never
  requires more than 2.39, which the current Lakka release (6.x) supports.
- This sample core requires only `GLIBC_2.4`, because it uses very old functions.
- An emulator may require more. For example, threads (`pthread_create`) require `GLIBC_2.34`, and
  some functions such as `strtol` or `sscanf` may require `GLIBC_2.38`.

Check the version with the **Check Linux .so** task after building. The last line shows the
highest version the core requires.

#### If you need glibc 2.35 (for example, Lakka 5.x)

If a core must run on a system with glibc 2.35 and the task shows a higher version, build it in a
**second devcontainer** based on `ubuntu:22.04` (glibc 2.35, GCC 11):

1. Move the current configuration to `.devcontainer/mingw64/` (Windows stays on Ubuntu 24.04: the
   DLL does not depend on glibc, and a modern MinGW is better).
2. Create `.devcontainer/linux-x64/` with a Dockerfile `FROM ubuntu:22.04` that installs
   `build-essential`, `git` and `pkg-config` (it needs neither MinGW nor quasi-msys2), and a
   `devcontainer.json` with `"containerEnv": { "platform": "unix" }`.
3. VS Code asks which configuration to open on **Reopen in Container**. The Makefile does not
   change.

Since glibc 2.35 is the ceiling of that container, everything built there runs on Lakka 5.x and
6.x.

To match the official libretro buildbot (Ubuntu 16.04, glibc 2.23, GCC 9), you can use its image
`git.libretro.com:5050/libretro-infrastructure/libretro-build-amd64-ubuntu:xenial-gcc9`, at the
cost of an older compiler and tools.

## Testing in RetroArch

1. Copy `sample_libretro.dll` to RetroArch's `cores/` folder and `sample_libretro.info` to `info/`.
2. **Load Core** → *Sample (Hello World)* → **Start Core**.
3. The options (color, speed, test tone) are in **Quick Menu → Core Options**.

## Using it as a base for another core

1. Change `TARGET_NAME` in the `Makefile`, rename `src/sample_libretro.c` and
   `sample_libretro.info`, and update `Makefile.common`.
2. Change `CORE_NAME`, the option keys (`sample_*`), and `valid_extensions` / `need_fullpath` in
   `retro_get_system_info`.
3. Add the emulator's sources to `SOURCES_C` / `SOURCES_CXX` in `Makefile.common`.
4. If the emulator needs libraries (zlib, libpng…), link them statically: a core must be a single
   file with no dependencies the frontend does not ship.

## Platforms

| Platform | `platform=` | Status |
|---|---|---|
| Windows x86_64 (MinGW-w64) | `win`, `win64` | ✅ |
| Linux x86_64 | `unix`, `linux` | ✅ |
| macOS, Android, iOS, Emscripten, Linux ARM, consoles | | Planned |

## License

MIT, see [LICENSE](LICENSE). Third-party licenses are listed in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
