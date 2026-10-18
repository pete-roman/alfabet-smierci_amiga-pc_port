# Alfabet Śmierci: Amiga to PC port

**[Polski](#polski)** | **[English](#english)**

![Alfabet Śmierci, ekran tytułowy / title screen](docs/screens/title.png)

Port by Pete Roman, for Seven Stars Multimedia and Maciej 'Maniak' Frontkiewicz, with love.

---

## Polski

Polska przygodówka point-and-click z 1996 roku (Arte / Seven Stars Multimedia, Amiga AGA), przeniesiona na komputery PC. Port uruchamia **oryginalne dyskietki gry**: zdjęcia, palety, muzyka, teksty i logika są te same co na Amidze 1200. Nowy jest tylko silnik, który je czyta.

### Jak zagrać

1. Pobierz **`AlfabetSmierci-1.0.zip`** z zakładki [Releases](../../releases) albo całe repo: **Code**, potem **Download ZIP**.
2. Rozpakuj i wejdź do folderu **`AlfabetSmierci`**.
3. Uruchom **`AlfabetSmierci.exe`**.

Windows 10/11, 64 bit. Bez instalacji i bez uprawnień administratora. Folder `AlfabetSmierci` to kompletna gra: exe, oryginalne dyskietki (`data/disk1..4.adf`), logika gry (`data/scripts`), instrukcja `CZYTAJ.txt` i licencje bibliotek.

**Sterowanie:** wszystko lewym przyciskiem myszy (wybór, przejście, dialog, kolejna kwestia). Prawy przycisk i spacja przewijają kwestie jak w oryginale. FIRE joysticka, którego gra w kilku miejscach wymaga: lewy klik w puste miejsce obrazka, spacja, środkowy przycisk albo pad. F11 lub Alt+Enter: pełny ekran. Tab: panele boczne.

![Dworzec](docs/screens/dworzec.png)
![Dialog w pociągu](docs/screens/pociag.png)

### Co jest w środku

- Obraz jak z układów AGA: bitplany, copper, sprite'y, blitter. Bez rozciągania i wygładzania, tylko całkowite powiększenie pikseli; piksel hires ma proporcje 1:2 jak na monitorze Amigi.
- Muzyka grana na żywo z oryginalnych modułów (The Player 6.0A, przełożone bezstratnie na ProTracker) przez libopenmpt z emulacją Pauli A1200. Żadnych nagranych wcześniej plików audio.
- Logika gry przełożona instrukcja po instrukcji z kodu 68000 na czytelne skrypty (`scripts/*.alf`, opis języka w `scripts/README.md`).
- Panele boczne na szerokim ekranie: opcje, szybki zapis, autorzy, narzędzia. Zapisy z menu gry działają na emulowanej dyskietce zapisu (`saves/stan_gry.adf`).

**Sprawdzone:** cała gra przechodzi automatem od dworca do zakończenia (263 kroki, także samym lewym przyciskiem), wszystkie lokacje przeszły testy, wybrane ekrany są identyczne bajt w bajt z oryginałem uruchomionym w WinUAE, tempo intro i 50 Hz zgodne z oryginałem.

### Dla programistów

| plik | co to jest |
|---|---|
| `ENGINE.md` | jak działa oryginał i jak działa port |
| `RECON.md` | format dyskietek, loader, CrunchMania, mapa pamięci |
| `STACK.md` | wybór technologii (C11, SDL3, libopenmpt) |
| `PROGRESS.md` | stan prac, tabela lokacji, komendy build/test |
| `CREDITS.md` | autorzy gry, port, narzędzia i licencje |
| `docs/WALKTHROUGH.md` | solucja wyprowadzona ze skryptów (spoilery!) |
| `src/` | silnik w C |
| `scripts/` | logika gry (generowana przez `tools/lifter.py` z dyskietki 1) |
| `tools/` | dekompresja, lifter, testy, porównania z WinUAE |
| `AlfabetSmierci/` | gotowa do gry wersja (wynik `build.sh`) |

Budowanie (Windows, Git Bash; wszystko lokalnie, nic systemowo):

    sh tools/fetch_deps.sh
    tools/toolchain/w64devkit/bin/sh tools/build_deps.sh
    tools/toolchain/w64devkit/bin/sh build.sh

Testy: `py tools/test_locations.py`, `py tools/playthrough.py` (szczegóły w `PROGRESS.md`).

#### Linux (pakiet do uruchomienia)

Gotową paczkę x86-64 z SDL3 i libopenmpt zbudowanymi statycznie przygotujesz w Dockerze. Wymaga zainstalowanego Dockera, ale nie zmienia pakietów systemowych:

    sh tools/build_linux_package.sh

Wynikiem jest archiwum `dist/linux/AlfabetSmierci-1.0-linux-x86_64.tar.gz`; uruchom z niego `uruchom.sh`. Opcjonalny pojedynczy plik AppImage przygotujesz przez `sh tools/make_linux_appimage.sh`. Przy systemie bez FUSE: `chmod +x AlfabetSmierci-1.0-linux-x86_64.AppImage`, potem `./AlfabetSmierci-1.0-linux-x86_64.AppImage --appimage-extract-and-run`. Paczki są budowane na Ubuntu 22.04 (glibc 2.35; binarnie wymaga do GLIBC 2.34) i przeznaczone dla x86-64. SDL3 i libopenmpt są statyczne; system dostarcza biblioteki sterowników okna i dźwięku (X11/Wayland oraz ALSA/PulseAudio). Zapisy trafiają do katalogu użytkownika SDL/XDG, a dane w paczce są tylko do odczytu. Wewnątrz są licencje dołączonych bibliotek oraz sumy SHA-256. Testy: `ALF_EXE=/pełna/ścieżka/AlfabetSmierci python3 tools/test_linux_saves.py`.

**Sterowanie:** lewy przycisk myszy wybiera i przechodzi, prawy przycisk lub spacja przewijają kwestie; FIRE działa spacją, środkowym przyciskiem lub padem. F11 albo Alt+Enter przełącza pełny ekran, Tab otwiera panele.

#### macOS (samodzielna aplikacja)

Na macOS z Homebrew przygotujesz wersjonowane archiwum `.app` z dołączonymi
bibliotekami, podpisem ad hoc i sumą SHA-256:

    brew install cmake pkgconf sdl3 libopenmpt
    python3 tools/package_macos.py

Wyniki trafiają do `dist/macos/`. Skrypt buduje aplikację, kopiuje pełny zestaw
niestandardowych bibliotek do `Contents/Frameworks`, zmienia ich odwołania na
ścieżki względne, podpisuje kod od środka na zewnątrz i sprawdza podpis oraz
zależności. Paczka zbudowana i sprawdzona lokalnie ma architekturę arm64 i wymaga
macOS 26.0, ponieważ taki minimalny system mają użyte binaria Homebrew. Nie jest to
Universal 2. Podpis ad hoc nie jest podpisem Developer ID, a paczka nie jest
notaryzowana przez Apple.

#### Linux i macOS (build deweloperski)

Natywny build deweloperski korzysta z CMake, C11, SDL3 i libopenmpt zainstalowanych w systemie. Na Ubuntu 26.04:

    sudo apt install build-essential cmake pkg-config libsdl3-dev libopenmpt-dev
    cmake -S . -B build/native -DCMAKE_BUILD_TYPE=Release
    cmake --build build/native --parallel

Na macOS z Homebrew:

    brew install cmake pkgconf sdl3 libopenmpt
    cmake -S . -B build/native -DCMAKE_BUILD_TYPE=Release
    cmake --build build/native --parallel

Na Linuxie gra znajduje się w `build/native/AlfabetSmierci/AlfabetSmierci`, a dane gry obok niej. Na macOS CMake tworzy `build/native/AlfabetSmierci.app`; dane trafiają do `Contents/Resources/data`. Te buildy wymagają systemowych bibliotek SDL3 i libopenmpt. Zapisy natywne trafiają do katalogu danych użytkownika, a obrazy dyskietek są tylko do odczytu. Testy wybierają natywny build automatycznie; `ALF_EXE=/ścieżka/do/programu` pozwala wskazać go ręcznie. GitHub Actions buduje gotowe paczki Linux i macOS.

### Prawa

Gra Alfabet Śmierci: (C) 1995/1996 Arte / Seven Stars Multimedia; programowanie Maciej 'Maniak' Frontkiewicz, grafika Ryszard 'Justus' Bagiński, muzyka i scenariusz Michał 'Dixan05' Prochownik. Gra jest dziś abandonware; obrazy dyskietek są dołączone w niezmienionej postaci. Kod portu (silnik, narzędzia, dokumentacja): licencja MIT, patrz [`LICENSE`](LICENSE). Biblioteki i ich licencje: [`CREDITS.md`](CREDITS.md) i `AlfabetSmierci/THIRD_PARTY_LICENSES.txt`.

---

## English

A Polish point-and-click adventure from 1996 (Arte / Seven Stars Multimedia, Amiga AGA), brought to PC. The port runs the **original game disks**: the photographs, palettes, music, texts and game logic are the same as on the Amiga 1200. Only the engine that reads them is new. The game itself is in Polish.

### How to play

1. Download **`AlfabetSmierci-1.0.zip`** from [Releases](../../releases), or the whole repository: **Code**, then **Download ZIP**.
2. Unzip and open the **`AlfabetSmierci`** folder.
3. Run **`AlfabetSmierci.exe`**.

Windows 10/11, 64-bit. No installer, no administrator rights. The `AlfabetSmierci` folder is the complete game: the exe, the original disk images (`data/disk1..4.adf`), the game logic (`data/scripts`), a Polish readme (`CZYTAJ.txt`) and the library licences.

**Controls:** everything with the left mouse button (select, walk, pick a dialogue line, next line). The right button and space also move speech on, as in the original. Joystick FIRE, which the game asks for in a few places: left click on an empty spot of the picture, space, the middle button or a gamepad. F11 or Alt+Enter: fullscreen. Tab: side panels.

### What is inside

- AGA-accurate picture: bitplanes, copper, sprites, blitter. No stretching or filtering, integer scaling only; hires pixels keep the Amiga's 1:2 aspect.
- Music played live from the original modules (The Player 6.0A, converted losslessly to ProTracker) through libopenmpt with A1200 Paula emulation. No pre-rendered audio.
- The game logic translated instruction by instruction from the 68000 code into readable scripts (`scripts/*.alf`, language described in `scripts/README.md`).
- Side panels on wide screens: options, quick save, credits, tools. The game's own save menu works on an emulated save disk (`saves/stan_gry.adf`).

**Verified:** the whole game is played through by a bot from the first location to the ending (263 steps, also with the left button only), every location passes the tests, selected screens are byte-identical to the original running in WinUAE, and the intro timing and the 50 Hz pacing match the original.

### For developers

Engine in C11 with SDL3 and libopenmpt, built with a portable MinGW-w64 toolchain (see `STACK.md`). Docs (mostly English): `ENGINE.md` (how the original and the port work), `RECON.md` (disk format, loader, CrunchMania, memory map), `CREDITS.md`. Working notes `PROGRESS.md` and `CLAUDE.md` are in Polish. Build:

    sh tools/fetch_deps.sh
    tools/toolchain/w64devkit/bin/sh tools/build_deps.sh
    tools/toolchain/w64devkit/bin/sh build.sh

For a ready-to-run Linux x86-64 package, build inside Docker (no host packages are installed):

    sh tools/build_linux_package.sh

This creates `dist/linux/AlfabetSmierci-1.0-linux-x86_64.tar.gz`; extract it and run `uruchom.sh`. For a single-file AppImage, first build the tar package and then run `sh tools/make_linux_appimage.sh`. If FUSE is unavailable, use `chmod +x AlfabetSmierci-1.0-linux-x86_64.AppImage` and `./AlfabetSmierci-1.0-linux-x86_64.AppImage --appimage-extract-and-run`. Packages target x86-64 and are built on Ubuntu 22.04 (glibc 2.35; the game binary requires up to GLIBC 2.34). SDL3 and libopenmpt are statically linked; the host still needs SDL's X11/Wayland display and ALSA/PulseAudio backend libraries. Saves go to the SDL/XDG per-user directory; bundled game data is read-only. Third-party licenses and SHA-256 manifests are included.

For a self-contained macOS application, install `cmake pkgconf sdl3 libopenmpt`
with Homebrew and run `python3 tools/package_macos.py`. The script copies the full
non-system library closure into the app, rewrites Mach-O paths, signs all nested code
and the app ad hoc, verifies them, and writes a versioned zip plus SHA-256 manifest to
`dist/macos/`. The locally verified package is arm64 and requires macOS 26.0 because
that is the deployment target of the installed Homebrew bottles. It is not Universal
2, Developer ID signed, or notarized.

For native developer builds, use system CMake, SDL3 and libopenmpt. On Ubuntu 26.04 install `build-essential cmake pkg-config libsdl3-dev libopenmpt-dev`; on macOS install `cmake pkgconf sdl3 libopenmpt` with Homebrew. Then run:

    cmake -S . -B build/native -DCMAKE_BUILD_TYPE=Release
    cmake --build build/native --parallel

Linux output is `build/native/AlfabetSmierci/AlfabetSmierci`. macOS output is `build/native/AlfabetSmierci.app`, with game data in its Resources directory. These developer builds require SDL3 and libopenmpt shared libraries from the system. Native saves use the per-user application data directory. Automation scripts discover this native build or accept an explicit path through `ALF_EXE`. GitHub Actions publishes the self-contained Linux and macOS packages.

### Rights

Alfabet Śmierci: (C) 1995/1996 Arte / Seven Stars Multimedia; code Maciej 'Maniak' Frontkiewicz, graphics Ryszard 'Justus' Bagiński, music and story Michał 'Dixan05' Prochownik. The game is abandonware today; the disk images are included unmodified. The port's code (engine, tools, documentation) is MIT licensed, see [`LICENSE`](LICENSE). Third-party libraries and their licences: [`CREDITS.md`](CREDITS.md) and `AlfabetSmierci/THIRD_PARTY_LICENSES.txt`.

The port was made with Claude (Anthropic); access to the tools was possible thanks to the cooperation with ReadyM (ReadyCode Limited).
