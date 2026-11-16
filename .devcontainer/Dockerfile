FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# 1. Install build tools, the MinGW-w64 compiler (posix threading model) and Wine (for ntldd)
RUN dpkg --add-architecture i386 && \
    apt-get update && apt-get install -y --no-install-recommends \
    autoconf \
    automake \
    bc \
    bison \
    build-essential \
    ca-certificates \
    cmake \
    curl \
    file \
    flex \
    gawk \
    git \
    gpg \
    gpgv \
    libtool \
    make \
    nasm \
    ninja-build \
    p7zip-full \
    patch \
    pkg-config \
    python3 \
    sed \
    tar \
    unzip \
    wget \
    wine64 \
    xxd \
    zip \
    zstd \
    gcc-mingw-w64-x86-64-posix \
    g++-mingw-w64-x86-64-posix \
    binutils-mingw-w64-x86-64 \
    && rm -rf /var/lib/apt/lists/*

# Keep `git rev-parse` (GIT_VERSION) in libretro Makefiles from failing: the mounted repo
# belongs to a user other than root.
RUN git config --global --add safe.directory '*'

# 2. Clone quasi-msys2 in MINGW64 mode
#    Use the main repo: mirror.msys2.org redirects each download to a different mirror
#    (sometimes out of sync), and signature checks fail at random.
WORKDIR /opt/quasi-msys2
RUN git clone https://github.com/HolyBlackCat/quasi-msys2.git . && \
    sed -i 's|^MIRROR_URL := https://mirror.msys2.org$|MIRROR_URL := https://repo.msys2.org|' Makefile && \
    grep -q '^MIRROR_URL := https://repo.msys2.org$' Makefile && \
    echo "MINGW64" > msystem.txt

# 3. Install the MINGW64 libraries and create the /mingw64 symlink
#    The list is set in devcontainer.json (build.args.QUASI_MSYS2_PACKAGES); this value is only the fallback.
#    The BuildKit cache keeps downloaded packages between rebuilds: when the list changes,
#    only the new packages are downloaded.
ARG QUASI_MSYS2_PACKAGES="_ntldd _zlib _bzip2 _xz _zstd _libpng _libjpeg-turbo _SDL2 _libxml2 _glew _openal _libogg _libvorbis _flac _lua"
RUN --mount=type=cache,id=quasi-msys2-mingw64,target=/opt/quasi-msys2/cache,sharing=locked \
    make install $QUASI_MSYS2_PACKAGES && \
    ln -nfs /opt/quasi-msys2/root/mingw64 /mingw64

# 4. Compiler that the quasi-msys2 environment (env/vars.src) uses for CC/CXX.
#    native_gcc = Ubuntu's MinGW GCC (runs natively on Linux, without Wine), through the
#    win-gcc / win-g++ wrappers that add the quasi-msys2 header and library paths.
#    Setting it keeps it from changing if clang is ever installed (autodetection would prefer clang).
ENV WIN_DEFAULT_COMPILER=native_gcc

# 5. Environment script for interactive terminals and VS Code tasks (BASH_ENV)
#    Only vars.src (CC, CXX, PATH, pkg-config, CMake). all.src also tries to configure
#    binfmt_misc in the kernel and to generate Wine wrappers every time bash starts.
RUN printf '%s\n' \
    'if [ -z "$QUASI_MSYS2_ROOT" ]; then' \
    '  export QUASI_MSYS2_QUIET=1' \
    '  source /opt/quasi-msys2/env/vars.src >/dev/null' \
    '  export CC_AS="$CC"' \
    'fi' > /etc/profile.d/mingw-env.sh && \
    echo 'source /etc/profile.d/mingw-env.sh' >> /root/.bashrc

ENV BASH_ENV=/etc/profile.d/mingw-env.sh

# 6. Cross compiler for Linux aarch64 (make platform=linux-aarch64)
#    A separate layer, so adding it does not rebuild the layers above.
#    Its glibc (libc6-dev-arm64-cross) has the same version as Ubuntu's: 2.39 on 24.04.
RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc-aarch64-linux-gnu \
    g++-aarch64-linux-gnu \
    binutils-aarch64-linux-gnu \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspaces
