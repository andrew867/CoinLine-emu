#!/usr/bin/env python3
"""Send a loopback INVITE to the central-office SIP UA so it rings the jack.

Usage (CO already running with registrar SIP on COINLINE_SIP_LOCAL_PORT):
  COINLINE_SIP_RING_PORT=5070 python tools/line-jack/sip_invite_ring.py
"""

from __future__ import annotations

import os
import socket
import time


def main() -> None:
	port = int(os.environ.get("COINLINE_SIP_RING_PORT") or "5070")
	sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
	sock.bind(("127.0.0.1", 0))
	local = sock.getsockname()[1]
	sock.settimeout(0.3)
	sdp = (
		"v=0\r\n"
		"o=- 1 1 IN IP4 127.0.0.1\r\n"
		"s=ring\r\n"
		"c=IN IP4 127.0.0.1\r\n"
		"t=0 0\r\n"
		f"m=audio {local + 2} RTP/AVP 0\r\n"
		"a=rtpmap:0 PCMU/8000\r\n"
	)
	invite = (
		f"INVITE sip:payphone@127.0.0.1:{port} SIP/2.0\r\n"
		f"Via: SIP/2.0/UDP 127.0.0.1:{local};branch=z9hG4bKring{local};rport\r\n"
		"From: <sip:lab@127.0.0.1>;tag=ring\r\n"
		f"To: <sip:payphone@127.0.0.1:{port}>\r\n"
		f"Call-ID: ring-{local}@127.0.0.1\r\n"
		"CSeq: 1 INVITE\r\n"
		f"Contact: <sip:lab@127.0.0.1:{local}>\r\n"
		"Content-Type: application/sdp\r\n"
		"Max-Forwards: 70\r\n"
		f"Content-Length: {len(sdp)}\r\n\r\n"
		f"{sdp}"
	)
	print(f"invite 127.0.0.1:{port}", flush=True)
	sock.sendto(invite.encode("ascii"), ("127.0.0.1", port))
	deadline = time.time() + 15
	buf = ""
	while time.time() < deadline:
		try:
			data, _ = sock.recvfrom(65535)
			text = data.decode("ascii", "replace")
			buf += text
			print(text.split("\r\n", 1)[0], flush=True)
			if "SIP/2.0 200" in buf or "SIP/2.0 180" in buf or "SIP/2.0 183" in buf:
				break
		except socket.timeout:
			pass
	print("done", flush=True)
	sock.close()


if __name__ == "__main__":
	main()
