# Instrukcja dla Codexa na macOS

Status 2026-10-17: wykonane lokalnie na Apple Silicon. Powtarzalny skrypt
`tools/package_macos.py` tworzy samodzielną, podpisaną ad hoc paczkę i archiwum z
sumą SHA-256. Wyniki i ograniczenia są zapisane w `PROGRESS.md`; dalsza część tego
pliku pozostaje instrukcją odtworzenia i kontroli wydania.

Cel: zbudować i sprawdzić Alfabet Śmierci na Macu, a następnie przygotować samodzielną paczkę `.app` i archiwum lub DMG. Linux ma osobny proces pakowania. Nie zmieniaj logiki gry ani oryginalnych obrazów dyskietek.

## Polecenie do wklejenia do Codexa

> Pracujemy na forku `2fd5/alfabet-smierci_amiga-pc_port`, na gałęzi `feat/linux-macos-build`. Przeczytaj `CLAUDE.md`, ewentualne `AGENTS.md` i `CLAUDE.local.md`, `PROGRESS.md` oraz całą instrukcję `docs/MACOS_CODEX.md`. Dokończ natywną wersję macOS: build, automatyczne testy, samodzielny bundle z bibliotekami, podpis lokalny i paczka do pobrania. Zweryfikuj rzeczywistą architekturę i zależności. Kontynuuj do uzyskania sprawdzonej paczki; opisuj konkretnie ograniczenia podpisu i niepotwierdzone platformy. Zachowaj działanie Windows i Linuksa. Zrób lokalne commity; bez pusha, publikacji release ani PR-a. Po automatycznych testach podaj użytkownikowi krótką listę kontroli obrazu, muzyki i sterowania.

Gałąź musi być dostępna na tym Macu. Jeśli nie ma jej na `origin`, lokalne commity z Linuksa trzeba najpierw przenieść na fork lub dostarczyć jako git bundle. Nie zastępuj jej starszym `main` z upstreamu.

Do lokalnego przekazania bez pusha można użyć dołączonego `alfabet-smierci-linux-macos.bundle`. Na Macu, w nowym katalogu:

```sh
git clone -b feat/linux-macos-build /ścieżka/alfabet-smierci-linux-macos.bundle alfabet-smierci
cd alfabet-smierci
git remote set-url origin https://github.com/2fd5/alfabet-smierci_amiga-pc_port.git
git remote add upstream https://github.com/pete-roman/alfabet-smierci_amiga-pc_port.git
```

Bundle zawiera repozytorium i lokalną gałąź, więc nie wymaga wcześniejszego opublikowania zmian na GitHubie.

## Co już istnieje

- `CMakeLists.txt` buduje na macOS `build/native/AlfabetSmierci.app`.
- Program jest w `Contents/MacOS/AlfabetSmierci`, zasoby w `Contents/Resources/data`.
- Obrazy dyskietek pochodzą z `AlfabetSmierci/data/disk1.adf` do `disk4.adf`; aktualne skrypty są kopiowane z `scripts/`.
- `src/app_paths.c` używa katalogu użytkownika SDL dla zapisów. Nie zapisuj nic wewnątrz `.app`.
- `tools/engine_path.py` rozpoznaje build macOS; `ALF_EXE` pozwala testować kopię z finalnej paczki.
- CI macOS przygotowuje build deweloperski. Samo spakowanie tego katalogu nie dołącza bibliotek z Homebrew.
- Automaty na Linuksie przeszły intro, lokacje 04/05 i pełne 263 kroki wraz z powrotem do tytułu. To nie potwierdza działania na macOS.

## Sprawdzenie środowiska i build deweloperski

Zacznij od `git status`, `git branch --show-current`, `uname -m`, `sw_vers`, `xcode-select -p` oraz dostępności `clang`, `cmake`, `pkg-config`, `python3` i `brew`. Nie nadpisuj pracy użytkownika. Jeśli brakuje narzędzi Apple, opisz brak i zastosuj zasady projektu dotyczące instalowania SDK. Do tego projektu wystarczający zestaw trzeba ustalić na maszynie; nie instaluj automatycznie pełnego Xcode.

Przy działającym Homebrew i narzędziach kompilatora:

```sh
brew install cmake pkgconf sdl3 libopenmpt
cmake -S . -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native --parallel
open build/native/AlfabetSmierci.app
```

Te komendy budują dla architektury lokalnych bibliotek. Nie nazywaj wyniku Universal 2 bez sprawdzenia wszystkich binarek przez `lipo -info`. Na Apple Silicon najpierw ukończ i przetestuj `arm64`; wynik dla Intela wymaga odpowiednich bibliotek i osobnej weryfikacji. Samo `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"` nie zamienia bibliotek Homebrew w biblioteki uniwersalne. Minimalną wersję macOS dobierz do SDK i zależności, zapisz w ustawieniach builda i raporcie.

## Testy silnika

```sh
python3 tools/test_locations.py loc_04 loc_05
python3 tools/playthrough.py
```

Test pełnego przejścia musi zakończyć się krokiem 263, lokacją `3D` i powrotem do `intro.alf` po zakończeniu. Logi są w `work/play/` i `work/loctest/`. Przy uruchamianiu bez ekranu sprawdź, czy SDL obsługuje `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy`; nie traktuj błędu uruchomienia jako udanego testu. Test z dummy nie weryfikuje obrazu na ekranie ani odsłuchu muzyki.

Sprawdź osobno intro z `ALF_AUTOPLAY` bez `ALF_SKIP_INTRO` (przykład i warunek dojścia do `loc_04` są w `PROGRESS.md`). Sprawdź szybki zapis i wczytanie oraz trzy sloty menu gry. `ALF_SAVE` zmienia tylko ścieżkę szybkiego zapisu; testy zapisu oryginalnej dyskietki wymagają izolacji katalogu użytkownika. Nie nadpisuj prawdziwych zapisów gracza.

## Samodzielna aplikacja

1. Przygotuj osobny katalog roboczy w `dist/` lub `work/`; zostaw build deweloperski do porównań. Ustal jeden powtarzalny skrypt pakowania macOS w `tools/`.
2. Zbuduj zależności statycznie albo skopiuj wszystkie wymagane biblioteki niesystemowe do `Contents/Frameworks`. Przy bibliotekach dynamicznych przejdź zależności rekurencyjnie, także zależności libopenmpt. Można użyć `BundleUtilities` z CMake albo narzędzia do przenoszenia dylibów. Zweryfikuj aktualną dokumentację wybranej metody.
3. Ustaw odwołania Mach-O na ścieżki relatywne wewnątrz bundle (`@rpath`, `@loader_path`, `@executable_path` stosownie do położenia). `otool -L` głównej binarki i każdej dołączonej biblioteki nie może wskazywać `/opt/homebrew`, `/usr/local`, `deps/`, katalogu builda ani katalogu domowego. Biblioteki i frameworki Apple pozostają systemowe.
4. Sprawdź `Info.plist`, wersję z `src/version.h`, architekturę, minimalny system, nazwę aplikacji i katalog zasobów. Dodaj licencję portu, credits, instrukcję uruchamiania oraz licencje faktycznie dołączonego kodu i bibliotek. Generator licencji Windows nie opisuje automatycznie zależności macOS.
5. Podpisuj po wszystkich zmianach Mach-O i zasobów. Podpisz kod zagnieżdżony od środka na zewnątrz, na końcu bundle. Dla lokalnego testu można użyć podpisu ad hoc. `codesign --verify --deep --strict --verbose=2 <ścieżka.app>` ma przejść. Nie używaj `--deep` jako zamiennika prawidłowego podpisywania zależności.
6. Podpis ad hoc nie daje publicznej tożsamości Developer ID ani notaryzacji. Jeśli użytkownik ma skonfigurowany certyfikat i profil `notarytool`, zastosuj podpis Developer ID, hardened runtime, notaryzację i stapling zgodnie z aktualną dokumentacją Apple. Jeśli nie ma tych danych, dostarcz paczkę testową i jasno opisz jej status. Nie wyłączaj Gatekeepera i nie usuwaj zbiorczo atrybutów bezpieczeństwa z systemu.

Referencje: [SDL i ścieżki bundle](https://wiki.libsdl.org/SDL3/SDL_GetBasePath), [SDL i architektury Apple](https://wiki.libsdl.org/SDL3/README-cmake), [CMake BundleUtilities](https://cmake.org/cmake/help/latest/module/BundleUtilities.html), [podpisywanie Apple](https://developer.apple.com/library/archive/technotes/tn2206/_index.html), [notaryzacja Apple](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution).

## Sprawdzenie finalnej paczki i raport

- Spakuj `.app` narzędziem zachowującym prawa i metadane, np. `ditto -c -k --sequesterRsrc --keepParent`, lub przygotuj DMG przez `hdiutil`. Użyj nazw z wersją i architekturą. Dodaj sumy SHA-256.
- Rozpakuj wynik do innego katalogu, ze spacjami w nazwie. Uruchom aplikację także z innym bieżącym katalogiem. Przetestuj wersję z paczki przez `ALF_EXE`, zamiast przypadkiem ponownie testować build deweloperski.
- Sprawdź uruchomienie z Findera oraz, dla DMG, po skopiowaniu aplikacji z obrazu. Nie zapisuj stanu wewnątrz bundle ani na zamontowanym obrazie.
- Sprawdź brak wymogu zainstalowanego Homebrew przez analizę wszystkich zależności i uruchomienie w środowisku bez bibliotek deweloperskich. Samo wyczyszczenie `PATH` nie ukrywa dylibów z absolutnych ścieżek.
- Poproś użytkownika o ocenę obrazu, proporcji 1:2, kursora, lewego/prawego kliku, spacji, pełnego ekranu, paneli oraz o odsłuch muzyki. Oddziel automatyczne wyniki od kontroli wykonanej przez człowieka.
- Uzupełnij `README.md`, `CREDITS.md` i `PROGRESS.md`. W raporcie podaj ścieżki i SHA-256 paczek, commit, system, architekturę, minimalną wersję systemu, stan podpisu/notaryzacji, dokładne wyniki testów oraz pozostałe ograniczenia. Nie deklaruj testów na Intelu, starszym macOS ani czystym Macu, jeśli nie zostały wykonane.
