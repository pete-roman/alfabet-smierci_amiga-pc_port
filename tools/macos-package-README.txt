ALFABET ŚMIERCI - wersja dla macOS
=================================

Gra: Arte / Seven Stars Multimedia (C) 1995/1996.
Port by Pete Roman, for Seven Stars Multimedia and Maciej 'Maniak' Frontkiewicz, with love.


URUCHOMIENIE

Przenieś AlfabetSmierci.app do wybranego katalogu, na przykład Aplikacje, i otwórz
ją w Finderze. Aplikacja zawiera SDL3, libopenmpt i pozostałe wymagane biblioteki.
Homebrew ani uprawnienia administratora nie są potrzebne.

Architektura tej paczki: @ARCHITECTURES@. Wymaga macOS @MINIMUM_MACOS@ lub
nowszego. Nie zakładaj zgodności z inną architekturą ani starszym macOS bez
osobnego builda i testu.

Aplikacja ma lokalny podpis ad hoc. Nie ma podpisu Apple Developer ID i nie jest
notaryzowana przez Apple. Przy pierwszym uruchomieniu pobranej kopii Gatekeeper może
wymagać użycia menu kontekstowego Otwórz albo potwierdzenia w Ustawieniach systemowych,
Prywatność i ochrona. Nie wyłączaj Gatekeepera.


DANE I ZAPISY

Oryginalne obrazy dyskietek oraz skrypty są w Contents/Resources/data wewnątrz
aplikacji. Gra ich nie modyfikuje. Zapisy są przechowywane w katalogu danych
użytkownika macOS wskazanym przez SDL, poza aplikacją:

  stan_gry.adf   trzy miejsca zapisu dostępne z oryginalnego menu gry
  stan.sav       szybki zapis z lewego panelu


STEROWANIE

  lewy przycisk myszy     wybór, przejście, opcja dialogu, kolejna kwestia
  prawy przycisk myszy    kolejna kwestia, jak w oryginale na Amidze
  spacja                  kolejna kwestia i FIRE
  środkowy przycisk       FIRE
  dowolny przycisk pada   FIRE
  F11 lub Alt+Enter       pełny ekran lub okno
  Tab                     pokaż lub ukryj panele boczne

Obraz zachowuje oryginalne proporcje pikseli 1:2, całkowite skalowanie i nearest
neighbour. Kursor gry jest jedynym kursorem także nad panelami.


AUTORZY I LICENCJE

Autorzy gry i portu: CREDITS.md oraz ekran Autorzy w grze. Informacje o kodzie
portu są w LICENSE.txt. Licencje dołączonych bibliotek znajdują się w
THIRD_PARTY_LICENSES.txt i katalogu licenses.
