Alfabet Śmierci, Linux package
==============================

This package contains the original game disks and the Linux port engine.
The game and its data are in Polish.

Run `./uruchom.sh`. The launcher works from any current directory, including
directories whose names contain spaces. The original disks and scripts in
`data/` are read-only. Game saves go to the user's SDL application data folder,
normally `$XDG_DATA_HOME/Pete Roman/Alfabet Smierci/` or
`~/.local/share/Pete Roman/Alfabet Smierci/`.

The matching `.AppImage` is a separate single-file distribution. If FUSE is
unavailable, make it executable and use its extraction fallback:
`chmod +x AlfabetSmierci-1.0-linux-x86_64.AppImage`, then
`./AlfabetSmierci-1.0-linux-x86_64.AppImage --appimage-extract-and-run`.

Sterowanie: lewy przycisk myszy wybiera, przechodzi i przewija dialogi. Prawy
przycisk i spacja także przewijają kwestie. F11 lub Alt+Enter włącza pełny
ekran, Tab pokazuje panele boczne. Szybki zapis jest w panelu; zapis z menu gry
tworzy emulowaną dyskietkę w tym samym katalogu danych użytkownika.

Compatibility: x86_64 Linux with glibc 2.35 or newer, corresponding to Ubuntu
22.04 or newer. SDL3 and libopenmpt are statically linked. The SDL3 Linux
display and audio backends still use operating-system libraries. A normal
desktop installation should provide an X11 or Wayland session, ALSA or
PulseAudio, and the matching X11/Wayland client libraries.

The `AlfabetSmierci` binary is built on Ubuntu 22.04. The package includes
`BUILD-INFO.txt`, `SHA256SUMS`, the MIT port license, credits and third-party
license notices. Check the files with `sha256sum -c SHA256SUMS` before running.

Controls, story and credit information are in the project README and in the
game's credits screens.
