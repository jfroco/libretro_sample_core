# Makefile for the sample libretro core.
#
# Usage:
#   make [platform=<platform>] [DEBUG=1]
#   make strip [platform=<platform>]   build, then strip the core in the root (for releases)
#   make clean
#
# Supported platforms (autodetected when `platform` is empty):
#   unix, linux     Linux, native gcc (x86_64 here; aarch64 on an ARM64 host) -> sample_libretro.so
#   linux-aarch64   Linux aarch64, cross-compiled with aarch64-linux-gnu-gcc  -> sample_libretro.so
#   win, win64      Windows x86_64 (MinGW-w64)                                -> sample_libretro.dll
#
# `win64` and `unix` are the values libretro's CI uses (see .gitlab-ci.yml).
# `linux-aarch64` is our own name for cross-compiling in the devcontainer.
#
# To add a platform, add a block below that sets TARGET, fpic and SHARED.
# The list of sources lives in Makefile.common (also used by jni/Android.mk later).

TARGET_NAME := sample
CORE_DIR    := .
DEBUG       ?= 0

# ---------------------------------------------------------------------------
# Platform detection
# ---------------------------------------------------------------------------

ifeq ($(platform),)
   platform = unix
   UNAME := $(shell uname -s 2>/dev/null)
   ifeq ($(UNAME),)
      platform = win
   else ifneq ($(findstring MINGW,$(UNAME)),)
      platform = win
   else ifneq ($(findstring MSYS,$(UNAME)),)
      platform = win
   else ifneq ($(findstring Darwin,$(UNAME)),)
      platform = osx
   endif
endif

# ---------------------------------------------------------------------------
# Version (shown by the frontend as "1.0 <commit>")
# ---------------------------------------------------------------------------

GIT_VERSION := " $(shell git rev-parse --short HEAD 2>/dev/null || echo unknown)"
ifneq ($(GIT_VERSION)," unknown")
   CFLAGS += -DGIT_VERSION=\"$(GIT_VERSION)\"
endif

# ---------------------------------------------------------------------------
# Platforms
# ---------------------------------------------------------------------------

# Linux (native, or aarch64 cross-compiled)
ifneq (,$(filter $(platform),unix linux linux-aarch64))
   TARGET := $(TARGET_NAME)_libretro.so
   fpic   := -fPIC
   SHARED := -shared -Wl,--version-script=$(CORE_DIR)/link.T -Wl,--no-undefined
   STRIP  ?= strip
   ifeq ($(platform),linux-aarch64)
      # Cross-compile, unless the host is already aarch64 (then the native gcc is used).
      ifneq ($(shell uname -m),aarch64)
         CC    := aarch64-linux-gnu-gcc
         CXX   := aarch64-linux-gnu-g++
         STRIP := aarch64-linux-gnu-strip
      endif
   endif
   # The devcontainer exports CC/CXX for Windows (quasi-msys2 win-gcc/win-g++).
   # Use the native compiler for Linux builds unless one was given on the command line.
   ifneq (,$(findstring win-,$(CC)))
      CC := gcc
   endif
   ifneq (,$(findstring win-,$(CXX)))
      CXX := g++
   endif

# Windows x86_64 (MinGW-w64)
# -static links libgcc, libstdc++ and libwinpthread into the DLL, so the core
# does not depend on MinGW DLLs that RetroArch does not ship.
else ifneq (,$(filter $(platform),win win64))
   TARGET := $(TARGET_NAME)_libretro.dll
   fpic   :=
   SHARED := -shared -static -static-libgcc -static-libstdc++ -Wl,--no-undefined
   STRIP  ?= x86_64-w64-mingw32-strip

else
   $(error Unsupported platform '$(platform)'. Use platform=win, platform=unix or platform=linux-aarch64)
endif

# ---------------------------------------------------------------------------
# Flags
# ---------------------------------------------------------------------------

include Makefile.common

ifeq ($(DEBUG),1)
   CFLAGS   += -O0 -g
   CXXFLAGS += -O0 -g
else
   CFLAGS   += -O2 -DNDEBUG
   CXXFLAGS += -O2 -DNDEBUG
endif

WARNINGS := -Wall -Wextra

CFLAGS   += $(fpic) $(INCFLAGS) $(WARNINGS) -MMD -MP
CXXFLAGS += $(fpic) $(INCFLAGS) $(WARNINGS) -MMD -MP
LDFLAGS  += $(fpic) $(SHARED)

# Objects go in obj/<target triple>/ (for example obj/aarch64-linux-gnu/), so
# switching platform or compiler does not reuse objects built for another one.
TRIPLE  := $(shell $(CC) -dumpmachine 2>/dev/null)
OBJDIR  := obj/$(if $(TRIPLE),$(TRIPLE),$(platform))
OBJECTS := $(patsubst $(CORE_DIR)/%,$(OBJDIR)/%,$(SOURCES_C:.c=.o) $(SOURCES_CXX:.cpp=.o))

# Link with the C++ driver only when there are C++ sources.
LINKER := $(if $(strip $(SOURCES_CXX)),$(CXX),$(CC))

# ---------------------------------------------------------------------------
# Rules
# ---------------------------------------------------------------------------

# Each platform links inside its own OBJDIR, and `all` copies the result to the
# root, where RetroArch and libretro's CI expect it. Both Linux builds produce
# sample_libretro.so; copying every time makes the file in the root always match
# the platform you just built, instead of make treating the other one's .so as
# up to date.
all: $(OBJDIR)/$(TARGET)
	cp $< $(TARGET)

$(OBJDIR)/$(TARGET): $(OBJECTS)
	$(LINKER) -o $@ $(OBJECTS) $(LDFLAGS) $(LIBS)

$(OBJDIR)/%.o: $(CORE_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/%.o: $(CORE_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Strip only the copy in the root: the one in OBJDIR keeps its symbols for debugging.
# --strip-unneeded keeps the exported retro_* functions.
# libretro's CI does not need this target: its templates strip the core after `make`.
strip: all
	$(STRIP) --strip-unneeded $(TARGET)

clean:
	rm -rf $(OBJDIR) $(TARGET)

.PHONY: all strip clean

-include $(OBJECTS:.o=.d)
