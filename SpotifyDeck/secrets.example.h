// ---------------------------------------------------------------------
//  secrets.example.h
//
//  Zkopiruj na "secrets.h" a vypln. secrets.h je v .gitignore, takze
//  se nikdy nedostane do repozitare.
//
//  Jednodussi cesta: spust z korene projektu
//      python tools/get_token.py
//  a secrets.h se vygeneruje sam vcetne refresh tokenu.
// ---------------------------------------------------------------------
#pragma once

// developer.spotify.com/dashboard -> tvoje app -> Settings
#define SPOTIFY_CLIENT_ID      "sem_vloz_client_id"
#define SPOTIFY_CLIENT_SECRET  "sem_vloz_client_secret"

// Vysledek OAuth prihlaseni (tools/get_token.py ho vypise).
// Pozor: refresh token od Spotify plati 6 mesicu, pak je potreba
// projit prihlasenim znovu. Deska te na to upozorni na obrazovce.
#define SPOTIFY_REFRESH_TOKEN  "sem_vloz_refresh_token"

// Volitelne: dvoupismenny kod zeme (ISO 3166-1 alpha-2).
// Diky nemu Spotify neposila 1,5 kB seznamu dostupnych trhu u kazde
// skladby - odpoved je zhruba o polovinu mensi. Nech "CZ" nebo zmen.
#define SPOTIFY_MARKET         "CZ"
