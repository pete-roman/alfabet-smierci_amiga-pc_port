"""Resolve the game executable used by the automation scripts.

Set ALF_EXE to select an executable explicitly. Otherwise use the native CMake
build when present, with the committed Windows build as a fallback.
"""
import os
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent


def executable():
    override = os.environ.get("ALF_EXE")
    if override:
        return pathlib.Path(override).expanduser().resolve()
    if sys.platform == "darwin":
        native = ROOT / "build" / "native" / "AlfabetSmierci.app" / "Contents" / "MacOS" / "AlfabetSmierci"
    else:
        native = ROOT / "build" / "native" / "AlfabetSmierci" / "AlfabetSmierci"
    if native.is_file():
        return native
    return ROOT / "AlfabetSmierci" / "AlfabetSmierci.exe"
