# PROGRESS: Alfabet Śmierci port

Stan na: 2026-10-17.

## Natywne buildy Linux/macOS (2026-10-15)

- Dodano osobny build CMake dla C11 + SDL3 + libopenmpt. Build Windows przez `build.sh` pozostaje bez zmian; natywne pliki powstają wyłącznie w ignorowanym `build/native/`.
- Linux: wykonywalny `build/native/AlfabetSmierci/AlfabetSmierci`, dane obok niego. macOS: `build/native/AlfabetSmierci.app`, dane w `Contents/Resources/data`. Zapisy Linux/macOS są w katalogu preferencji użytkownika SDL, zapis Windows nadal trafia obok programu.
- Narzędzia testowe automatycznie wybierają natywny build, jeśli istnieje; `ALF_EXE` pozwala wskazać inny plik. Systemowy build deweloperski korzysta z SDL3/libopenmpt z systemu. Osobny Linuxowy tar.gz statycznie łączy SDL3 i libopenmpt, buduje się w przypiętym obrazie Ubuntu 22.04 i pozostawia dynamiczne sterowniki SDL do obsługi przez system (X11/Wayland, ALSA/PulseAudio). `tools/make_linux_appimage.sh` opakowuje tar w AppImage. CI buduje Linuxowy tar i AppImage, uruchamia smoke/save testy i udostępnia oba pliki; osobny job buduje macOS-owe archiwum deweloperskie.
- Sprawdzone lokalnie na Linuxie: systemowy CMake build z SDL3 3.4.16/libopenmpt 0.8.9; pakiet dystrybucyjny skompilowany w Ubuntu 22.04 (glibc 2.35), rozpakowany w ścieżce ze spacjami i zweryfikowany przez SHA256SUMS. Pakiet wymaga maksymalnie GLIBC_2.34 i dynamicznie linkuje tylko libc/libm. Intro zrzuciło ekrany i ślad w `intro.alf`; testy `loc_04`/`loc_05`, trzy sloty dyskietki zapisu oraz szybki zapis/odczyt przeszły. Pakiet przeszedł wszystkie 263 kroki pełnego playthrough, osiągnął loc_3D i wrócił do tytułu. Testy graficzno-audio na Wayland/PulseAudio i X11/PulseAudio również przeszły; macOS nie jest zweryfikowany lokalnie.
- AppImage powstaje przez przypięte SHA-256 appimagetool 1.9.1 i runtime 20251108. Uruchomienie przez FUSE oraz `--appimage-extract-and-run` przeszło test dojścia do loc_04 i szybkiego zapisu/odczytu bez modyfikacji paczki. Pomiar wersji TAR na Wayland/PulseAudio: 49.88 kl./s i 48008 próbek/s, logi w `work/desktop-package-test/`. Odsłuch muzyki i ocena sterowania przez gracza pozostają kontrolą ręczną.
- Instrukcja przekazania prac Codexowi na Macu: `docs/MACOS_CODEX.md`. Mac lub runner macOS służy do natywnego builda i domknięcia paczki `.app`; samo archiwum deweloperskie z CI nadal wymaga bibliotek Homebrew.
- Build natywny odszukuje zasoby względem własnej lokalizacji SDL, a nie bieżącego katalogu. Dodatkowa kontrola z katalogu `/tmp` zapisała szybki zapis `stan.sav` w katalogu użytkownika SDL.

## Samodzielna paczka macOS (2026-10-17)

- Sprawdzone lokalnie na Apple Silicon: macOS 26.6.2, arm64, Apple Clang 21.0.0,
  CMake 4.2.1, SDL3 3.4.18 i libopenmpt 0.8.9. Skrypt
  `python3 tools/package_macos.py` buduje aplikację, kopiuje pełne drzewo bibliotek
  niesystemowych do `Contents/Frameworks`, zmienia ścieżki Mach-O na relatywne,
  dołącza dokumentację i licencje, podpisuje kod od środka na zewnątrz i tworzy zip
  oraz sumę SHA-256.
- Finalny artefakt:
  `dist/macos/AlfabetSmierci-1.0-macos-arm64.zip`, SHA-256
  `b0700843ec2383858cb1a2e704cf3ca09d708c2ecdf724a806a38ba6397a8026`,
  źródłowy commit `04a4a45c816e69dbeec0db05c8f4aa8b6b3597fe`.
- Aplikacja i wszystkie dołączone biblioteki są arm64. Minimalny system to macOS
  26.0, ponieważ taki cel mają zainstalowane binaria Homebrew. Nie jest to Universal
  2, build Intel ani paczka sprawdzona na starszym macOS.
- Dołączone biblioteki dynamiczne: SDL3, libopenmpt, mpg123, libogg, libvorbis i
  libvorbisfile. `otool -L` głównej binarki i każdej biblioteki nie zawiera ścieżek
  Homebrew, repozytorium, builda ani katalogu użytkownika. Nie wykonano osobnego testu
  na czystym Macu bez Homebrew, ale kompletność paczki potwierdza rekurencyjna analiza
  zależności i uruchomienie kopii z rozpakowanego archiwum.
- Podpis ad hoc przechodzi
  `codesign --verify --deep --strict --verbose=2`. Na tym Macu nie ma tożsamości
  Apple Developer ID ani profilu `notarytool`, dlatego paczka nie jest podpisana
  publicznym certyfikatem i nie jest notaryzowana ani staplowana.
- Dokładna kopia z zipa została rozpakowana do ścieżki ze spacjami. Przeszła
  `loc_04`, `loc_05`, trzy sloty oryginalnego menu, szybki zapis i odczyt, pełne
  przejście 263 kroków do `loc_3D` oraz powrót do tytułu. Osobny test bez
  `ALF_SKIP_INTRO`, uruchomiony z `/private/tmp`, doszedł do `loc_04` w klatce 5500.
  Zapisy testowe były odizolowane przez tymczasowy katalog użytkownika macOS.
- Start dwuklikiem z Findera przeszedł. Znaleziony podczas kontroli błąd Retina
  powodował mały obraz z dużą czarną ramką w pełnym ekranie. Commit `04a4a45`
  włącza bufor high-DPI, liczy układ z rzeczywistego rozmiaru renderera i koryguje
  rozmiar kursora. Sprawdzone wizualnie: pełny ekran wykorzystuje prawie cały ekran,
  F11 przechodzi do poprawnie wypełnionego okna i wraca do pełnego ekranu bez
  pomniejszenia.
- Testy dummy nie potwierdzają odsłuchu muzyki. Ręcznie pozostaje ocena brzmienia,
  kursora i wygody sterowania przez gracza; nie deklarować ich jako sprawdzonych
  automatem.

## Gdzie jesteśmy

- **Faza 1** (rekonesans, RECON.md): zrobiona.
- **Faza 2** (assety, stack, Checkpoint A, STACK.md): zrobiona.
- **Faza 3** (silnik MVP, Checkpoint B, grywalny wycinek): zrobiona.
- **Faza 4** (reszta gry, save/load, szlif, finalny build): **w toku**.
  - Gotowe: całe intro zliftowane i działa (Seven Stars, arte, tytuł z linijką portu, plansze autorów, strony fabuły; porównane wizualnie z WinUAE), przejście intro -> gra, wszystkie 65 procedur lokacji zliftowanych, zakończenie (`loc_3D`) działa, credits (2 osobne ekrany, karta autora portu, Claude/Claude Code, linia ReadyM), zapis gry na `saves/stan_gry.adf` (menu gry, 3 sloty) i szybki zapis z panelu (`saves/stan.sav`).
  - Smoke test lokacji (warp + wszystkie przedmioty + 4000 klatek losowych kliknięć) przeszedł dla wszystkich lokacji (`loc_02`..`loc_3C`). Jedyne "PROBLEMS" to ekrany śmierci, zgodnie z oczekiwaniem.
  - **Gra przechodzi do końca** (2026-10-14): `tools/playthrough.py` gra 263 kroki solucji (`tests/walkthrough.json`, opis `docs/WALKTHROUGH.md`) jednym ciągiem od dworca do ostatniej karty zakończenia (83 327 klatek, ok. 28 min czasu gry), z kontrolą lokacji i flag po każdym kroku; potem klawisz wraca do tytułu.
  - Po drodze: FIRE joysticka (gra go wymaga w 3 miejscach) pod spacją, środkowym przyciskiem myszy i przyciskiem pada; zakończenie wraca do tytułu (decyzja Pete'a).

## Dokładny następny krok

Plan Pete'a z 2026-10-14 (każdy punkt to etap: commit + PROGRESS.md + linia statusu; stop przed finalnym buildem = Checkpoint C):

1. (zrobione) Lokacje opcjonalne (sklep, ślepy zaułek, wizja, akta parafii, loc_1C) i śmierci normalną drogą: `py tools/test_paths.py`, wszystkie 8 scenariuszy ok, raport `work/paths/report.txt`.
2. (zrobione) Porównanie z oryginałem w WinUAE: te same kliknięcia w obu, zrzut pamięci, porównanie bitplanów ekranu (`$96738`) i kolorów z copper listy (`$591DE`) plus niezależny render do RGB (`tools/ref_scenes.py`, `tools/compare_ref.py`, `tools/uae_ref.py`). Dworzec (`loc_04`, dysk 2), miasteczko (`loc_05`, klatka z programu) i szpital (`loc_11`, dysk 3): identyczne bajt w bajt, 0 różnych pikseli. Mapa (`loc_06`): 31 pikseli w prostokącie znacznika szpitala (x 311-321, linie 168-174), bo w oryginale flaga `$7C0CD` była już ustawiona przez wcześniejszy, przerwany przebieg; to różnica stanu testu, nie portu. Dysk 4 nieporównany (lokacje dopiero w drugiej fazie gry).
3. (zrobione) Normalne tempo z intro (`py tools/realtime_check.py`, raport `work/realtime/`): 50.00 klatek na sekundę zegara, audio 47 993 próbek/s (48 kHz, urządzenie gra w czasie rzeczywistym), zmiana lokacji dworzec -> miasteczko 60 klatek (fade out ok. 32 + fade in ok. 29, zgodnie z pomiarem w WinUAE). Intro: czasy plansz (BPLCON0 pod `$29916`) w porcie i w oryginale w WinUAE: arte 24.2/24.5 s, trzecia plansza 35.6/35.7 s, tytuł 43.8/44.4 s, plansze autorów 55.1/55.1 s, fabuła 65.9/66.3 s; różnice do 0.6 s, w granicy rozdzielczości odczytu przez IPC. Muzyka: tempa wierszy nie da się prosto porównać (moduły mają pętle patternów E6x i zmiany speed Fxx); konwersja P60A -> MOD jest symboliczna, ucho Pete'a w buildzie testowym to ostatnie sprawdzenie.
4. (zrobione) Szlif: punkty 1-3 nie wykazały błędów portu. Jedyny otwarty błąd wizualny (znak na krawędzi stron fabuły) sprawdzony: wszystkie 4 strony fabuły są identyczne z oryginałem bajt w bajt (bitplan `$2AFD0`, copper `$29908`), więc to zachowanie oryginału. Printy `ALF_DBG` zostają jako udokumentowane narzędzie diagnostyczne (bez zmiennej nic nie robią). FIRE zostaje na spacji, środkowym przycisku myszy i padzie.
5. (zrobione) Paczka: nowe CZYTAJ.txt po polsku (uruchamianie, co jest oryginałem a co portem, skąd dane, zapisy, sterowanie z FIRE, powrót do tytułu po zakończeniu). CREDITS.md i ekran narzędzi w grze uzupełnione o PowerShell oraz miniz, minimp3 i stb_vorbis (siedzą w libopenmpt, gra ich nie używa); THIRD_PARTY_LICENSES.txt zawiera ich licencje. Oba ekrany credits sprawdzone na zrzutach (8 stron autorów z kartą autora portu, 2 strony narzędzi z linią ReadyM). Nieużywane `stb_image.h` usunięte z `fetch_deps.sh`/`build_deps.sh`. Regresja: loc_04, loc_05 ok, intro -> gra ok.
6. (zrobione) Checkpoint C: build testowy wysłany Pete'owi 2026-10-15.
7. (zrobione 2026-10-15) Poprawki po teście Pete'a:
   - Dialogi: w oryginale (kod i WinUAE) kwestie przewija tylko prawy przycisk, a kursor jest schowany (copper SPR0PT = `$1CC16`). Port: przewija też lewy przycisk i spacja/FIRE, rączka widoczna w czasie mowy. Cała gra przechodzi samym lewym przyciskiem (`py tools/playthrough.py --lmb-only`, 263 kroki).
   - FIRE: dodatkowo lewy klik w puste miejsce obrazka w loc_2D/34/35 (sprawdzone na zapisach przed krokami 179/182/187).
   - Kursor: sprite'y nie trafiają do obrazu, okno ma sprzętowy kursor z grafiki gry w skali okna, jedyny, także nad panelami; brak śladu na krawędzi.
   - Linia portu: tytuł (była), ostatnia strona "Autorzy" (teraz na górze karty), nowa strona po napisach końcowych w stylu oryginału (`show_port_end_page` w `natives.c`).
   - Polskie znaki: dialog w pociągu identyczny bajt w bajt z oryginałem; "ż" w foncie gry to "z" z poprzeczną kreską, tekst ma poprawne kody. Bez zmian.
8. (zrobione 2026-10-15) Wersja 1.0: Pete zaakceptował build jako "wersję 1 do ogrania" (sprawdził ręcznie do szpitala). Repo grywalne prosto z GitHuba: folder `AlfabetSmierci/` (exe, oryginalne dyskietki, skrypty, CZYTAJ, licencje) jest commitowany, `README.md` jako strona repo, numer wersji w `src/version.h` (tytuł okna, CZYTAJ), tag `v1.0`. Paczki na dysku obok repo: `hoyn-games/AlfabetSmierci-1.0.zip` (gra) i `hoyn-games/alfabet-port-1.0-src.zip` (całe repo, to samo co Download ZIP z GitHuba).
**Teraz:** wersja 1.1 po uwagach Pete'a z grania. Kandydaci: porównanie lokacji z dysku 4 z WinUAE, skala w pełnym ekranie, przegląd kodu pod Maniaka. Push na GitHuba tylko na wyraźne polecenie Pete'a.

## Tabela lokacji

Kolumny: procedura, adres w oryginale, zliftowana, uruchamia się (warp), przetestowana (smoke test), przechodzi do końca (pełne przejście gry), uwagi. "przeszło do" = numery lokacji, które silnik odwiedził w teście (linie `loc $XX` w logu).
Ekrany śmierci (`*_2`, `*_3` wołane z `game_over`/`game_over_b` przez `call`) kończą wątek gry w teście warp, to jest oczekiwane.
Kolumna "przechodzi do końca": "tak" = lokacja zaliczona w pełnym przejściu gry (`tools/playthrough.py`, 263 kroki solucji z `tests/walkthrough.json`, 2026-10-14, od dworca do ostatniej karty zakończenia jednym ciągiem, z kontrolą lokacji i flag po każdym kroku). "tak (osobno)" = poza ścieżką solucji (lokacja opcjonalna albo śmierć), osiągnięta normalną drogą w `tools/test_paths.py` z zapisów pełnego przejścia. "n/d" = ekran śmierci, który gra losuje w grupie już sprawdzonej normalną drogą.

| procedura | adres | zliftowana | uruchamia się | przetestowana | przechodzi do końca | uwagi |
|---|---|---|---|---|---|---|
| `loc_02` | `$13EF6` | tak | tak | tak | tak (osobno) | ekran śmierci osiągnięty normalną drogą: game_over, zła odpowiedź u Leni (loc_0B wiersz 1, przez loc_0E); muzyka śmierci, potem restart w loc_04 (`tools/test_paths.py`) |
| `loc_02_2` | `$13F3A` | tak | tak | tak | n/d | ekran śmierci; `game_over` losuje ekran, ta grupa sprawdzona normalną drogą (inny ekran z tej samej grupy); sam ekran sprawdzony warpem |
| `loc_03` | `$030F0` | tak | tak | tak | tak | przeszło do: 03 05 |
| `loc_04` | `$035DC` | tak | tak | tak | tak | przeszło do: 03 04 05 |
| `loc_05` | `$0390A` | tak | tak | tak | tak | przeszło do: 03 04 05 |
| `loc_06` | `$04028` | tak | tak | tak | tak | przeszło do: 05 |
| `loc_07` | `$044FA` | tak | tak | tak | tak (osobno) | sklep, poza ścieżką solucji; osiągnięta normalną drogą i opuszczona (`tools/test_paths.py`) |
| `loc_07_2` | `$13FC2` | tak | tak | tak | n/d | ekran śmierci; `game_over` losuje ekran, ta grupa sprawdzona normalną drogą (inny ekran z tej samej grupy); sam ekran sprawdzony warpem |
| `loc_08` | `$04CE4` | tak | tak | tak | tak | przeszło do: 05 08 |
| `loc_08_2` | `$13F7E` | tak | tak | tak | n/d | ekran śmierci; `game_over` losuje ekran, ta grupa sprawdzona normalną drogą (inny ekran z tej samej grupy); sam ekran sprawdzony warpem |
| `loc_09` | `$0527A` | tak | tak | tak | tak | przeszło do: 05 |
| `loc_0A` | `$05A62` | tak | tak | tak | tak (osobno) | ślepy zaułek, poza ścieżką solucji; osiągnięta normalną drogą i opuszczona (`tools/test_paths.py`) |
| `loc_0B` | `$05C3C` | tak | tak | tak | tak | przeszło do: 05 0B |
| `loc_0C` | `$06252` | tak | tak | tak | tak | przeszło do: 0C 0F |
| `loc_0D` | `$067A8` | tak | tak | tak | tak | przeszło do: 0D |
| `loc_0E` | `$071B2` | tak | tak | tak | tak (osobno) | śmierć po złej odpowiedzi w loc_0B, osiągnięta normalną drogą (`tools/test_paths.py`) |
| `loc_0F` | `$07214` | tak | tak | tak | tak | przeszło do: 0F 10 |
| `loc_0F_2` | `$072E2` | tak | tak | tak | tak | przeszło do: 05 |
| `loc_10` | `$075AC` | tak | tak | tak | tak | przeszło do: 10 |
| `loc_11` | `$07D32` | tak | tak | tak | tak | przeszło do: 10 |
| `loc_12` | `$082FC` | tak | tak | tak | tak | przeszło do: 03 04 05 12 |
| `loc_12_2` | `$1395C` | tak | tak | tak | tak | zwycięstwo nad Bercerem, droga do zakończenia; przeszło do: 12 3C |
| `loc_12_3` | `$1407C` | tak | tak | tak | n/d | ekran śmierci; `game_over_b` losuje ekran, ta grupa sprawdzona normalną drogą (inny ekran z tej samej grupy); sam ekran sprawdzony warpem |
| `loc_13` | `$08720` | tak | tak | tak | tak | przeszło do: 03 04 05 13 |
| `loc_15` | `$08D34` | tak | tak | tak | tak | przeszło do: 0D |
| `loc_16` | `$0917C` | tak | tak | tak | tak | przeszło do: 05 |
| `loc_17` | `$09952` | tak | tak | tak | tak (osobno) | wizja, poza ścieżką solucji; osiągnięta normalną drogą i opuszczona (`tools/test_paths.py`) |
| `loc_18` | `$099D6` | tak | tak | tak | tak (osobno) | akta parafii, poza ścieżką solucji; osiągnięta normalną drogą i opuszczona (`tools/test_paths.py`) |
| `loc_19` | `$09E40` | tak | tak | tak | tak | przeszło do: 05 19 |
| `loc_1B` | `$0A676` | tak | tak | tak | tak | przeszło do: 05 1B 1C |
| `loc_1C` | `$0A946` | tak | tak | tak | tak | przerywnik po rozmowie w loc_1B (krok 42), w logu przejścia niewidoczny, bo krótki; jego flaga `$7C0E1`=1 sprawdzona |
| `loc_1D` | `$0A9B4` | tak | tak | tak | tak | przeszło do: 04 05 1D 1E |
| `loc_1E` | `$0AC3E` | tak | tak | tak | tak | przeszło do: 03 04 05 1E |
| `loc_1F` | `$0B558` | tak | tak | tak | tak | przeszło do: 1F |
| `loc_1F_2` | `$14148` | tak | tak | tak | n/d | ekran śmierci; `game_over_b` losuje ekran, ta grupa sprawdzona normalną drogą (inny ekran z tej samej grupy); sam ekran sprawdzony warpem |
| `loc_20` | `$0BBDE` | tak | tak | tak | tak | przeszło do: 1F 20 |
| `loc_21` | `$0B080` | tak | tak | tak | tak | przeszło do: 21 |
| `loc_22` | `$0BF1A` | tak | tak | tak | tak | przeszło do: 22 24 |
| `loc_22_2` | `$14104` | tak | tak | tak | n/d | ekran śmierci; `game_over_b` losuje ekran, ta grupa sprawdzona normalną drogą (inny ekran z tej samej grupy); sam ekran sprawdzony warpem |
| `loc_23` | `$0CDA0` | tak | tak | tak | tak | przeszło do: 23 |
| `loc_23_2` | `$140C0` | tak | tak | tak | tak (osobno) | ekran śmierci osiągnięty normalną drogą: game_over_b, lustro w loc_21 przed zamalowaniem; muzyka śmierci, potem restart w loc_04 (`tools/test_paths.py`) |
| `loc_24` | `$0D2B2` | tak | tak | tak | tak | przeszło do: 22 24 |
| `loc_25` | `$0D706` | tak | tak | tak | tak | przeszło do: 25 |
| `loc_27` | `$0DA7A` | tak | tak | tak | tak | przeszło do: 27 28 2D |
| `loc_27_2` | `$14216` | tak | tak | tak | n/d | ekran śmierci; `sub_1418C` losuje ekran, ta grupa sprawdzona normalną drogą (inny ekran z tej samej grupy); sam ekran sprawdzony warpem |
| `loc_28` | `$0EA88` | tak | tak | tak | tak | przeszło do: 27 28 2D |
| `loc_2B` | `$0F15E` | tak | tak | tak | tak | przeszło do: 04 05 2B |
| `loc_2C` | `$0F51C` | tak | tak | tak | tak | przeszło do: 0D 2C |
| `loc_2D` | `$0F91A` | tak | tak | tak | tak | przeszło do: 04 05 13 22 2D |
| `loc_2E` | `$0FC78` | tak | tak | tak | tak | przeszło do: 22 2E 34 |
| `loc_2F` | `$0FE8C` | tak | tak | tak | tak | przeszło do: 2E 2F 34 |
| `loc_30` | `$10880` | tak | tak | tak | tak | przeszło do: 30 |
| `loc_31` | `$10B64` | tak | tak | tak | tak | przeszło do: 0D 31 |
| `loc_31_2` | `$14282` | tak | tak | tak | tak (osobno) | ekran śmierci osiągnięty normalną drogą: sub_1425A, koniec sceny w loc_2B bez proszku; muzyka śmierci, potem restart w loc_04 (`tools/test_paths.py`) |
| `loc_32` | `$112F4` | tak | tak | tak | tak | przeszło do: 04 05 23 32 |
| `loc_33` | `$1158C` | tak | tak | tak | tak | przeszło do: 33 |
| `loc_34` | `$11C06` | tak | tak | tak | tak | przeszło do: 22 34 |
| `loc_35` | `$1224C` | tak | tak | tak | tak | przeszło do: 35 |
| `loc_35_2` | `$141DE` | tak | tak | tak | tak (osobno) | ekran śmierci osiągnięty normalną drogą: sub_1418C, zły klocek w loc_35; muzyka śmierci, potem restart w loc_04 (`tools/test_paths.py`) |
| `loc_37` | `$12E9E` | tak | tak | tak | tak | przeszło do: 37 |
| `loc_38` | `$1316A` | tak | tak | tak | tak | przeszło do: 38 |
| `loc_39` | `$13392` | tak | tak | tak | tak | przeszło do: 39 3A |
| `loc_3A` | `$1366C` | tak | tak | tak | tak | przeszło do: 39 3A |
| `loc_3C` | `$13A84` | tak | tak | tak | tak | przeszło do: 3C |
| `loc_3D` | `$13CFA` | tak | tak | tak | tak | zakończenie gry: obrazek, gratulacje, podziękowania; potem klik/klawisz wraca do tytułu (natywny `halt`) |

Intro (`scripts/intro.alf`, osobny program z dysku 1): zliftowane, działa, sprawdzone wizualnie z WinUAE.

## Znane bugi

- (wyjaśnione 2026-10-15) Strony fabuły w intro: znak na prawej krawędzi to zachowanie oryginału, strony są identyczne bajt w bajt z WinUAE.
- (sprawdzone 2026-10-15) Tempo intro zgodne z oryginałem w WinUAE w granicy 0.6 s na planszę, mimo że wynika z budżetu VM (`INSNS_PER_FRAME`), nie z pomiaru instrukcji.
- Opcja "Skala" w panelu działa tylko w oknie (w pełnym ekranie skala jest maksymalna całkowita).
- `src/script.c`: diagnostyczne printy pod zmienną `ALF_DBG` zostają jako udokumentowane narzędzie (bez zmiennej nic nie robią).

## Otwarte pytania (do Pete'a)

Brak. Odpowiedzi z 2026-10-14 (koniec gry wraca do tytułu, exe 8.9 MB OK, jeden slot szybkiego zapisu) są w CLAUDE.md.

## Niedokończone

- Porównanie pikselowe `loc_05` z WinUAE: przerwane. Wstrzykiwanie myszy przez IPC (`CFG MOUSE1_HORIZ 100`) skacze kursorem do limitów (`$1CC12`=027A, `$1CC14`=0253), skala ruchu nieskalibrowana. Nic w repo od tego nie zależy.

## Komendy

Wszystko z katalogu repo (`C:\Users\proma\hoyn-games\alfabet-port`), Git Bash.

Natywny build Linux/macOS (systemowe CMake, SDL3 i libopenmpt, wynik w `build/native/`):

    cmake -S . -B build/native -DCMAKE_BUILD_TYPE=Release
    cmake --build build/native --parallel

Na macOS Homebrew: `brew install cmake pkgconf sdl3 libopenmpt`. Na Ubuntu 26.04: `sudo apt install build-essential cmake pkg-config libsdl3-dev libopenmpt-dev`.

Pakiet Linux x86-64 (Ubuntu 22.04 baseline; Docker, wynik `dist/linux/`):

    sh tools/build_linux_package.sh
    sh tools/make_linux_appimage.sh

Pierwsza komenda tworzy tar.gz i sumę SHA256; druga buduje pojedynczy plik AppImage z gotowego tar.gz. Po rozpakowaniu tar uruchom `uruchom.sh`. AppImage bez FUSE uruchom przez `./AlfabetSmierci-1.0-linux-x86_64.AppImage --appimage-extract-and-run`. SDL3 i libopenmpt są statyczne, lecz host nadal musi dostarczyć biblioteki graficzne i audio wymagane przez X11/Wayland oraz ALSA/PulseAudio.

Jednorazowo (toolchain i biblioteki, wszystko lokalnie w `tools/toolchain` i `deps`, nic systemowo):

    sh tools/fetch_deps.sh
    tools/toolchain/w64devkit/bin/sh tools/build_deps.sh

Oryginalne dane: `orig/zips` (kopie zipów, nietykane, lokalnie), dyskietki w repo: `AlfabetSmierci/data/disk1..4.adf`. Assety do podglądu:

    py tools/extract.py                       # -> extracted/

Lifting skryptów (regeneruje `scripts/*.alf`, czyta `AlfabetSmierci/data/disk1.adf`):

    PYTHONIOENCODING=utf-8 py tools/lifter.py

Build + paczka (exe, dane, CZYTAJ.txt, THIRD_PARTY_LICENSES.txt, zip):

    tools/toolchain/w64devkit/bin/sh build.sh
    # -> AlfabetSmierci/AlfabetSmierci.exe (+ data/scripts, CZYTAJ, licencje), work/AlfabetSmierci-<wersja>.zip

Uruchomienie:

    AlfabetSmierci/AlfabetSmierci.exe

Zmienne środowiskowe silnika (testy/diagnostyka):

| zmienna | działanie |
|---|---|
| `ALF_WINDOWED=1` | okno zamiast pełnego ekranu |
| `ALF_SKIP_INTRO=1` | od razu gra |
| `ALF_WARP=loc_XX` | skok do procedury lokacji |
| `ALF_ITEMS=7BXXX,7BYYY` | przedmioty do ekwipunku na start (hex adresy nazw, bez `$`) |
| `ALF_AUTOPLAY=plik` | skrypt zdarzeń: `<klatka> move x y`, `lclick x y`, `lup`, `rclick`, `rup`, `key`, `keyup`, `fire`, `fireup`, `qsave`, `qload`, `mark tekst`, `state`, `shot plik.bmp`, `canvas plik.bmp`, `pclick`, `oclick`, `dump plik`, `quit` |
| `ALF_FAST=1` | bez czekania na 50 Hz |
| `ALF_TRACE=N` | co N klatek linia `tick` z miejscem w skrypcie |
| `ALF_DBG=1` | printy diagnostyczne VM (lmb, call) |
| `ALF_CONTROL=1` | sterowanie na żywo: komendy autoplay ze stdin, `<opóźnienie w klatkach> akcja`; `state` odpowiada na stdout (lokacja, linia skryptu, `rwait`, tabela hotspotów, `lmbpoll`). Używa tego `tools/playthrough.py` |
| `ALF_SAVE=plik` | plik szybkiego zapisu zamiast `saves/stan.sav` (testy) |

Testy:

    py tools/test_locations.py                 # wszystkie lokacje (~4 min na lokację max, długo: puszczać w tle)
    py tools/test_locations.py loc_04 loc_05   # wybrane; raport work/loctest/report.txt

Pełne przejście gry (solucja `tests/walkthrough.json`, opis `docs/WALKTHROUGH.md`, z zapisem stanu po każdym kroku w `work/play/step_NNN.sav`, log `work/play/log.txt`):

    py tools/playthrough.py                                   # od początku (ALF_SKIP_INTRO)
    py tools/playthrough.py --from 61 --load work/play/step_060.sav   # wznowienie

Bot klika prawym tylko gdy gra czeka na prawy klik (`rwait`), wiersz dialogu dopiero gdy lista opcji jest na ekranie i gra odpytuje wejście, a krok uznaje za skończony, gdy gra znowu odpytuje wejście (`lmbpoll` <= 2). Po każdym kroku sprawdza lokację z następnym krokiem solucji.

Porównanie z oryginałem (WinUAE uruchomiony przez `run_original.bat`):

    py tools/uae_ref.py boot                   # przeklikuje intro (JOY1_FIRE_BUTTON = lewy przycisk), wkłada dysk 2
    py tools/ref_scenes.py uae                 # sceny w oryginale -> work/ref/uae_*.bin
    py tools/ref_scenes.py port                # te same w porcie -> work/ref/port_*.bin
    py tools/compare_ref.py work/ref/uae_town.bin work/ref/port_town.bin work/ref/town

Lokacje opcjonalne i śmierci normalną drogą (potrzebuje `work/play/step_NNN.sav` z pełnego przejścia):

    py tools/test_paths.py                     # raport work/paths/report.txt

Regresja szybka po zmianach: `loc_04`, `loc_05` muszą dać `ok`, a intro -> gra musi przejść. Strony fabuły w intro czekają na klik (albo ~12 min każda), więc autoplay klika co 500 klatek. Ścieżki dla exe muszą być windowsowe (`pwd -W`), nie `/c/...`. Sprawdzone 2026-10-14: w klatce ~5500 gra jest w `loc_04.alf`, zrzut pokazuje dworzec.

    W=$(pwd -W)
    { for t in 1000 1500 2000 2500 3000 3500 4000; do printf '%d lclick 320 200\n%d lup\n' $t $((t+3)); done
      printf '6000 shot %s/work/auto/regress.bmp\n6002 quit\n' "$W"; } > work/auto/regress.txt
    ALF_AUTOPLAY="$W/work/auto/regress.txt" ALF_FAST=1 ALF_WINDOWED=1 ALF_TRACE=500 \
      AlfabetSmierci/AlfabetSmierci.exe > work/auto/regress.log 2>&1
    tail -2 work/auto/regress.log     # musi być "at loc_04.alf:..."; regress.bmp = dworzec

Lifter czyta tylko `AlfabetSmierci/data`, `work/` nie jest potrzebny do regeneracji skryptów. Rozpakowane binarki do analizy: `py tools/unpack_disks.py` (-> `work/unpacked/`), disasm: `tools/dis68k.py` (wynik zawsze do pliku).

Oryginał w WinUAE (AROS, bez Kickstartu):

    run_original.bat
    powershell -File tools/uaeipc.ps1 "CFG floppy0 C:/.../AlfabetSmierci/data/disk2.adf"   # zmiana dyskietki
    powershell -File tools/uaeipc.ps1 "DBG m 1cc12 1"                            # podgląd pamięci
    powershell -File tools/winshot.ps1 out.png                                   # zrzut okna (PrintWindow)
    powershell -File tools/uaepark.ps1                                           # przywróć zminimalizowane okno
