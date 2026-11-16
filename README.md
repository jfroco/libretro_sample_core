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
| `Makefile` | Per-platform build (`platform=win`, `platform=unix`, `platform=linux-aarch64`) |
| `Makefile.common` | Sources and include paths, shared by all platforms |
| `link.T` | Exports only the `retro_*` symbols (Linux) |
| `sample_libretro.info` | Core metadata for RetroArch |
| `.gitlab-ci.yml` | Build recipe for libretro's CI |
| `.devcontainer/` | Ubuntu 24.04 image with MinGW-w64, quasi-msys2 and the aarch64 cross compiler |
| `.vscode/tasks.json` | Build and check tasks |

## Building

Open the folder in VS Code and choose **Reopen in Container**. Inside the container:

```bash
make -j$(nproc) platform=win             # -> sample_libretro.dll (Windows x86_64)
make -j$(nproc) platform=unix            # -> sample_libretro.so  (Linux x86_64)
make -j$(nproc) platform=linux-aarch64   # -> sample_libretro.so  (Linux aarch64)
make DEBUG=1 platform=win                # no optimization, with debug symbols
make -j$(nproc) platform=win strip       # build, then strip (for releases)
make clean platform=win
```

These are also available as VS Code tasks (`Ctrl+Shift+B` builds for Windows).

The devcontainer sets `platform=win`, so `make` with no arguments builds for Windows. Object files
go in `obj/<target triple>/` (for example `obj/aarch64-linux-gnu/`), so there is no need to run
`make clean` when switching platforms.

Each build is linked in `obj/<target triple>/` and then copied to the root. Both Linux builds are
named `sample_libretro.so`, because that is the name RetroArch expects, so the file in the root is
always the one for the platform you built last. The other one stays in its `obj/` folder, for
example `obj/aarch64-linux-gnu/sample_libretro.so`.

### Stripping for releases

`make strip` builds the core and then removes the symbols it does not need with
`<strip> --strip-unneeded`, using the right tool for each platform:

| Platform | Strip tool |
|---|---|
| `win` | `x86_64-w64-mingw32-strip` |
| `unix` | `strip` |
| `linux-aarch64` | `aarch64-linux-gnu-strip` |

The exported `retro_*` functions are kept, so the core still loads. Only the copy in the root is
stripped; the one in `obj/<target triple>/` keeps its symbols for debugging. Use the
**Release … (stripped)** tasks in VS Code, or pass `STRIP=<tool>` to use another strip tool.

libretro's CI does not use this target: its templates set `STRIP_CORE_LIB: 1` and strip the core
themselves after running `make`.

### Linux aarch64 (handhelds)

`platform=linux-aarch64` cross-compiles from the x86_64 devcontainer with
`aarch64-linux-gnu-gcc`. To the Makefile it is the same as `platform=unix` with a different
compiler. The name is ours: libretro's CI builds aarch64 with `platform=unix` and a native `gcc` on
an ARM64 machine (see [Getting a core built by libretro](#getting-a-core-built-by-libretro)). If you
run `platform=linux-aarch64` on an aarch64 host, the Makefile uses the native `gcc`.

The build targets generic ARMv8-A, so it runs on any aarch64 device. To tune it for a specific CPU
without losing compatibility, pass `-mtune` through the environment (a `CFLAGS=` argument after
`make` would replace all the Makefile's flags instead of adding to them):

```bash
CFLAGS="-mtune=cortex-a53" make -j$(nproc) platform=linux-aarch64
```

The core must match RetroArch's architecture: a 64-bit RetroArch (aarch64) needs this build. Some
handheld distributions also ship a 32-bit RetroArch (armhf), which would be another target.

On Knulli/Batocera, copy `sample_libretro.so` to `/usr/lib/libretro/` and `sample_libretro.info`
to `/usr/share/libretro/info/`.

The glibc rules below also apply here. The cross compiler's glibc has the same version as the
devcontainer's Ubuntu (2.39 on 24.04). On aarch64 the lowest version a core can require is
`GLIBC_2.17`, the first glibc release for that architecture; this sample requires exactly that.

### Checking the result

```bash
file sample_libretro.dll
x86_64-w64-mingw32-objdump -p sample_libretro.dll | grep "DLL Name"
x86_64-w64-mingw32-objdump -p sample_libretro.dll | grep -oE "retro_[a-z_]+" | sort -u
```

The DLL must depend only on system DLLs (`KERNEL32.dll`, `msvcrt.dll`) and export the 25
`retro_*` functions. For Linux, use the **Check Linux .so** task (or **Check Linux aarch64 .so**),
which also shows the highest glibc version the core requires.

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
   `devcontainer.json` with `"containerEnv": { "platform": "unix" }`. For aarch64, also install
   `gcc-aarch64-linux-gnu`, `g++-aarch64-linux-gnu` and `binutils-aarch64-linux-gnu`.
3. VS Code asks which configuration to open on **Reopen in Container**. The Makefile does not
   change.

Since glibc 2.35 is the ceiling of that container, everything built there runs on systems with
glibc 2.35 or newer, such as Lakka 5.x and 6.x.

To match the official libretro buildbot (Ubuntu 16.04, glibc 2.23, GCC 9), you can use its image
`git.libretro.com:5050/libretro-infrastructure/libretro-build-amd64-ubuntu:xenial-gcc9`, at the
cost of an older compiler and tools.

## Testing in RetroArch

1. Copy `sample_libretro.dll` to RetroArch's `cores/` folder and `sample_libretro.info` to `info/`.
2. **Load Core** → *Sample (Hello World)* → **Start Core**.
3. The options (color, speed, test tone) are in **Quick Menu → Core Options**.

## Using it as a base for another core

1. Change `TARGET_NAME` in the `Makefile` and `CORENAME` in `.gitlab-ci.yml` (they must match),
   rename `src/sample_libretro.c` and `sample_libretro.info`, and update `Makefile.common`.
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
| Linux aarch64 | `linux-aarch64` (cross), `unix` (native) | ✅ |
| macOS, Android, iOS, Emscripten, Linux armhf, consoles | | Planned |

## Getting a core built by libretro

libretro can build a core for every platform and publish it in RetroArch's **Online Updater**. This
is meant for the emulators you port, not for this sample: libretro already has its own samples
(libretro-samples). Before asking, check that the emulator's license allows it to be distributed;
cores with non-commercial licenses are accepted but left out of some distributions.

### 1. The build recipe: `.gitlab-ci.yml`

libretro does not build from GitHub. Their team mirrors the repository on their GitLab server
(git.libretro.com), where `.gitlab-ci.yml` in the root of the main branch lists the platforms to
build. GitHub ignores the file, and it cannot be run locally.

Each job extends a ready-made template from
[libretro-infrastructure/ci-templates](https://git.libretro.com/libretro-infrastructure/ci-templates),
which runs `make` with these values:

| Job | Template | What it passes to the Makefile |
|---|---|---|
| `libretro-build-windows-x64` | `windows-x64-mingw.yml` | `platform=win64`, `CC=x86_64-w64-mingw32.static-gcc` |
| `libretro-build-linux-x64` | `linux-x64.yml` | `platform=unix`, `CC=gcc` (Ubuntu 16.04, glibc 2.23) |
| `libretro-build-linux-aarch64` | `linux-aarch64.yml` | `platform=unix`, `CC=gcc` (native on ARM64) |

The Makefile supports all of these values, so the recipe needs no extra variables. The templates
expect `<CORENAME>_libretro.dll` or `.so` in the root of the repository: `CORENAME` in
`.gitlab-ci.yml` must match `TARGET_NAME` in the Makefile. When you add a platform, add its
template to `include:` and a job that extends its `-make-default` job; the
[snes9x recipe](https://github.com/libretro/snes9x/blob/master/.gitlab-ci.yml) shows many
platforms.

### 2. The core info file

Open a pull request with `<name>_libretro.info` to
[libretro-super](https://github.com/libretro/libretro-super), in `dist/info/`. That folder has an
annotated example, `00_example_libretro.info`. Without this file the core does not show up
correctly in the Online Updater.

### 3. Ask the libretro team to add the core

Ask the team to mirror the repository on their GitLab and enable CI. The usual place is the
`#programming` channel of the libretro Discord. In the past the repository had to be moved to the
`libretro` GitHub organization (with the maintainer added back as an admin); this may no longer be
required.

Once it is enabled, builds appear on buildbot.libretro.com and then in the Online Updater.

### 4. Optional

- Document the core in [libretro-docs](https://github.com/libretro/docs).
- Add icons and playlist content to RetroArch's assets, and the games to the libretro database.

See also [Developing cores](https://docs.libretro.com/development/cores/developing-cores/) in the
libretro docs.

## License

MIT, see [LICENSE](LICENSE). Third-party licenses are listed in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
