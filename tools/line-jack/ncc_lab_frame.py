#!/usr/bin/env python3
"""Open CoinLine NCC and send one lab hello frame (Payphone-01 id).

Proves the listener stores a terminal-matched frame. The emulator management
dial opens the TCP session; firmware modem-data TX of NCC frames is a separate
milestone. This helper fills the framed-session gap for lab acceptance.
"""

from __future__ import annotations

import json
import os
import socket
import sys
import time
import urllib.request

FRAME = bytes.fromhex("02000D0102030405070102718B03")
HOST = os.environ.get("COINLINE_NCC_HOST") or "127.0.0.1"
PORT = int(os.environ.get("COINLINE_NCC_PORT") or "4000")
API = os.environ.get("COINLINE_API") or "http://127.0.0.1:8080"


def main() -> int:
	before = json.load(urllib.request.urlopen(f"{API}/api/ncc/sessions"))[0]
	sock = socket.create_connection((HOST, PORT), 5)
	sock.sendall(FRAME)
	time.sleep(0.8)
	sock.close()
	time.sleep(0.4)
	after = json.load(urllib.request.urlopen(f"{API}/api/ncc/sessions"))[0]
	detail = json.load(urllib.request.urlopen(f"{API}/api/ncc/sessions/{after['id']}"))
	print(
		json.dumps(
			{
				"new_session": after["correlationId"] != before["correlationId"],
				"correlationId": after["correlationId"],
				"lastFrameSampleHex": after.get("lastFrameSampleHex"),
				"terminalId": after.get("terminalId"),
				"frames": detail.get("frames"),
			},
			indent=2,
		)
	)
	ok = bool(after.get("lastFrameSampleHex")) and bool(detail.get("frames"))
	return 0 if ok else 1


if __name__ == "__main__":
	sys.exit(main())
