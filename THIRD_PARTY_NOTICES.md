# Third-party notices

This repository includes code from the following projects.

## libretro_hello_world_core

- Source: https://github.com/Lightnet/libretro_hello_world_core
- Author: Lightnet
- License: MIT (stated in the project's README; the project has no LICENSE file)
- Used in:
  - `src/sample_libretro.c`: rewritten from the original `src/lib.c`.
  - `src/font.h`: 8x8 font, copied without changes to the glyphs. The original
    project does not say where the font comes from.

## libretro.h (libretro API)

- Source: https://github.com/libretro/libretro-common, file `include/libretro.h`
- Commit: `52781c4a8b5aae19d858f4a2326c8f8308c70f54`
- Copyright (C) 2010-2024 The RetroArch team
- License: MIT. The full license text is at the top of
  `libretro-common/include/libretro.h`.

To update the header, download it again from libretro-common and update the
commit above.

## Build tools (not distributed)

The devcontainer (`.devcontainer/`) downloads Ubuntu, MinGW-w64 and
[quasi-msys2](https://github.com/HolyBlackCat/quasi-msys2) when the image is
built. None of their files are part of this repository or of the compiled core,
except the parts of the compiler runtime that are linked statically into the
Windows DLL (libgcc, under the GCC Runtime Library Exception, and the MinGW-w64
runtime, under permissive licenses). Neither license imposes conditions on the
core.
