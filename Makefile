# Makefile for the sample libretro core.
#
# Usage:
#   make [platform=<platform>] [DEBUG=1]
#   make clean
#
# Supported platforms (autodetected when `platform` is empty):
#   unix, linux   Linux x86_64 (native gcc)        -> sample_libretro.so
#   win, win64    Windows x86_64 (MinGW-w64)       -> sample_libretro.dll
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

# Linux x86_64
ifneq (,$(filter $(platform),unix linux))
   TARGET := $(TARGET_NAME)_libretro.so
   fpic   := -fPIC
   SHARED := -shared -Wl,--version-script=$(CORE_DIR)/link.T -Wl,--no-undefined
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

else
   $(error Unsupported platform '$(platform)'. Use platform=unix or platform=win)
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

# Objects go in obj/<platform>/ so switching platforms does not reuse objects.
OBJDIR  := obj/$(platform)
OBJECTS := $(patsubst $(CORE_DIR)/%,$(OBJDIR)/%,$(SOURCES_C:.c=.o) $(SOURCES_CXX:.cpp=.o))

# Link with the C++ driver only when there are C++ sources.
LINKER := $(if $(strip $(SOURCES_CXX)),$(CXX),$(CC))

# ---------------------------------------------------------------------------
# Rules
# ---------------------------------------------------------------------------

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(LINKER) -o $@ $(OBJECTS) $(LDFLAGS) $(LIBS)

$(OBJDIR)/%.o: $(CORE_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/%.o: $(CORE_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -rf $(OBJDIR) $(TARGET)

.PHONY: all clean

-include $(OBJECTS:.o=.d)
