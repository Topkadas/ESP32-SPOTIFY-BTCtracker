#!/usr/bin/env python3
"""
SpotifyDeck - ziskani refresh tokenu (jedinym prikazem).

Nahrazuje rucni skladani authorize URL + curl. Skript:
  1. spusti maly lokalni server na http://127.0.0.1:8888/callback
  2. otevre prohlizec se Spotify prihlasenim
  3. zachyti navratovy ?code=...
  4. vymeni ho za access + refresh token
  5. volitelne rovnou zapise SpotifyDeck/secrets.h

Pouziti:
    python tools/get_token.py
    python tools/get_token.py --client-id XXX --client-secret YYY
    python tools/get_token.py --port 8888 --no-write

Zadne externi zavislosti - jen standardni knihovna Pythonu 3.8+.
"""

from __future__ import annotations

import argparse
import base64
import getpass
import http.server
import json
import os
import secrets as pysecrets
import socket
import sys
import threading
import urllib.error
import urllib.parse
import urllib.request
import webbrowser
from pathlib import Path

# Presne ty scopes, ktere firmware potrebuje - nic navic.
SCOPES = [
    "user-read-playback-state",     # co hraje, hlasitost, shuffle/repeat, zarizeni
    "user-modify-playback-state",   # play/pause/next/prev/seek/volume/shuffle/repeat
    "user-read-currently-playing",  # aktualni skladba
]

AUTH_URL = "https://accounts.spotify.com/authorize"
TOKEN_URL = "https://accounts.spotify.com/api/token"

REPO_ROOT = Path(__file__).resolve().parent.parent
SECRETS_PATH = REPO_ROOT / "SpotifyDeck" / "secrets.h"

# --------------------------------------------------------------------------
# HTML, ktere uvidi uzivatel v prohlizeci po navratu ze Spotify
# --------------------------------------------------------------------------

PAGE = """<!doctype html><html lang="cs"><head><meta charset="utf-8">
<title>SpotifyDeck</title><style>
*{{box-sizing:border-box}}
body{{margin:0;min-height:100vh;display:grid;place-items:center;
  background:#0b1220;color:#f3f6fc;
  font:16px/1.6 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif}}
.card{{max-width:440px;padding:40px;text-align:center;background:#131d33;
  border:1px solid #2a3a5c;border-radius:20px}}
.ok{{color:#1ed760}} .bad{{color:#ff6b6b}}
h1{{margin:0 0 12px;font-size:24px}} p{{margin:0;color:#8b96ae}}
.mark{{font-size:56px;line-height:1;margin-bottom:16px}}
</style></head><body><div class="card">
<div class="mark {cls}">{mark}</div><h1>{title}</h1><p>{body}</p>
</div></body></html>"""


def page(ok: bool, title: str, body: str) -> bytes:
    return PAGE.format(
        cls="ok" if ok else "bad",
        mark="&#10003;" if ok else "&#10007;",
        title=title,
        body=body,
    ).encode("utf-8")


# --------------------------------------------------------------------------
# Lokalni callback server
# --------------------------------------------------------------------------

class CallbackHandler(http.server.BaseHTTPRequestHandler):
    """Zachyti jediny pozadavek na /callback a ulozi query parametry."""

    # naplni se pred spustenim serveru
    expected_state: str = ""
    result: dict[str, str] = {}
    done: threading.Event

    def do_GET(self) -> None:  # noqa: N802 (jmeno urcuje BaseHTTPRequestHandler)
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path not in ("/callback", "/callback/"):
            self.send_response(404)
            self.end_headers()
            return

        query = {k: v[0] for k, v in urllib.parse.parse_qs(parsed.query).items()}

        if query.get("state") != self.expected_state:
            self._respond(400, page(False, "Neplatny state",
                                    "Odpoved neodpovida zahajenemu prihlaseni. "
                                    "Spust skript znovu."))
            type(self).result = {"error": "state_mismatch"}
        elif "error" in query:
            self._respond(400, page(False, "Spotify odmitlo prihlaseni",
                                    f"Duvod: {query['error']}"))
            type(self).result = {"error": query["error"]}
        elif "code" in query:
            self._respond(200, page(True, "Hotovo",
                                    "Muzes zavrit okno a vratit se do terminalu."))
            type(self).result = {"code": query["code"]}
        else:
            self._respond(400, page(False, "Chybi kod",
                                    "V odpovedi nebyl zadny ?code=."))
            type(self).result = {"error": "no_code"}

        type(self).done.set()

    def _respond(self, status: int, body: bytes) -> None:
        self.send_response(status)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *_args) -> None:
        """Ticho - vestavene logovani do stderr tu jen zavazi."""


# --------------------------------------------------------------------------
# Pomocne funkce
# --------------------------------------------------------------------------

def die(msg: str) -> "NoReturn":  # type: ignore[valid-type]
    print(f"\n  CHYBA: {msg}\n", file=sys.stderr)
    sys.exit(1)


def port_is_free(port: int) -> bool:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            s.bind(("127.0.0.1", port))
            return True
        except OSError:
            return False


def post_token(payload: dict[str, str], client_id: str, client_secret: str) -> dict:
    """POST na /api/token s Basic autorizaci. Vraci rozparsovany JSON."""
    basic = base64.b64encode(f"{client_id}:{client_secret}".encode()).decode()
    req = urllib.request.Request(
        TOKEN_URL,
        data=urllib.parse.urlencode(payload).encode(),
        headers={
            "Authorization": f"Basic {basic}",
            "Content-Type": "application/x-www-form-urlencoded",
        },
        method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            return json.loads(resp.read().decode())
    except urllib.error.HTTPError as e:
        detail = e.read().decode(errors="replace")
        die(f"Spotify vratilo HTTP {e.code}.\n  Odpoved: {detail}\n\n"
            "  Nejcastejsi priciny:\n"
            "    - Redirect URI v aplikaci na developer.spotify.com nesedi PRESNE\n"
            "    - Client secret je prekopirovany spatne\n"
            "    - Kod z prohlizece uz vyprsel (plati ~10 minut)")
    except urllib.error.URLError as e:
        die(f"Nepodarilo se spojit se Spotify: {e.reason}")


def write_secrets(path: Path, client_id: str, client_secret: str,
                  refresh_token: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)

    if path.exists():
        answer = input(f"\n  {path} uz existuje. Prepsat? [a/N] ").strip().lower()
        if answer not in ("a", "ano", "y", "yes"):
            print("  Ponechano beze zmeny.")
            return
        backup = path.with_suffix(".h.bak")
        backup.write_bytes(path.read_bytes())
        print(f"  Zaloha: {backup}")

    path.write_text(
        "// ---------------------------------------------------------------\n"
        "//  secrets.h  -  vygenerovano nastrojem tools/get_token.py\n"
        "//\n"
        "//  NIKDY tento soubor necommituj. Je v .gitignore.\n"
        "//  Kdyz refresh token unikne, okamzite v Spotify dashboardu\n"
        "//  klikni na \"Rotate client secret\" - tim se stary token zneplatni.\n"
        "// ---------------------------------------------------------------\n"
        "#pragma once\n\n"
        f'#define SPOTIFY_CLIENT_ID      "{client_id}"\n'
        f'#define SPOTIFY_CLIENT_SECRET  "{client_secret}"\n'
        f'#define SPOTIFY_REFRESH_TOKEN  "{refresh_token}"\n',
        encoding="utf-8",
    )
    print(f"  Zapsano: {path}")


# --------------------------------------------------------------------------
# Hlavni tok
# --------------------------------------------------------------------------

def main() -> None:
    ap = argparse.ArgumentParser(
        description="Ziska Spotify refresh token pro SpotifyDeck.")
    ap.add_argument("--client-id", default=os.environ.get("SPOTIFY_CLIENT_ID"))
    ap.add_argument("--client-secret", default=os.environ.get("SPOTIFY_CLIENT_SECRET"))
    ap.add_argument("--port", type=int, default=8888,
                    help="port lokalniho callbacku (musi sedet s Redirect URI)")
    ap.add_argument("--no-write", action="store_true",
                    help="jen vypsat token, nezapisovat secrets.h")
    args = ap.parse_args()

    print("\n  SpotifyDeck - ziskani refresh tokenu")
    print("  " + "-" * 44)

    client_id = (args.client_id or input("\n  Client ID:     ").strip())
    if not client_id:
        die("Client ID je povinne.")

    client_secret = (args.client_secret
                     or getpass.getpass("  Client secret: ").strip())
    if not client_secret:
        die("Client secret je povinny.")

    redirect_uri = f"http://127.0.0.1:{args.port}/callback"

    if not port_is_free(args.port):
        die(f"Port {args.port} je obsazeny. Uvolni ho, nebo pouzij --port a "
            "stejne cislo nastav i v Redirect URI na developer.spotify.com.")

    state = pysecrets.token_urlsafe(24)
    CallbackHandler.expected_state = state
    CallbackHandler.result = {}
    CallbackHandler.done = threading.Event()

    server = http.server.HTTPServer(("127.0.0.1", args.port), CallbackHandler)
    threading.Thread(target=server.serve_forever, daemon=True).start()

    auth_url = AUTH_URL + "?" + urllib.parse.urlencode({
        "client_id": client_id,
        "response_type": "code",
        "redirect_uri": redirect_uri,
        "scope": " ".join(SCOPES),
        "state": state,
        "show_dialog": "true",
    })

    print(f"\n  Redirect URI:  {redirect_uri}")
    print("  (presne tohle musi byt ulozene v aplikaci na developer.spotify.com)")
    print("\n  Otevirám prohlizec... pokud se neotevre, zkopiruj tuhle adresu:\n")
    print(f"  {auth_url}\n")
    webbrowser.open(auth_url)

    print("  Cekam na navrat ze Spotify (max 5 minut)...", flush=True)
    if not CallbackHandler.done.wait(timeout=300):
        server.shutdown()
        die("Vyprsel cas cekani. Spust skript znovu.")

    server.shutdown()

    if "error" in CallbackHandler.result:
        die(f"Prihlaseni se nezdarilo: {CallbackHandler.result['error']}")

    print("  Kod prijat, vymenuji za token...")

    tokens = post_token({
        "grant_type": "authorization_code",
        "code": CallbackHandler.result["code"],
        "redirect_uri": redirect_uri,
    }, client_id, client_secret)

    refresh_token = tokens.get("refresh_token")
    if not refresh_token:
        die(f"Odpoved neobsahuje refresh_token:\n  {json.dumps(tokens, indent=2)}")

    granted = tokens.get("scope", "")
    missing = [s for s in SCOPES if s not in granted]

    print("\n  " + "-" * 44)
    print("  Refresh token (plati, dokud ho nezrusis):\n")
    print(f"  {refresh_token}\n")
    print("  " + "-" * 44)
    if missing:
        print(f"\n  POZOR: chybi scope: {', '.join(missing)}")
        print("  Ovladani prehravani nebude fungovat. Spust skript znovu"
              " a schval vsechna opravneni.")

    if args.no_write:
        print("\n  (--no-write) secrets.h nebyl zapsan.\n")
        return

    write_secrets(SECRETS_PATH, client_id, client_secret, refresh_token)
    print("\n  Hotovo. Ted uz jen nahraj sketch do desky.\n")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n  Preruseno.\n")
        sys.exit(130)
