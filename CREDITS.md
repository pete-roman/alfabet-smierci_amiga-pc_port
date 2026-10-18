# Credits

## 1. Alfabet Śmierci (original game, Amiga AGA, 1996)

Everything in this port, from the photographs and the music to the story, the dialogue and the design, is the work of the original team. The port only re-hosts it.

Published by **Arte**, produced by **Seven Stars Multimedia** (C) 1995/1996.

| Role (as credited in the intro) | Person |
|---|---|
| Programowanie (code) | Maciej 'Maniak' Frontkiewicz |
| Grafika (graphics) | Ryszard 'Justus' Bagiński |
| Muzyka i scenariusz (music and story) | Michał 'Dixan05' Prochownik |

The in-game credits screen reproduces the original credits text verbatim. Source: main game binary, text at `$7F02F`..`$7FA90`. That text includes the thanks to the management of the district hospital in Żywiec (Pan Czesław Kapała and the surgery ward staff), Klub "Śrubka", EL DORADO, AB..Z, the people thanked by name, the residents of Żywiec, Jeleśnia and the surrounding area, and each author's personal thanks and greetings.

**Port by Pete Roman, for Seven Stars Multimedia and Maciej 'Maniak' Frontkiewicz, with love.**

The last page of the in-game credits is the port author's card, in the style of the original authors' personal pages: Piotr 'Pete' Romańczyk, Autor portu z systemów Amiga na PC, with his personal thanks to the authors, the Seven Stars and Arte team, and the people of Żywiec and Jeleśnia.

## 2. Tools and libraries

This list is kept up to date from day one. "Ships" means the item is part of the portable build.

| Tool / library | Author(s) / team | Licence | Used for | Ships |
|---|---|---|---|---|
| Python 3.14 | Python Software Foundation | PSF-2.0 | All tooling (decruncher, extractors, lifter) | no |
| amitools 0.8.1 | Christian Vogelgsang | GPL-2.0-or-later | `xdftool` ADF inspection | no |
| Capstone 5.0.9 | Nguyen Anh Quynh and contributors | BSD-3-Clause | 68k disassembly for reverse engineering | no |
| Pillow 12.3.0 | Jeffrey 'Alex' Clark and contributors (fork of PIL by Fredrik Lundh) | MIT-CMU | PNG export, mockups | no |
| NumPy 2.5 | NumPy Developers | BSD-3-Clause | Palette matching, pixel comparison with the original (`tools/compare_ref.py`) | no |
| ancient | Teemu Suutari (temisu) | BSD-2-Clause | Reference for the CrunchMania format (`tools/crunchmania.py` is an independent Python reimplementation) | no |
| CrunchMania | Thomas Schwarz | freeware (Amiga) | The packer the original game used; not run by us | no |
| ProWizard "The Player" depacker | Sylvain 'Asle' Chipaux; libxmp port by Claudio Matsuoka and Alice Rowan | MIT | `tools/p60a.py` and `src/audio.c` are ports of it (P60A to MOD) | yes (C port) |
| libxmp | Claudio Matsuoka, Hipolito Carraro Jr and contributors | MIT | Format reference (The Player 6.0A support) | no |
| The Player 6.0A | Jarno Paananen ('Guru' / Sahara Surfers) | freeware (Amiga) | Music replayer used by the original game; its module format is what we convert | no |
| libopenmpt 0.8.9 / openmpt123 | OpenMPT Project Developers and contributors | BSD-3-Clause | Music engine of the port (A1200 Paula emulation); openmpt123 for verifying the converted modules | yes (libopenmpt) |
| WinUAE 6.0.3 | Toni Wilen and contributors, based on UAE by Bernd Schmidt | GPL-2.0 | Reference emulation of the original (palettes, layout, timing), driven over its IPC pipe | no |
| AROS Kickstart replacement ROM (built into WinUAE) | The AROS Development Team | AROS Public License 1.1 | Booting the original in WinUAE without a Commodore ROM | no |
| Git | Linus Torvalds, Junio C Hamano and contributors | GPL-2.0 | Version control | no |
| Windows PowerShell | Microsoft | Windows component | Driving WinUAE over IPC and window captures (`tools/uaeipc.ps1`, `tools/winshot.ps1`, `tools/uaepark.ps1`) | no |
| CMake | Kitware and contributors | BSD-3-Clause | Native Linux and macOS builds | no |
| pkg-config (pkgconf) | pkgconf project contributors | ISC | Locating system SDL3 and libopenmpt for native builds | no |
| Homebrew | Homebrew contributors | BSD-2-Clause | macOS build dependencies | no |
| Apple Clang and macOS command-line tools | Apple | Apple developer tools licence | macOS compilation, Mach-O rewriting, ad hoc signing and zip creation | no |
| Docker / Moby | Docker and Moby contributors | Apache-2.0 (engine and CLI) | Isolated Ubuntu 22.04 Linux package builds | no |
| appimagetool 1.9.1 | Simon Peter and the AppImage Team | MIT (tool source; dependencies keep their licences) | Creating the Linux AppImage | no |
| AppImage type-2 runtime 20251108 | Simon Peter and the AppImage contributors | MIT; bundled musl, libfuse, squashfuse, zstd and zlib have their own licences | Starting or extracting the single-file Linux package | yes (AppImage only; notices included) |
| Codex and GPT-6 Luna | OpenAI | commercial | Linux/macOS build support, Linux packaging and verification | no |
| Claude Opus 5.5 | Anthropic | commercial | AI model that did the reverse engineering, the lifter, the engine and the docs with Pete | no |
| Claude Code | Anthropic | commercial | The agentic coding tool the port was built in | no |

Access to the Anthropic tools was possible thanks to the cooperation with ReadyM (ReadyCode Limited).

### Engine and build

| Library | Author(s) / team | Licence | Ships |
|---|---|---|---|
| SDL3 3.4.18 | Sam Lantinga and SDL contributors | zlib | yes |
| miniz, minimp3, stb_vorbis (built into libopenmpt; the game's music does not use them) | Rich Geldreich, RAD Game Tools, Valve Software; Lion (lieff); Sean Barrett | MIT; CC0 1.0; MIT or public domain | yes (inside libopenmpt) |
| mpg123 1.33.7 | mpg123 project contributors | LGPL-2.1-only | yes (macOS package, libopenmpt dependency) |
| libogg 1.3.6 | Xiph.Org Foundation | BSD-3-Clause | yes (macOS package, libopenmpt dependency) |
| libvorbis and libvorbisfile 1.3.7 | Xiph.Org Foundation | BSD-3-Clause | yes (macOS package, libopenmpt dependency) |
| MinGW-w64 runtime, GCC runtime libraries | mingw-w64 project; GNU Project | ZPL / public domain; GPL-3.0 with GCC Runtime Library Exception | yes (statically linked) |
| w64devkit (build toolchain: GCC, binutils, mingw-w64) | Christopher Wellons; GCC by the GNU Project; mingw-w64 project | Unlicense (kit); GPL-3.0 with runtime exception (GCC); mingw-w64 runtime: ZPL / public domain | no (toolchain only) |
| CMake | Kitware and contributors | BSD-3-Clause | no (toolchain only) |
