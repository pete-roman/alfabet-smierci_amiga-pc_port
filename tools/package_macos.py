#!/usr/bin/env python3
"""Build and package a self-contained, ad hoc signed macOS application."""

from __future__ import annotations

import hashlib
import os
import pathlib
import plistlib
import re
import shutil
import subprocess
import sys


ROOT = pathlib.Path(__file__).resolve().parent.parent
BUILD = ROOT / "build" / "native"
DIST = ROOT / "dist" / "macos"
APP_NAME = "AlfabetSmierci.app"
SYSTEM_PREFIXES = ("/System/Library/", "/usr/lib/")


def run(*args: os.PathLike[str] | str, capture: bool = False) -> str:
    command = [os.fspath(arg) for arg in args]
    result = subprocess.run(command, check=True, text=True,
                            capture_output=capture)
    return result.stdout if capture else ""


def output(*args: os.PathLike[str] | str) -> str:
    return run(*args, capture=True)


def version() -> str:
    text = (ROOT / "src" / "version.h").read_text(encoding="utf-8")
    match = re.search(r'^#define PORT_VERSION "([^"]+)"', text, re.M)
    if not match:
        raise RuntimeError("PORT_VERSION was not found")
    return match.group(1)


def dependencies(binary: pathlib.Path) -> list[str]:
    lines = output("otool", "-L", binary).splitlines()[1:]
    return [line.strip().split(" (", 1)[0] for line in lines if line.strip()]


def dylib_id(binary: pathlib.Path) -> str | None:
    lines = output("otool", "-D", binary).splitlines()[1:]
    return lines[0].strip() if lines else None


def is_system(path: str) -> bool:
    return path.startswith(SYSTEM_PREFIXES)


def external_dependencies(binary: pathlib.Path) -> list[pathlib.Path]:
    own_id = dylib_id(binary)
    result = []
    for dependency in dependencies(binary):
        if dependency == own_id or dependency.startswith("@") or is_system(dependency):
            continue
        if not dependency.startswith("/"):
            raise RuntimeError(f"unsupported dependency in {binary}: {dependency}")
        path = pathlib.Path(dependency)
        if not path.is_file():
            raise RuntimeError(f"missing dependency of {binary}: {dependency}")
        result.append(path)
    return result


def dependency_closure(executable: pathlib.Path) -> dict[str, pathlib.Path]:
    pending = external_dependencies(executable)
    found: dict[str, pathlib.Path] = {}
    while pending:
        path = pending.pop().resolve()
        name = path.name
        previous = found.get(name)
        if previous:
            if previous != path:
                raise RuntimeError(f"two dylibs have the same name: {previous} and {path}")
            continue
        found[name] = path
        pending.extend(external_dependencies(path))
    return found


def rewrite_dependencies(binary: pathlib.Path, original: pathlib.Path,
                         destination: str) -> None:
    own_id = dylib_id(original)
    for dependency in dependencies(original):
        if dependency == own_id or dependency.startswith("@") or is_system(dependency):
            continue
        run("install_name_tool", "-change", dependency,
            f"{destination}/{pathlib.Path(dependency).resolve().name}", binary)


def architectures(binary: pathlib.Path) -> tuple[str, ...]:
    return tuple(output("lipo", "-archs", binary).strip().split())


def minimum_macos(binary: pathlib.Path) -> tuple[int, ...]:
    text = output("vtool", "-show-build", binary)
    match = re.search(r"^\s*minos\s+([0-9.]+)$", text, re.M)
    if not match:
        raise RuntimeError(f"cannot determine the minimum macOS version of {binary}")
    return tuple(int(part) for part in match.group(1).split("."))


def version_text(parts: tuple[int, ...]) -> str:
    return ".".join(str(part) for part in parts)


def copy_formula_license(formula: str, relative: str,
                         destination: pathlib.Path) -> None:
    prefix = pathlib.Path(output("brew", "--prefix", formula).strip()).resolve()
    source = prefix / relative
    if not source.is_file():
        raise RuntimeError(f"license file not found: {source}")
    shutil.copy2(source, destination)


def write_licenses(resources: pathlib.Path) -> None:
    licenses = resources / "licenses"
    licenses.mkdir()
    copy_formula_license("sdl3", "LICENSE.txt", licenses / "SDL3-zlib.txt")
    copy_formula_license("libopenmpt", "LICENSE",
                         licenses / "libopenmpt-BSD-3-Clause.txt")
    copy_formula_license("mpg123", "COPYING", licenses / "mpg123-LGPL-2.1.txt")
    copy_formula_license("libogg", "COPYING", licenses / "libogg-BSD-3-Clause.txt")
    copy_formula_license("libvorbis", "COPYING",
                         licenses / "libvorbis-BSD-3-Clause.txt")

    notices = (ROOT / "AlfabetSmierci" / "THIRD_PARTY_LICENSES.txt").read_text(
        encoding="utf-8")
    start = notices.index("-" * 72 + "\nminiz")
    end = notices.index("-" * 72 + "\nMinGW-w64")
    (licenses / "libopenmpt-bundled-and-ProWizard.txt").write_text(
        notices[start:end].rstrip() + "\n", encoding="utf-8")

    sdl_version = output("pkg-config", "--modversion", "sdl3").strip()
    openmpt_version = output("pkg-config", "--modversion", "libopenmpt").strip()
    (resources / "THIRD_PARTY_LICENSES.txt").write_text(
        f"""Third-party code included in the macOS application
====================================================

SDL3 {sdl_version} is included as libSDL3.0.dylib. Its zlib license is in
licenses/SDL3-zlib.txt.

libopenmpt {openmpt_version} is included as libopenmpt.0.dylib. Its BSD-3-Clause
license is in licenses/libopenmpt-BSD-3-Clause.txt. Notices for its bundled
miniz, minimp3 and stb_vorbis code are in
licenses/libopenmpt-bundled-and-ProWizard.txt.

libopenmpt dynamically uses the included mpg123, libogg, libvorbis and
libvorbisfile libraries. Their license texts are in licenses/.

The port includes a ProWizard-derived converter for The Player 6.0A modules.
Its MIT license is also in licenses/libopenmpt-bundled-and-ProWizard.txt.

Apple frameworks and libraries referenced under /System/Library and /usr/lib
are supplied by macOS and are not included in this application.
""", encoding="utf-8")


def validate_dependencies(binaries: list[pathlib.Path]) -> None:
    forbidden = ("/opt/homebrew", "/usr/local", "/Cellar/", "/Users/",
                 os.fspath(ROOT), "/deps/", "/build/")
    for binary in binaries:
        for dependency in dependencies(binary):
            if dependency.startswith("@") or is_system(dependency):
                continue
            raise RuntimeError(f"non-portable dependency in {binary}: {dependency}")
        listing = "\n".join(dependencies(binary))
        for fragment in forbidden:
            if fragment in listing:
                raise RuntimeError(f"forbidden path {fragment!r} in {binary}")


def sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> None:
    if sys.platform != "darwin":
        raise SystemExit("This packager must run on macOS.")
    for tool in ("brew", "cmake", "otool", "vtool", "lipo",
                 "install_name_tool", "codesign", "ditto"):
        if not shutil.which(tool):
            raise RuntimeError(f"required tool not found: {tool}")

    run("cmake", "-S", ROOT, "-B", BUILD, "-DCMAKE_BUILD_TYPE=Release")
    run("cmake", "--build", BUILD, "--parallel")

    source_app = BUILD / APP_NAME
    if not source_app.is_dir():
        raise RuntimeError(f"application was not built: {source_app}")
    release = version()
    executable_relative = pathlib.Path("Contents/MacOS/AlfabetSmierci")
    source_executable = source_app / executable_relative
    closure = dependency_closure(source_executable)

    DIST.mkdir(parents=True, exist_ok=True)
    stage = DIST / APP_NAME
    if stage.exists():
        shutil.rmtree(stage)
    shutil.copytree(source_app, stage, symlinks=True)
    executable = stage / executable_relative
    frameworks = stage / "Contents" / "Frameworks"
    resources = stage / "Contents" / "Resources"
    frameworks.mkdir()

    staged_libraries: list[tuple[pathlib.Path, pathlib.Path]] = []
    for name, source in sorted(closure.items()):
        destination = frameworks / name
        shutil.copy2(source, destination, follow_symlinks=True)
        run("chmod", "u+w", destination)
        run("codesign", "--remove-signature", destination)
        staged_libraries.append((destination, source))

    run("codesign", "--remove-signature", executable)
    rewrite_dependencies(executable, source_executable,
                         "@executable_path/../Frameworks")
    for destination, source in staged_libraries:
        rewrite_dependencies(destination, source, "@loader_path")
        run("install_name_tool", "-id", f"@rpath/{destination.name}", destination)

    binaries = [executable] + [item[0] for item in staged_libraries]
    expected_arches = architectures(executable)
    for binary in binaries[1:]:
        actual = architectures(binary)
        if actual != expected_arches:
            raise RuntimeError(
                f"architecture mismatch: {binary} has {actual}, expected {expected_arches}")
    minimum = max(minimum_macos(binary) for binary in binaries)
    minimum_text = version_text(minimum)

    plist_path = stage / "Contents" / "Info.plist"
    with plist_path.open("rb") as source:
        plist = plistlib.load(source)
    plist.update({
        "CFBundleDisplayName": "Alfabet Śmierci",
        "CFBundleGetInfoString": f"Alfabet Śmierci, port {release}",
        "LSMinimumSystemVersion": minimum_text,
        "NSHighResolutionCapable": True,
        "NSHumanReadableCopyright": "© 1995/1996 Arte / Seven Stars Multimedia; port © 2026 Piotr 'Pete' Romańczyk",
    })
    with plist_path.open("wb") as destination:
        plistlib.dump(plist, destination, sort_keys=True)

    shutil.copy2(ROOT / "LICENSE", resources / "LICENSE.txt")
    shutil.copy2(ROOT / "CREDITS.md", resources / "CREDITS.md")
    readme = (ROOT / "tools" / "macos-package-README.txt").read_text(encoding="utf-8")
    readme = readme.replace("@ARCHITECTURES@", " ".join(expected_arches))
    readme = readme.replace("@MINIMUM_MACOS@", minimum_text)
    (resources / "CZYTAJ.txt").write_text(readme, encoding="utf-8")
    write_licenses(resources)
    commit = output("git", "-C", ROOT, "rev-parse", "HEAD").strip()
    (resources / "BUILD-INFO.txt").write_text(
        f"""Alfabet Śmierci macOS package
Release: {release}
Source commit: {commit}
Architecture: {' '.join(expected_arches)}
Minimum macOS: {minimum_text}
SDL3: {output('pkg-config', '--modversion', 'sdl3').strip()}
libopenmpt: {output('pkg-config', '--modversion', 'libopenmpt').strip()}
Signature: ad hoc, no Developer ID, not notarized
""", encoding="utf-8")

    validate_dependencies(binaries)
    for binary, _source in reversed(staged_libraries):
        run("codesign", "--force", "--sign", "-", "--timestamp=none", binary)
    run("codesign", "--force", "--sign", "-", "--timestamp=none", stage)
    run("codesign", "--verify", "--deep", "--strict", "--verbose=2", stage)

    arch_label = ("universal2" if set(expected_arches) == {"x86_64", "arm64"}
                  and len(expected_arches) == 2 else "-".join(expected_arches))
    package_name = f"AlfabetSmierci-{release}-macos-{arch_label}"
    archive = DIST / f"{package_name}.zip"
    checksum = DIST / f"{archive.name}.sha256"
    archive.unlink(missing_ok=True)
    checksum.unlink(missing_ok=True)
    run("ditto", "-c", "-k", "--sequesterRsrc", "--keepParent", stage, archive)
    digest = sha256(archive)
    checksum.write_text(f"{digest}  {archive.name}\n", encoding="ascii")
    print(f"Packaged {archive}")
    print(f"SHA-256 {digest}")
    print(f"Architecture: {' '.join(expected_arches)}")
    print(f"Minimum macOS: {minimum_text}")


if __name__ == "__main__":
    main()
