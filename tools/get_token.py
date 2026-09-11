#!/usr/bin/env python3
"""
SpotifyDeck - get a refresh token (in a single command).

Replaces hand-assembling an authorize URL plus curl. The script:
  1. starts a small local server on http://127.0.0.1:8888/callback
  2. opens a browser with the Spotify sign-in
  3. catches the ?code=... that comes back
  4. exchanges it for an access + refresh token
  5. optionally writes SpotifyDeck/secrets.h for you

Usage:
    python tools/get_token.py
    python tools/get_token.py --client-id XXX --client-secret YYY
    python tools/get_token.py --port 8888 --no-write

No external dependencies - just the Python 3.8+ standard library.
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

# Exactly the scopes the firmware needs - nothing more.
SCOPES = [
    "user-read-playback-state",     # what plays, volume, shuffle/repeat, devices
    "user-modify-playback-state",   # play/pause/next/prev/seek/volume/shuffle/repeat
    "user-read-currently-playing",  # the current track
]

AUTH_URL = "https://accounts.spotify.com/authorize"
TOKEN_URL = "https://accounts.spotify.com/api/token"

REPO_ROOT = Path(__file__).resolve().parent.parent
SECRETS_PATH = REPO_ROOT / "SpotifyDeck" / "secrets.h"

# --------------------------------------------------------------------------
# The HTML the user sees in the browser on the way back from Spotify
# --------------------------------------------------------------------------

PAGE = """<!doctype html><html lang="en"><head><meta charset="utf-8">
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
# The local callback server
# --------------------------------------------------------------------------

class CallbackHandler(http.server.BaseHTTPRequestHandler):
    """Catches the one /callback request and stores its query parameters."""

    # filled in before the server starts
    expected_state: str = ""
    result: dict[str, str] = {}
    done: threading.Event

    def do_GET(self) -> None:  # noqa: N802 (the name is dictated by BaseHTTPRequestHandler)
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path not in ("/callback", "/callback/"):
            self.send_response(404)
            self.end_headers()
            return

        query = {k: v[0] for k, v in urllib.parse.parse_qs(parsed.query).items()}

        if query.get("state") != self.expected_state:
            self._respond(400, page(False, "Invalid state",
                                    "This response does not match the sign-in "
                                    "that was started. Run the script again."))
            type(self).result = {"error": "state_mismatch"}
        elif "error" in query:
            self._respond(400, page(False, "Spotify refused the sign-in",
                                    f"Reason: {query['error']}"))
            type(self).result = {"error": query["error"]}
        elif "code" in query:
            self._respond(200, page(True, "All done",
                                    "You can close this window and go back to "
                                    "the terminal."))
            type(self).result = {"code": query["code"]}
        else:
            self._respond(400, page(False, "No code",
                                    "There was no ?code= in the response."))
            type(self).result = {"error": "no_code"}

        type(self).done.set()

    def _respond(self, status: int, body: bytes) -> None:
        self.send_response(status)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *_args) -> None:
        """Silence - the built-in stderr logging only gets in the way here."""


# --------------------------------------------------------------------------
# Helper functions
# --------------------------------------------------------------------------

def die(msg: str) -> "NoReturn":  # type: ignore[valid-type]
    print(f"\n  ERROR: {msg}\n", file=sys.stderr)
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
    """POST to /api/token with Basic auth. Returns the parsed JSON."""
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
        die(f"Spotify returned HTTP {e.code}.\n  Response: {detail}\n\n"
            "  The most common causes:\n"
            "    - the Redirect URI in the app on developer.spotify.com does not match EXACTLY\n"
            "    - the client secret was copied over wrong\n"
            "    - the code from the browser has expired (it lasts ~10 minutes)")
    except urllib.error.URLError as e:
        die(f"Could not reach Spotify: {e.reason}")


def write_secrets(path: Path, client_id: str, client_secret: str,
                  refresh_token: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)

    if path.exists():
        answer = input(f"\n  {path} already exists. Overwrite? [y/N] ").strip().lower()
        if answer not in ("y", "yes"):
            print("  Left unchanged.")
            return
        backup = path.with_suffix(".h.bak")
        backup.write_bytes(path.read_bytes())
        print(f"  Backup: {backup}")

    path.write_text(
        "// ---------------------------------------------------------------\n"
        "//  secrets.h  -  generated by tools/get_token.py\n"
        "//\n"
        "//  NEVER commit this file. It is in .gitignore.\n"
        "//  If the refresh token ever leaks, go to the Spotify dashboard and\n"
        "//  hit \"Rotate client secret\" - that invalidates the old one.\n"
        "// ---------------------------------------------------------------\n"
        "#pragma once\n\n"
        f'#define SPOTIFY_CLIENT_ID      "{client_id}"\n'
        f'#define SPOTIFY_CLIENT_SECRET  "{client_secret}"\n'
        f'#define SPOTIFY_REFRESH_TOKEN  "{refresh_token}"\n',
        encoding="utf-8",
    )
    print(f"  Written: {path}")


# --------------------------------------------------------------------------
# Main flow
# --------------------------------------------------------------------------

def main() -> None:
    ap = argparse.ArgumentParser(
        description="Gets a Spotify refresh token for SpotifyDeck.")
    ap.add_argument("--client-id", default=os.environ.get("SPOTIFY_CLIENT_ID"))
    ap.add_argument("--client-secret", default=os.environ.get("SPOTIFY_CLIENT_SECRET"))
    ap.add_argument("--port", type=int, default=8888,
                    help="port of the local callback (must match the Redirect URI)")
    ap.add_argument("--no-write", action="store_true",
                    help="only print the token, do not write secrets.h")
    args = ap.parse_args()

    print("\n  SpotifyDeck - getting a refresh token")
    print("  " + "-" * 44)

    client_id = (args.client_id or input("\n  Client ID:     ").strip())
    if not client_id:
        die("Client ID is required.")

    client_secret = (args.client_secret
                     or getpass.getpass("  Client secret: ").strip())
    if not client_secret:
        die("Client secret is required.")

    redirect_uri = f"http://127.0.0.1:{args.port}/callback"

    if not port_is_free(args.port):
        die(f"Port {args.port} is busy. Free it up, or use --port and set the "
            "same number in the Redirect URI on developer.spotify.com.")

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
    print("  (this exact value has to be saved in the app on developer.spotify.com)")
    print("\n  Opening the browser... if it does not open, copy this address:\n")
    print(f"  {auth_url}\n")
    webbrowser.open(auth_url)

    print("  Waiting for Spotify to come back (5 minutes max)...", flush=True)
    if not CallbackHandler.done.wait(timeout=300):
        server.shutdown()
        die("Timed out waiting. Run the script again.")

    server.shutdown()

    if "error" in CallbackHandler.result:
        die(f"Sign-in failed: {CallbackHandler.result['error']}")

    print("  Code received, exchanging it for a token...")

    tokens = post_token({
        "grant_type": "authorization_code",
        "code": CallbackHandler.result["code"],
        "redirect_uri": redirect_uri,
    }, client_id, client_secret)

    refresh_token = tokens.get("refresh_token")
    if not refresh_token:
        die(f"The response contains no refresh_token:\n  {json.dumps(tokens, indent=2)}")

    granted = tokens.get("scope", "")
    missing = [s for s in SCOPES if s not in granted]

    print("\n  " + "-" * 44)
    print("  Refresh token (valid until you revoke it):\n")
    print(f"  {refresh_token}\n")
    print("  " + "-" * 44)
    if missing:
        print(f"\n  WARNING: missing scope: {', '.join(missing)}")
        print("  Playback control will not work. Run the script again"
              " and approve every permission.")

    if args.no_write:
        print("\n  (--no-write) secrets.h was not written.\n")
        return

    write_secrets(SECRETS_PATH, client_id, client_secret, refresh_token)
    print("\n  Done. Now just flash the sketch onto the board.\n")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n  Interrupted.\n")
        sys.exit(130)
