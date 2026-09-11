// ---------------------------------------------------------------------
//  secrets.example.h
//
//  Copy this to "secrets.h" and fill it in. secrets.h is in .gitignore,
//  so it never makes it into the repository.
//
//  The easier way: run this from the project root
//      python tools/get_token.py
//  and secrets.h is generated for you, refresh token included.
// ---------------------------------------------------------------------
#pragma once

// developer.spotify.com/dashboard -> your app -> Settings
#define SPOTIFY_CLIENT_ID      "paste_client_id_here"
#define SPOTIFY_CLIENT_SECRET  "paste_client_secret_here"

// The result of the OAuth sign-in (tools/get_token.py prints it).
// Note: a Spotify refresh token is good for 6 months, after that you
// have to go through the sign-in again. The board warns you on screen.
#define SPOTIFY_REFRESH_TOKEN  "paste_refresh_token_here"

// Optional: two-letter country code (ISO 3166-1 alpha-2).
// With it Spotify stops sending the 1.5 kB list of available markets
// with every track - the response is roughly half the size. Keep "CZ"
// or change it.
#define SPOTIFY_MARKET         "CZ"
