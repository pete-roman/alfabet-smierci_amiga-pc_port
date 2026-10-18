#!/bin/sh
# Wrap the already built, self-contained Linux package in a pinned AppImage runtime.
# Does not install anything system-wide and does not need FUSE to build.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
VERSION=$(sed -n 's/^#define PORT_VERSION "\([^"]*\)"/\1/p' "$ROOT/src/version.h")
NAME="AlfabetSmierci-$VERSION-linux-x86_64"
STAGE="$ROOT/dist/linux/$NAME"
APPDIR="$ROOT/build/appimage/AlfabetSmierci.AppDir"
TOOLS="$ROOT/tools/toolchain/appimage-1.9.1"
OUTPUT="$ROOT/dist/linux/$NAME.AppImage"

[ "$(uname -s)" = Linux ] && [ "$(uname -m)" = x86_64 ] || {
    echo 'AppImage packaging currently requires Linux x86_64.' >&2
    exit 1
}
[ -x "$STAGE/AlfabetSmierci" ] || {
    echo 'Build the portable Linux package first: sh tools/build_linux_package.sh' >&2
    exit 1
}
mkdir -p "$TOOLS" "$(dirname -- "$APPDIR")"
fetch() {
    url=$1
    target=$2
    digest=$3
    if [ ! -f "$target" ]; then
        curl --proto '=https' --tlsv1.2 --connect-timeout 10 --max-time 120 \
            -fsSL --retry 3 "$url" -o "$target.part"
        printf '%s  %s\n' "$digest" "$target.part" | sha256sum -c -
        mv "$target.part" "$target"
    fi
    printf '%s  %s\n' "$digest" "$target" | sha256sum -c -
}
fetch https://github.com/AppImage/appimagetool/releases/download/1.9.1/appimagetool-x86_64.AppImage \
    "$TOOLS/appimagetool.AppImage" ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0
fetch https://github.com/AppImage/type2-runtime/releases/download/20251108/runtime-x86_64 \
    "$TOOLS/runtime-x86_64" 2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d
fetch https://raw.githubusercontent.com/AppImage/type2-runtime/20251108/LICENSE \
    "$TOOLS/runtime-LICENSE" aa154fc9070614bbe7921f89db11efd1dba7a1f3a41685958110e2230f9c0ca1
fetch 'https://git.musl-libc.org/cgit/musl/plain/COPYRIGHT?h=v1.2.5' \
    "$TOOLS/musl-COPYRIGHT" f9bc4423732350eb0b3f7ed7e91d530298476f8fec0c6c427a1c04ade22655af
fetch https://raw.githubusercontent.com/libfuse/libfuse/fuse-3.15.0/LGPL2.txt \
    "$TOOLS/libfuse-LGPL-2.0.txt" dc626520dcd53a22f727af3ee42c770e56c97a64fe3adb063799d8ab032fe551
fetch https://raw.githubusercontent.com/vasi/squashfuse/0.5.2/LICENSE \
    "$TOOLS/squashfuse-BSD.txt" 9e909cc8a8ba27b1a649c964cf9eda37de911f3138c2b64e6d2f01703904ac13
fetch https://raw.githubusercontent.com/facebook/zstd/v1.5.6/LICENSE \
    "$TOOLS/zstd-BSD.txt" 7055266497633c9025b777c78eb7235af13922117480ed5c674677adc381c9d8
fetch https://raw.githubusercontent.com/madler/zlib/v1.3.1/LICENSE \
    "$TOOLS/zlib-LICENSE.txt" 845efc77857d485d91fb3e0b884aaa929368c717ae8186b66fe1ed2495753243
chmod 755 "$TOOLS/appimagetool.AppImage"
# Extract the verified tool on every run, so a stale extraction cannot select another version.
rm -rf "$TOOLS/squashfs-root"
(cd "$TOOLS" && ./appimagetool.AppImage --appimage-extract > extract.log)

if [ -d "$APPDIR" ]; then
    chmod -R u+w "$APPDIR"
    rm -rf "$APPDIR"
fi
mkdir -p "$APPDIR"
cp -a "$STAGE/." "$APPDIR/"
cp "$ROOT/tools/linux-desktop/alfabet-smierci.desktop" "$APPDIR/"
cp "$ROOT/tools/linux-desktop/alfabet-smierci.svg" "$APPDIR/"
ln -s alfabet-smierci.svg "$APPDIR/.DirIcon"
cp "$TOOLS/runtime-LICENSE" "$APPDIR/APPIMAGE_RUNTIME_LICENSE.txt"
mkdir -p "$APPDIR/licenses/appimage-runtime"
for notice in musl-COPYRIGHT libfuse-LGPL-2.0.txt squashfuse-BSD.txt zstd-BSD.txt zlib-LICENSE.txt; do
    cp "$TOOLS/$notice" "$APPDIR/licenses/appimage-runtime/"
done
cat > "$APPDIR/APPIMAGE_RUNTIME_SOURCE.txt" <<'NOTICE'
AppImage type-2 runtime 20251108, upstream binary SHA-256:
2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d

Runtime source, build instructions and third-party library sources:
https://github.com/AppImage/type2-runtime/tree/20251108
https://github.com/AppImage/type2-runtime/blob/20251108/BUILD.md
https://github.com/AppImage/type2-runtime/blob/20251108/scripts/common/install-dependencies.sh
https://github.com/AppImage/type2-runtime/blob/20251108/scripts/chroot/build.sh

This runtime is unmodified. Its license notice lists the statically linked
runtime libraries and their licenses. The game executable and original disks
are separate files in the SquashFS payload; they have their own notices.
To use a rebuilt runtime, extract this AppImage with --appimage-extract and
run appimagetool --runtime-file YOUR_RUNTIME squashfs-root OUTPUT.AppImage.
NOTICE
cat > "$APPDIR/AppRun" <<'LAUNCHER'
#!/bin/sh
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec "$DIR/AlfabetSmierci" "$@"
LAUNCHER
chmod 755 "$APPDIR/AppRun"
SOURCE_DATE_EPOCH=${SOURCE_DATE_EPOCH:-$(git -C "$ROOT" log -1 --format=%ct)}
export SOURCE_DATE_EPOCH
ARCH=x86_64 VERSION="$VERSION" "$TOOLS/squashfs-root/AppRun" \
    --runtime-file "$TOOLS/runtime-x86_64" --no-appstream --comp zstd \
    --mksquashfs-opt=-processors --mksquashfs-opt="${JOBS:-4}" \
    "$APPDIR" "$OUTPUT"
chmod 755 "$OUTPUT"
(cd "$ROOT/dist/linux" && sha256sum "$NAME.AppImage" > "$NAME.AppImage.sha256")
printf 'Packaged %s\n' "$OUTPUT"
