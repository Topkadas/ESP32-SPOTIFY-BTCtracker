# SpotifyDeck

*[English version](README.md)*

Čtyři stránky na jedné laciné desce s dotykovým displejem: **Spotify ovladač**,
**počasí**, **kurz bitcoinu** a **analogové hodiny**. Přepíná se to tahem prstu.

Běží na desce z rodiny **CYD — Cheap Yellow Display** (ESP32-2432S024 nebo -S028).
Žádné pájení, žádné dráty, jedna deska a USB kabel.

| | |
|---|---|
| ![Spotify](docs/01-spotify.png) | ![Počasí](docs/02-pocasi.png) |
| ![Bitcoin](docs/03-bitcoin.png) | ![Hodiny](docs/04-hodiny.png) |

> Celé prostředí displeje je **česky i anglicky**. Dlouhý stisk → *Jazyk*.
> Volba se ukládá do desky.

---

## Podporované desky

Projekt běží na dvou deskách z rodiny „Cheap Yellow Display". Vypadají skoro stejně a mají
shodně zapojený displej, ale ve třech věcech se liší — a když je nastavíš špatně, zůstane
černá obrazovka nebo nefunguje dotyk.

| | **ESP32-2432S024** (2,4") | **ESP32-2432S028** (2,8") |
|---|---|---|
| Podsvícení | **GPIO 27** | **GPIO 21** |
| Dotyk (verze R) | XPT2046 **sdílí SPI s displejem**, `TOUCH_CS 33` | XPT2046 na **vlastní SPI** (SCK 25, MOSI 32, MISO 39, CS 33) |
| Panel | potřebuje `TFT_INVERSION_ON` | bez inverze |
| Piny displeje | MISO 12, MOSI 13, SCLK 14, CS 15, DC 2, RST −1 — **stejné** | stejné |

Dvě nastavení spolu musí souhlasit a musí sedět na tvoji desku:

- `DECK_BOARD` v [`SpotifyDeck/DeckConfig.h`](SpotifyDeck/DeckConfig.h) — `24` nebo `28`
- `CYD_BOARD` v [`User_Setup.h`](User_Setup.h) — stejné číslo

**Nevíš, kterou desku máš?** Nastav v `DeckConfig.h` `BL_SELFTEST` na `3` a nahraj. Deska po
startu projede kandidáty na pin podsvícení, každý na 2,5 s podrží HIGH a pak LOW, a přitom
píše a kreslí, který zrovna zkouší. Ten, u kterého se displej rozsvítí, je tvůj. Pak vrať
`BL_SELFTEST` na `0`.

Verze **-C** obou desek mají místo toho kapacitní CST820 na I²C, který tenhle firmware zatím
neumí. `TOUCH_PROBE 1` v `DeckConfig.h` ti řekne, co máš: po startu prohledá I²C na CST820
a pak SPI na XPT2046 a výsledek vypíše.

---

## Co to umí

### 1 · Spotify

- **Obal alba rozmazaný přes celou obrazovku** jako pozadí + ostrý čtverec obalu.
  Rozmazání nic nestojí — JPEG se prostě podruhé dekóduje v měřítku 1:8
  (300×300 → 37×37 px), dvakrát se přejede box filtrem a bilineárně roztáhne.
- **Automatická expozice pozadí.** Světlý obal (typicky *brat*) se ztlumí,
  tmavý zesvětlí, aby byl bílý text vždy čitelný.
- Název / interpret / album; **dlouhý název sám popojíždí**.
- **Průběh skladby** se mezi dotazy dopočítává lokálně, takže plyne plynule,
  i když se serveru ptáme jen jednou za 3 vteřiny. Tahem prstu přetočíš.
- **Play / pauza / další / předchozí / shuffle / repeat**, hlasitost tahem.
- Klepnutí na obal → obal přes celou obrazovku.
- Podpora **podcastů** (episode) i vícero interpretů.
- Respektuje `actions.disallows` od Spotify — co zrovna nejde, je zašedlé.

### 2 · Počasí

- Zdroj **Open-Meteo** — zdarma, **bez API klíče a bez registrace**.
- Teplota, pocitová teplota, vlhkost, vítr, slovní popis, předpověď na 3 dny.
- **Ikony počasí kreslené z primitiv** (slunce, měsíc, mraky, déšť, sníh,
  mlha, bouřka) — žádné bitmapy, takže jsou ostré v jakékoliv velikosti.
- Barva pozadí se mění podle počasí a denní doby.
- **Místo nastavíš jménem** ("Jirny", "Praha") ve WiFi portálu; přeloží se
  na souřadnice přes geokódovací API a uloží do NVS.

### 3 · Bitcoin

- Kurz z veřejného **Binance API** (bez klíče), **graf za 48 hodin**.
- Změna za 24 h, denní maximum a minimum.
- Měna **USD / EUR / CZK** — klepnutím na horní polovinu se přepíná.
  Koruny Binance nemá, počítají se přes **denní kurz ČNB**.
- Pozadí je zelené nebo červené podle toho, kam se to za den hnulo.

### 4 · Hodiny

- Analogový ciferník s vyhlazenými ručičkami, 60 rysek, datum.
- Kreslí se do spritu a posílá jedním průchodem → **neblikají**.
- Klepnutím se přepne na velké digitální hodiny.
- Čas z NTP, středoevropské pásmo včetně letního času.

### Napříč všemi stránkami

- **Čeština i angličtina.** Celé prostředí se přepíná v nastavení a volba
  přežije restart. Výchozí jazyk se dá změnit v `DeckConfig.h`.
- **Prst doleva/doprava** = další/předchozí stránka.
- **Dlouhý stisk** = nastavení (kalibrace dotyku, otočení displeje,
  WiFi portál, jazyk, smazání cache obalů, restart).
- **Kalibrace dotyku přímo na desce** — dva terče, výsledek do NVS.
  Pozná i případné prohození os. Žádné přepisování konstant ve zdrojáku.
- **Automatické ztlumení podsvícení** po nečinnosti, volitelně podle
  světelného čidla na desce.
- **RGB LED** na desce svítí dominantní barvou obalu alba.
- Podsvícení se na okamžik stáhne při velkém překreslení, takže z trhaného
  odkrývání JPEGu je plynulé prolnutí.

---

## Co potřebuješ

| Věc | Poznámka |
|---|---|
| Deska **CYD** | ESP32-2432S024 (2,4") nebo -S028 (2,8"), rezistivní dotyk |
| **USB kabel, který umí data** | Nabíjecí kabel se nepřihlásí jako port |
| **Spotify Premium** | Ovládání přehrávání jde jen s Premium |
| Arduino IDE | verze 2.x |
| Python 3.8+ | jen pro získání tokenu, jednorázově |

WiFi musí být **2,4 GHz** — ESP32 na 5 GHz neumí.

---

## Instalace krok za krokem

### 1. ESP32 do Arduino IDE

**Soubor → Nastavení → Additional Boards Manager URLs:**

```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

Pak **Nástroje → Deska → Boards Manager**, najdi `esp32` a nainstaluj balík
od **Espressif Systems**.

### 2. Knihovny

**Nástroje → Spravovat knihovny**, nainstaluj:

| Knihovna | Autor |
|---|---|
| `TFT_eSPI` | Bodmer |
| `TJpg_Decoder` | Bodmer |
| `XPT2046_Touchscreen` | Paul Stoffregen |
| `ArduinoJson` | Benoit Blanchon |
| `WiFiManager` | tzapu |

> Oproti návodům, které kolují po internetu, tady **není potřeba žádná
> Spotify knihovna staženy jako ZIP**. Klient Spotify API je součástí projektu
> (`DeckSpotify.cpp`) — je menší, správně ošetřuje chyby a nemusíš řešit
> „SpotifyArduino.h not found".

### 3. Nastavení displeje

Zkopíruj `User_Setup.h` z kořene projektu přes:

```
Dokumenty/Arduino/libraries/TFT_eSPI/User_Setup.h
```

> ⚠️ **Tenhle soubor je společný pro celý počítač.** Když ho přepíšeš,
> změní se nastavení displeje i pro tvoje ostatní TFT_eSPI projekty.
> Původní si radši nejdřív přejmenuj na `User_Setup.h.bak`.

**Tenhle krok je povinný, ne volitelný.** Přiložený soubor nese pin podsvícení,
inverzi barev a chip select dotyku pro tvoji desku. Bez něj dostaneš černou
obrazovku, vybledlé barvy nebo nefunkční dotyk — a ani jedno není na první
pohled zřejmé.

Nahoře v souboru nastav `CYD_BOARD` na `24` nebo `28` podle své desky a stejné
číslo dej i do `DECK_BOARD` v `SpotifyDeck/DeckConfig.h`.

### 4. Spotify aplikace

1. [developer.spotify.com/dashboard](https://developer.spotify.com/dashboard) → **Create app**
2. Redirect URI **přesně**:
   ```
   http://127.0.0.1:8888/callback
   ```
3. Zaškrtni **Web API**, ulož.
4. Opiš si **Client ID** a **Client secret**.

### 5. Token

V kořeni projektu spusť:

```bash
python tools/get_token.py
```

(Ve Windows jde taky dvojklik na `tools/get_token.bat`.)

Skript sám spustí lokální server, otevře prohlížeč, zachytí návratový kód,
vymění ho za token a **rovnou zapíše `SpotifyDeck/secrets.h`**. Žádné
kopírování kódu z adresního řádku, žádný curl, žádný závod s desetiminutovou
platností kódu.

### 6. Nahrání

Otevři `SpotifyDeck/SpotifyDeck.ino` a nastav:

| Nastroje → | Hodnota |
|---|---|
| Deska | **ESP32 Dev Module** |
| Partition Scheme | **Huge APP (3MB No OTA / 1MB SPIFFS)** |
| PSRAM | **Disabled** (WROOM na desce žádnou nemá) |
| Flash Size | 4MB (32Mb) |
| Upload Speed | 921600, při potížích 115200 |

Pak **Nahrát**. Když se to zasekne na „Connecting...", drž tlačítko **BOOT**,
dokud nezačne psát.

### 7. První spuštění

1. Deska vytvoří WiFi síť **`SpotifyDeck`**, heslo **`spotifydeck`**.
2. Připoj se telefonem, otevře se portál.
3. Vyber svou WiFi, zadej heslo a do pole **„Mesto pro pocasi"** napiš
   svoje město (výchozí `Jirny`).
4. Po restartu tě deska provede **kalibrací dotyku** — dvakrát klepni
   doprostřed terče.

Hotovo.

---

## Ovládání

| Gesto | Co udělá |
|---|---|
| Tah prstem doleva / doprava | Další / předchozí stránka |
| Dlouhý stisk (≈1 s) | Nastavení |
| Klepnutí na tlačítko | Příslušný příkaz |
| Tah po průběhu skladby | Přetočení |
| Tah po pruhu hlasitosti | Hlasitost |
| Klepnutí na obal | Obal přes celou obrazovku |
| Klepnutí na hodiny | Analogové ⇄ digitální |
| Klepnutí nahoře na BTC stránce | USD → EUR → CZK |
| Klepnutí na počasí | Vynutí obnovu |

---

## Nastavení v kódu

Všechno laditelné je v [`SpotifyDeck/DeckConfig.h`](SpotifyDeck/DeckConfig.h).

```c
#define FEAT_BLUR_BACKGROUND  1   // rozmazany obal pres celou plochu
#define FEAT_FULLSCREEN_ART   1   // klepnuti na obal
#define FEAT_VOLUME_SLIDER    1
#define FEAT_SEEK_BAR         1
#define FEAT_AUTO_DIM         1   // ztlumeni po necinnosti
#define FEAT_LDR_BRIGHTNESS   1   // jas podle svetelneho cidla
#define FEAT_RGB_LED          1   // LED v barve alba
#define FEAT_ART_CACHE        1   // cache obalu v LittleFS
#define FEAT_NTP_CLOCK        1

#define DEFAULT_LANG_EN       0   // 1 = startovat v anglictine
```

Dál se tam dá změnit rozložení obrazovky (souřadnice všech prvků),
intervaly dotazů, časové pásmo, jméno a heslo WiFi portálu.

---

### Vestavěná diagnostika

Čtyři přepínače v [`SpotifyDeck/DeckConfig.h`](SpotifyDeck/DeckConfig.h) tam jsou proto, že
každý z nich stál jedno skutečné ladění:

| Přepínač | K čemu je |
|---|---|
| `BL_SELFTEST` | Projede kandidáty na pin podsvícení, takže i z černé obrazovky poznáš, který pin tvoje deska používá. Hodnota = počet kol. |
| `TOUCH_PROBE` | Po startu prohledá I²C na kapacitní CST820 a SPI na rezistivní XPT2046 a napíše, kterou variantu máš. |
| `WIFI_FORGET` | Jednorázově zahodí uloženou WiFi a jde rovnou do portálu. **Pak to vrať na `0`**, jinak deska zapomene síť při každém restartu. |
| `DECK_VERBOSE` | Výpis na Serial (115200), včetně seznamu všech WiFi sítí v dosahu se silou signálu a kanálem. |

Deska taky hned po startu napíše, **proč se naposledy restartovala** (zapnutí, panic,
watchdog, podpětí, nebo vlastní `ESP.restart()`). Když se něco nečekaně rebootuje, tenhle
řádek ušetří spoustu hádání.


---

## Jak je to uvnitř

```
SpotifyDeck/
├─ SpotifyDeck.ino    přepínání stránek, stavový automat, dotyk
├─ DeckConfig.h       piny, přepínače funkcí, rozložení obrazovky
├─ secrets.h          klíče (v .gitignore, nikdy se necommituje)
│
├─ DeckHttp.*         jeden sdílený HTTP/HTTPS klient pro všechny moduly
├─ DeckSpotify.*      Spotify Web API - token, stav, ovládání, obal
├─ DeckWeather.*      Open-Meteo + geokódování
├─ DeckCrypto.*       Binance + denní kurz ČNB
├─ DeckClock.*        analogové hodiny ve spritu
│
├─ DeckArt.*          dekódování obalu, dominantní barva, rozmazané pozadí
├─ DeckDraw.*         sdílený sprite na text, paleta, skleněné panely
├─ DeckUi.*           obrazovky Spotify, stavový řádek, nastavení, podsvícení
├─ DeckIcons.*        všechny ikony kreslené z primitiv
├─ DeckLang.*        české a anglické texty prostředí
├─ DeckColor.h        barevná matematika (RGB565/888, HSV, míchání)
├─ DeckText.h         převod UTF-8 → ASCII
├─ DeckTouch.*        dotyk + kalibrace
├─ DeckNet.*          WiFi, konfigurační portál, NTP
└─ DeckTft.h          jedna globální instance displeje
```

Pár rozhodnutí, která stojí za vysvětlení:

**Jedna TLS relace pro všechno.** ESP32 bez PSRAM má po nabootování WiFi
kolem 250 kB volné haldy a jedno TLS spojení z toho ukousne 40–50 kB.
Kdyby měl každý modul vlastní `WiFiClientSecure`, došla by paměť. Proto je
v `DeckHttp` jeden klient pro všechny — včetně hlídání, že se znovupoužité
spojení nepošle na jiný server, což `HTTPClient` sám nekontroluje.

**Obaly alb po obyčejném HTTP.** `i.scdn.co` servíruje obrázky i bez TLS.
Je to veřejný obrázek, takže se tím nic neriskuje, a ušetří to celý TLS
buffer i handshake. Na desce bez PSRAM je to rozdíl mezi „jde to" a
„dochází paměť".

**JSON filtr.** Odpověď `/v1/me/player` má několik kB. Parametrem
`market=CZ` z ní zmizí 1,5 kB seznamu dostupných trhů a filtr ArduinoJsonu
nechá v paměti jen to, co se opravdu kreslí — asi 1 kB.

**Text přes sprite.** Každý řádek se poskládá mimo obrazovku (pozadí +
text) a teprve hotový se pošle na displej. Proto nad rozmazaným obalem
nic nebliká, i když se překresluje každou vteřinu.

**Rozptylování (dithering).** RGB565 má v zeleném kanálu jen 64 úrovní,
takže plynulý přechod přes celou výšku displeje se rozpadne do pruhů.
Bayerova matice 4×4 před zaokrouhlením je rozbije.

---

## Bezpečnost

- **`secrets.h` je v `.gitignore`.** Commituje se jen `secrets.example.h`.
  Kdyby ti refresh token přesto unikl, klikni v Spotify dashboardu na
  *Rotate client secret* — tím se starý token okamžitě zneplatní.
- **TLS certifikáty se neověřují** (`setInsecure()`). Na domácí síti je to
  běžný kompromis u těchhle zařízení; ušetří to flash i údržbu kořenových
  certifikátů. Kdo chce jistotu, nahradí to za `setCACert()` v
  `DeckHttp.cpp`.
- Obaly alb chodí po **plain HTTP** — jde o veřejná data, žádné přihlašovací
  údaje se tudy neposílají.
- **Refresh token od Spotify platí 6 měsíců.** Pak se musí získat nový.
  Deska na to sama upozorní obrazovkou „Spotify se odhlasilo".

---

## Omezení, o kterých je dobré vědět

- **Displej neumí diakritiku.** Vestavěné fonty TFT_eSPI i Adafruit FreeFonts
  obsahují jen znaky ASCII. Projekt proto všechny texty ze sítě převádí
  (`DeckText.h`) — „Dvořák" se zobrazí jako „Dvorak", ne jako dva obdélníky.
- Ovládání přehrávání **vyžaduje Spotify Premium**. Bez něj API vrací 403.
- Displej **nic nepřehrává**, jen zrcadlí to, co hraje jinde.
- Binance API může být v některých zemích nedostupné; stránka to napíše.

---

## Když něco nefunguje

| Problém | Řešení |
|---|---|
| Nahrávání se zasekne na „Connecting..." | Jiný (datový) kabel, nebo drž BOOT |
| V Nástroje → Port nic není | Kabel je jen nabíjecí, nebo chybí ovladač CH340 |
| Barvy jsou invertované | Máš desku se dvěma USB → v `User_Setup.h` přepni na blok B (ST7789) |
| Dotyk trefuje vedle | Dlouhý stisk → **Kalibrace dotyku** |
| Obraz je vzhůru nohama | Dlouhý stisk → **Otocit displej** |
| „Nic se nepřehrává", i když hraje | Spotify musí opravdu hrát na nějakém zařízení; displej jen zrcadlí |
| „Chybi Premium" | Ovládání přehrávání jde jen s Premium účtem |
| „Spotify se odhlasilo" | Refresh token vypršel (6 měsíců) → spusť znovu `tools/get_token.py` |
| Počasí hlásí „misto nenalezeno" | Napiš město bez diakritiky, případně i se zemí („Praha") |
| Po startu je bílá/černá obrazovka | Zkontroluj, že `User_Setup.h` je opravdu v `libraries/TFT_eSPI/` |
| Občas problikne šum | V `User_Setup.h` sniž `SPI_FREQUENCY` na `40000000` |
| **Černá obrazovka, ale deska naběhne** | Špatný pin podsvícení pro tvůj model. Nastav `BL_SELFTEST 3` v `DeckConfig.h`, nahraj a sleduj, u kterého pinu se rozsvítí |
| Barvy jsou vybledlé / invertované | Přehoď `TFT_INVERSION_ON` v `User_Setup.h` |
| Dotyk vůbec nereaguje | Nezkopírovaný `User_Setup.h`, nebo si `CYD_BOARD` a `DECK_BOARD` neodpovídají |
| Deska zapomene WiFi při každém restartu | `WIFI_FORGET` je pořád `1` v `DeckConfig.h` |

Sériový monitor na **115200 baud** vypisuje, co se děje — od WiFi přes
tokeny až po velikosti stažených obalů.

---

## Licence

MIT — viz [LICENSE](LICENSE).

Data: [Spotify Web API](https://developer.spotify.com/documentation/web-api),
[Open-Meteo](https://open-meteo.com/) (CC BY 4.0),
[Binance](https://binance-docs.github.io/apidocs/spot/en/),
[ČNB](https://www.cnb.cz/cs/financni-trhy/devizovy-trh/).
