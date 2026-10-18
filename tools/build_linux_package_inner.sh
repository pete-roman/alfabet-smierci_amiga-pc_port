#!/bin/sh
# Build static SDL3/libopenmpt and stage the final Linux tarball. Run inside the
# pinned Ubuntu 22.04 build image from build_linux_package.sh or equivalent.
set -eu
cd "$(dirname "$0")/.."
ROOT=$(pwd)
JOBS=${JOBS:-4}
DEPS_SRC="$ROOT/deps/linux-src"
DEPS_WORK="$ROOT/deps/linux-build"
DEPS_PREFIX="$ROOT/deps/linux-install-ubuntu22.04-x86_64"
SDL_ARCHIVE="$DEPS_SRC/SDL3-3.4.18.tar.gz"
MPT_ARCHIVE="$DEPS_SRC/libopenmpt-0.8.9+release.makefile.tar.gz"

mkdir -p "$DEPS_SRC" "$DEPS_WORK/src"
fetch_verified() {
    url=$1
    path=$2
    expected=$3
    if [ ! -f "$path" ]; then
        curl --proto '=https' --tlsv1.2 --retry 3 -fL "$url" -o "$path"
    fi
    printf '%s  %s\n' "$expected" "$path" | sha256sum -c -
}

# SDL3's digest is published on the upstream release page. The libopenmpt
# digest pins the official release archive fetched over HTTPS.
fetch_verified \
    https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-3.4.18.tar.gz \
    "$SDL_ARCHIVE" 9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3
fetch_verified \
    'https://lib.openmpt.org/files/libopenmpt/src/libopenmpt-0.8.9+release.makefile.tar.gz' \
    "$MPT_ARCHIVE" 9273b88b67973cc69e54d748ab1b749399d6d07695f1c37d0c59f88b4106074f

if [ ! -d "$DEPS_WORK/src/SDL3-3.4.18" ]; then
    mkdir -p "$DEPS_WORK/src/SDL3-3.4.18"
    tar xzf "$SDL_ARCHIVE" -C "$DEPS_WORK/src/SDL3-3.4.18" --strip-components=1
fi
if [ ! -d "$DEPS_WORK/src/libopenmpt-0.8.9" ]; then
    mkdir -p "$DEPS_WORK/src/libopenmpt-0.8.9"
    tar xzf "$MPT_ARCHIVE" -C "$DEPS_WORK/src/libopenmpt-0.8.9" --strip-components=1
fi

cmake -S "$DEPS_WORK/src/SDL3-3.4.18" -B "$DEPS_WORK/build-sdl3" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$DEPS_PREFIX" \
    -DCMAKE_INSTALL_LIBDIR=lib -DSDL_SHARED=OFF -DSDL_STATIC=ON \
    -DSDL_DEPS_SHARED=ON -DSDL_TEST_LIBRARY=OFF -DSDL_TESTS=OFF \
    -DSDL_EXAMPLES=OFF -DSDL_INSTALL_TESTS=OFF -DSDL_INSTALL_CPACK=OFF \
    -DSDL_X11_XTEST=OFF
cmake --build "$DEPS_WORK/build-sdl3" --parallel "$JOBS"
cmake --install "$DEPS_WORK/build-sdl3"

make -C "$DEPS_WORK/src/libopenmpt-0.8.9" -j "$JOBS" CONFIG=gcc \
    CC=gcc CXX=g++ AR=ar LD=g++ SHARED_LIB=0 STATIC_LIB=1 \
    EXAMPLES=0 OPENMPT123=0 TEST=0 NO_ZLIB=1 NO_MPG123=1 NO_OGG=1 \
    NO_VORBIS=1 NO_VORBISFILE=1 NO_PORTAUDIO=1 NO_PORTAUDIOCPP=1 \
    NO_PULSEAUDIO=1 NO_SDL2=1 NO_FLAC=1 NO_SNDFILE=1
make -C "$DEPS_WORK/src/libopenmpt-0.8.9" -j "$JOBS" CONFIG=gcc \
    CC=gcc CXX=g++ AR=ar LD=g++ SHARED_LIB=0 STATIC_LIB=1 \
    EXAMPLES=0 OPENMPT123=0 TEST=0 NO_ZLIB=1 NO_MPG123=1 NO_OGG=1 \
    NO_VORBIS=1 NO_VORBISFILE=1 NO_PORTAUDIO=1 NO_PORTAUDIOCPP=1 \
    NO_PULSEAUDIO=1 NO_SDL2=1 NO_FLAC=1 NO_SNDFILE=1 \
    PREFIX="$DEPS_PREFIX" install

cmake -S "$ROOT" -B "$ROOT/build/linux-package" -DCMAKE_BUILD_TYPE=Release \
    -DALF_STATIC_THIRD_PARTY=ON -DALF_DEPS_PREFIX="$DEPS_PREFIX"
cmake --build "$ROOT/build/linux-package" --parallel "$JOBS"

VERSION=$(sed -n 's/^#define PORT_VERSION "\([^"]*\)"/\1/p' "$ROOT/src/version.h")
PACKAGE_NAME="AlfabetSmierci-$VERSION-linux-x86_64"
DIST="$ROOT/dist/linux"
STAGE="$DIST/$PACKAGE_NAME"
if [ -d "$STAGE" ]; then
    chmod -R u+w "$STAGE"
fi
rm -rf "$STAGE"
mkdir -p "$STAGE/data/scripts" "$STAGE/licenses"
cp "$ROOT/build/linux-package/AlfabetSmierci/AlfabetSmierci" "$STAGE/AlfabetSmierci"
cp "$ROOT/AlfabetSmierci/data/"disk*.adf "$STAGE/data/"
cp "$ROOT/scripts/"*.alf "$STAGE/data/scripts/"
cp "$ROOT/LICENSE" "$STAGE/LICENSE.txt"
cp "$ROOT/CREDITS.md" "$STAGE/CREDITS.md"
cp "$DEPS_WORK/src/SDL3-3.4.18/LICENSE.txt" "$STAGE/licenses/SDL3-zlib.txt"
cp "$DEPS_WORK/src/libopenmpt-0.8.9/LICENSE" "$STAGE/licenses/libopenmpt-BSD-3-Clause.txt"
cp "$DEPS_WORK/src/libopenmpt-0.8.9/include/miniz/LICENSE" "$STAGE/licenses/miniz-MIT.txt"
cp "$DEPS_WORK/src/libopenmpt-0.8.9/include/minimp3/LICENSE" "$STAGE/licenses/minimp3-CC0-1.0.txt"
cp "$DEPS_WORK/src/libopenmpt-0.8.9/src/mpt/LICENSE.BSL-1.0.txt" "$STAGE/licenses/libopenmpt-mpt-BSL-1.0.txt"
cp "$DEPS_WORK/src/libopenmpt-0.8.9/src/mpt/LICENSE.BSD-3-Clause.txt" "$STAGE/licenses/libopenmpt-mpt-BSD-3-Clause.txt"
cp "$DEPS_WORK/src/SDL3-3.4.18/src/video/yuv2rgb/LICENSE" "$STAGE/licenses/SDL3-yuv2rgb-BSD-3-Clause.txt"
cp "$DEPS_WORK/src/SDL3-3.4.18/src/hidapi/LICENSE.txt" "$STAGE/licenses/SDL3-hidapi-license-options.txt"
cp "$DEPS_WORK/src/SDL3-3.4.18/src/hidapi/LICENSE-bsd.txt" "$STAGE/licenses/SDL3-hidapi-BSD.txt"
sed -n '/This software is available under 2 licenses/,$p' \
    "$DEPS_WORK/src/libopenmpt-0.8.9/include/stb_vorbis/stb_vorbis.c" \
    > "$STAGE/licenses/stb_vorbis-MIT-or-Unlicense.txt"
python3 - "$ROOT/tools/make_licenses.py" "$STAGE/licenses/ProWizard-MIT.txt" <<'PY'
import pathlib, runpy, sys
license_text = runpy.run_path(sys.argv[1])["PROWIZARD"]
pathlib.Path(sys.argv[2]).write_text(license_text, encoding="utf-8")
PY
cp /usr/share/common-licenses/GPL-3 "$STAGE/licenses/GPL-3.0.txt"
awk 'found || /^GCC RUNTIME LIBRARY EXCEPTION$/ {found=1; print}' \
    "$ROOT/AlfabetSmierci/THIRD_PARTY_LICENSES.txt" \
    > "$STAGE/licenses/GCC-Runtime-Library-Exception-3.1.txt"
cat > "$STAGE/THIRD_PARTY_LICENSES.txt" <<'NOTICES'
Third-party code included in the Linux executable
==================================================

SDL3 3.4.18 is statically linked. Its zlib license is in
licenses/SDL3-zlib.txt.

libopenmpt 0.8.9 is statically linked. Its BSD-3-Clause license is in
licenses/libopenmpt-BSD-3-Clause.txt. The build disables external zlib,
MPG123, Ogg, Vorbis, PortAudio, FLAC and libsndfile dependencies; libopenmpt's
bundled miniz, minimp3, stb_vorbis, mpt BSL and mpt BSD components are covered
by the files in licenses/.

SDL's yuv2rgb and HIDAPI components are covered by the licenses in licenses/.
The HIDAPI build uses the BSD license option.

The port includes a ProWizard-derived converter for The Player 6.0A modules;
its MIT license is in licenses/ProWizard-MIT.txt.

GCC's libgcc and libstdc++ runtime portions are statically linked. The GPL-3.0
text and GCC Runtime Library Exception 3.1 are in licenses/.

The operating-system SDL backends (X11, Wayland, ALSA and PulseAudio) load the
host's system libraries dynamically; those libraries are not included here.
NOTICES
cp "$ROOT/tools/linux-package-README.txt" "$STAGE/README.txt"
cat > "$STAGE/uruchom.sh" <<'LAUNCHER'
#!/bin/sh
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$DIR"
exec "$DIR/AlfabetSmierci" "$@"
LAUNCHER
cat > "$STAGE/BUILD-INFO.txt" <<INFO
Alfabet Smierci Linux package
Release: $VERSION
Source commit: $(git -C "$ROOT" rev-parse HEAD)
Architecture: x86_64
Build baseline: Ubuntu 22.04, glibc 2.35
SDL3: 3.4.18 (static)
libopenmpt: 0.8.9 (static)
SDL Linux display/audio backends use libraries installed by the host operating system.
INFO
chmod 755 "$STAGE/AlfabetSmierci" "$STAGE/uruchom.sh"
chmod -R a-w "$STAGE/data"
(cd "$STAGE" && find . -type f ! -name SHA256SUMS -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS)

mkdir -p "$DIST"
SOURCE_DATE_EPOCH=$(git -C "$ROOT" log -1 --format=%ct)
ARCHIVE="$DIST/$PACKAGE_NAME.tar.gz"
tar --sort=name --owner=0 --group=0 --numeric-owner --mtime="@$SOURCE_DATE_EPOCH" \
    -C "$DIST" -cf - "$PACKAGE_NAME" | gzip -n > "$ARCHIVE"
(cd "$DIST" && sha256sum "$(basename "$ARCHIVE")" > "$(basename "$ARCHIVE").sha256")
printf 'Packaged %s\n' "$ARCHIVE"
