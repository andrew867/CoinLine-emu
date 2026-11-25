#!/usr/bin/env python3
"""Central office on the emulator line jack.

Speaks the jack protocol (R/C/V/W in, H/E/S/X/D/M out). Provides dial tone
after seize, routes collected digits and applies a polarity reversal on answer,
rings inbound until the handset lifts, and bridges 8 kHz PCM both ways.

Optional SIP (env, never hard-coded secrets):
  COINLINE_SIP_HOST, COINLINE_SIP_USER, COINLINE_SIP_PASSWORD
  COINLINE_SIP_PORT (registrar port, default 5060)
  COINLINE_SIP_LOCAL_PORT  local UDP bind for this UA (default 5070; avoid
                           host 5060 when CoinLine SipModem already owns it)
  COINLINE_SIP_MY_IP  local UDP bind address
  COINLINE_SIP_PUBLIC_IP  optional public IPv4 address advertised in SDP
  COINLINE_SIP_RTP_PORT  optional fixed local UDP media port
  COINLINE_OUTBOUND  expected digits for local and fixed-target tests (default 5551234)
  COINLINE_SIP_DEST   optional fixed target for a matched dial in a lab test.
                     Use "local" to INVITE the host SipModem (COINLINE_SIP_PEER,
                     default 127.0.0.1:5060) without the registrar.
  COINLINE_SIP_PEER  host:port for local SipModem INVITE (default 127.0.0.1:5060)
  COINLINE_LINE_PORT  jack port (default 4010)
  COINLINE_CO_MODE    auto|local|sip  (default auto: sip if credentials present)
  COINLINE_CO_TIMEOUT  session timeout in seconds (default 180, 0 keeps the office running)
  COINLINE_CO_INBOUND=1  ring after firmware E1 (local or SIP)
"""

from __future__ import annotations

import hashlib
import ipaddress
import json
import math
import os
import random
import re
import secrets
import select
import socket
import struct
import threading
import time
from collections import deque
from dataclasses import dataclass
from urllib.request import parse_http_list, parse_keqv_list
from sip_media import AudioOffer, RtpWireSocket, SECURE_PROFILES, SdesSessions, sdes_offer, sdp_answer

HOST = "127.0.0.1"
PORT = int(os.environ.get("COINLINE_LINE_PORT", "4010"))
EXPECT = os.environ.get("COINLINE_OUTBOUND") or "5551234"
SIP_DEST = os.environ.get("COINLINE_SIP_DEST") or ""
SIP_PEER = os.environ.get("COINLINE_SIP_PEER") or "127.0.0.1:5060"
MODE = (os.environ.get("COINLINE_CO_MODE") or "auto").strip().lower()


NCC_LOOPBACK = str(int.from_bytes(socket.inet_aton(HOST), "big"))


@dataclass(frozen=True)
class LocalVoiceRoute:
	digits: tuple[str, ...]
	target: str
	port: int
	from_user: str = "co"

	@classmethod
	def parse(cls, raw: str) -> LocalVoiceRoute | None:
		if not raw:
			return None
		value = json.loads(raw)
		if not isinstance(value, dict):
			raise ValueError("Invalid exact loopback voice route")
		digits = value.get("digits")
		target = value.get("target")
		port = value.get("port")
		if (not isinstance(digits, list) or not digits
				or not all(isinstance(d, str) and re.fullmatch(r"[0-9]{1,15}", d) and d != NCC_LOOPBACK for d in digits)
				or len(set(digits)) != len(digits)
				or not isinstance(target, str) or not re.fullmatch(r"[0-9]{1,15}", target)
				or type(port) is not int or not 1 <= port <= 65535):
			raise ValueError("Invalid exact loopback voice route")
		return cls(tuple(digits), target, port)

	def matches(self, digits: str) -> bool:
		return digits in self.digits


def env_mode() -> str:
	host = os.environ.get("COINLINE_SIP_HOST") or ""
	user = os.environ.get("COINLINE_SIP_USER") or ""
	password = os.environ.get("COINLINE_SIP_PASSWORD") or ""
	if MODE == "local":
		return "local"
	if MODE == "sip":
		return "sip"
	if SIP_DEST.strip().lower() == "local" or os.environ.get("COINLINE_SIP_PEER_ONLY") == "1":
		return "sip"
	if host and user and password:
		return "sip"
	return "local"


# Known lab terminal id used by CoinLine seed data (Payphone-01).
NCC_LAB_FRAME = bytes.fromhex("02000D0102030405070102718B03")


class Bell202Tx:
	"""Bell 202 at 1200 baud → 8 kHz float samples (mark 1200 / space 2200)."""

	def __init__(self) -> None:
		self._bits: deque[bool] = deque()
		self._phase = 0.0
		self._bit_left = 0.0
		self._mark = True
		self._primed = False

	def queue(self, data: bytes) -> None:
		if not self._primed:
			self._bits.extend([True] * 48)
			self._primed = True
		for value in data:
			self._bits.append(False)
			for bit in range(8):
				self._bits.append(((value >> bit) & 1) == 1)
			self._bits.append(True)

	@property
	def pending(self) -> bool:
		return bool(self._bits)

	def render(self, n: int) -> list[int]:
		out: list[int] = []
		for _ in range(n):
			if self._bit_left <= 0:
				if self._bits:
					self._mark = self._bits.popleft()
					self._bit_left += 8000.0 / 1200.0
				else:
					self._mark = True
					self._bit_left = 0.0
			freq = 1200.0 if self._mark else 2200.0
			self._phase += 2 * math.pi * freq / 8000.0
			if self._phase > 2 * math.pi:
				self._phase -= 2 * math.pi
			out.append(int(0.45 * 32767.0 * math.sin(self._phase)))
			if self._bit_left > 0:
				self._bit_left -= 1.0
		return out


def mulaw_encode(sample: int) -> int:
	sample = max(-32768, min(32767, int(sample)))
	sign = 0x80 if sample < 0 else 0
	if sample < 0:
		sample = -sample
	sample = min(sample, 32635) + 0x84
	exponent = 7
	mask = 0x4000
	while exponent > 0 and (sample & mask) == 0:
		exponent -= 1
		mask >>= 1
	mantissa = (sample >> (exponent + 3)) & 0x0F
	return (~(sign | (exponent << 4) | mantissa)) & 0xFF


def mulaw_decode(byte: int) -> int:
	byte = (~byte) & 0xFF
	sign = byte & 0x80
	exponent = (byte >> 4) & 0x07
	mantissa = byte & 0x0F
	sample = ((mantissa << 3) + 0x84) << exponent
	sample -= 0x84
	return -sample if sign else sample


class LocalSipPeer:
	"""Minimal INVITE/ACK/BYE + G.711 RTP toward a loopback SipModem."""

	def __init__(self, pcm: PcmBridge) -> None:
		self.pcm = pcm
		self._stop = threading.Event()
		self._thread: threading.Thread | None = None
		self._sock: socket.socket | None = None
		self._rtp_remote: tuple[str, int] | None = None
		self._call_id = ""
		self._from_tag = "co"
		self._to_tag = ""
		self._cseq = 1
		self._local_port = 0
		self._peer_host = "127.0.0.1"
		self._peer_port = 5060
		self._bell = Bell202Tx()
		# After answer, emit one NCC lab frame as Bell 202 so SipModem → NCC stores it.
		if os.environ.get("COINLINE_SIP_LAB_FRAME", "1") != "0":
			self._bell.queue(NCC_LAB_FRAME)
			# Trailing mark bits (matches SipModemLabProbe padding).
			self._bell._bits.extend([True] * 120)

	def dial(self) -> bool:
		host, _, port_s = SIP_PEER.partition(":")
		self._peer_host = host or "127.0.0.1"
		self._peer_port = int(port_s or "5060")
		sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
		sock.bind(("127.0.0.1", 0))
		sock.settimeout(0.2)
		self._sock = sock
		self._local_port = sock.getsockname()[1]
		rtp_port = self._local_port + 2
		self._call_id = f"co-{self._local_port}-{random.randint(1, 1_000_000)}@127.0.0.1"
		sdp = (
			"v=0\r\n"
			"o=- 1 1 IN IP4 127.0.0.1\r\n"
			"s=co\r\n"
			"c=IN IP4 127.0.0.1\r\n"
			"t=0 0\r\n"
			f"m=audio {rtp_port} RTP/AVP 0\r\n"
			"a=rtpmap:0 PCMU/8000\r\n"
		)
		invite = (
			f"INVITE sip:ncc@{self._peer_host} SIP/2.0\r\n"
			f"Via: SIP/2.0/UDP 127.0.0.1:{self._local_port};branch=z9hG4bK{self._local_port};rport\r\n"
			f"From: <sip:co@127.0.0.1>;tag={self._from_tag}\r\n"
			f"To: <sip:ncc@{self._peer_host}>\r\n"
			f"Call-ID: {self._call_id}\r\n"
			f"CSeq: {self._cseq} INVITE\r\n"
			f"Contact: <sip:co@127.0.0.1:{self._local_port}>\r\n"
			"Content-Type: application/sdp\r\n"
			"Max-Forwards: 70\r\n"
			f"Content-Length: {len(sdp)}\r\n\r\n"
			f"{sdp}"
		)
		print(f"sip local invite {self._peer_host}:{self._peer_port}", flush=True)
		sock.sendto(invite.encode("ascii"), (self._peer_host, self._peer_port))
		deadline = time.time() + 8
		buf = ""
		while time.time() < deadline:
			try:
				data, _ = sock.recvfrom(65535)
				buf += data.decode("ascii", "replace")
			except socket.timeout:
				pass
			if "SIP/2.0 200" in buf:
				break
			if "SIP/2.0 4" in buf or "SIP/2.0 5" in buf or "SIP/2.0 6" in buf:
				print("sip local invite failed", flush=True)
				return False
		if "SIP/2.0 200" not in buf:
			print("sip local invite timeout", flush=True)
			return False
		for line in buf.split("\r\n"):
			if line.lower().startswith("to:") and "tag=" in line:
				self._to_tag = line.split("tag=", 1)[1].strip()
			if line.startswith("m=audio "):
				parts = line.split()
				if len(parts) >= 2:
					self._rtp_remote = (self._peer_host, int(parts[1]))
		ack = (
			f"ACK sip:ncc@{self._peer_host} SIP/2.0\r\n"
			f"Via: SIP/2.0/UDP 127.0.0.1:{self._local_port};branch=z9hG4bKack{self._local_port};rport\r\n"
			f"From: <sip:co@127.0.0.1>;tag={self._from_tag}\r\n"
			f"To: <sip:ncc@{self._peer_host}>;tag={self._to_tag}\r\n"
			f"Call-ID: {self._call_id}\r\n"
			f"CSeq: {self._cseq} ACK\r\n"
			"Max-Forwards: 70\r\n"
			"Content-Length: 0\r\n\r\n"
		)
		sock.sendto(ack.encode("ascii"), (self._peer_host, self._peer_port))
		print("sip local answered", flush=True)
		self._thread = threading.Thread(target=self._pump, name="local-sip-rtp", daemon=True)
		self._thread.start()
		return True

	def stop(self) -> None:
		self._stop.set()
		if self._sock is not None and self._call_id:
			self._cseq += 1
			bye = (
				f"BYE sip:ncc@{self._peer_host} SIP/2.0\r\n"
				f"Via: SIP/2.0/UDP 127.0.0.1:{self._local_port};branch=z9hG4bKbye{self._local_port};rport\r\n"
				f"From: <sip:co@127.0.0.1>;tag={self._from_tag}\r\n"
				f"To: <sip:ncc@{self._peer_host}>;tag={self._to_tag}\r\n"
				f"Call-ID: {self._call_id}\r\n"
				f"CSeq: {self._cseq} BYE\r\n"
				"Max-Forwards: 70\r\n"
				"Content-Length: 0\r\n\r\n"
			)
			try:
				self._sock.sendto(bye.encode("ascii"), (self._peer_host, self._peer_port))
			except OSError:
				pass
		try:
			if self._sock is not None:
				self._sock.close()
		except OSError:
			pass

	def _pump(self) -> None:
		assert self._sock is not None
		seq = 1
		stamp = 0
		ssrc = 0xC0111E01
		while not self._stop.is_set():
			# Drain SIP/RTP from the peer socket.
			try:
				while True:
					data, addr = self._sock.recvfrom(65535)
					if len(data) >= 12 and data[0] == 0x80:
						payload = data[12:]
						pcm = b"".join(struct.pack("<h", mulaw_decode(b)) for b in payload)
						self.pcm.push_to_phone(pcm)
					elif data.startswith(b"SIP/"):
						pass
			except socket.timeout:
				pass
			except OSError:
				break
			if self._bell.pending:
				linear = self._bell.render(160)
			else:
				raw = self.pcm.take_from_phone(160)
				linear = [struct.unpack_from("<h", raw, i)[0] for i in range(0, len(raw), 2)]
			if self._rtp_remote is not None:
				payload = bytes(mulaw_encode(s) for s in linear)
				packet = bytearray(12 + len(payload))
				packet[0] = 0x80
				packet[1] = 0
				packet[2] = (seq >> 8) & 0xFF
				packet[3] = seq & 0xFF
				packet[4:8] = stamp.to_bytes(4, "big")
				packet[8:12] = ssrc.to_bytes(4, "big")
				packet[12:] = payload
				try:
					self._sock.sendto(packet, self._rtp_remote)
				except OSError:
					break
				seq = (seq + 1) & 0xFFFF
				stamp = (stamp + 160) & 0xFFFFFFFF
			time.sleep(0.018)


def patch_pyvoip_not_found() -> None:
	"""pyVoIP raises KeyError when a 404 Call-ID is missing from phone.calls."""
	try:
		from pyVoIP.VoIP import VoIPPhone
	except Exception:
		return

	orig = VoIPPhone._callback_RESP_NotFound

	def safe(self, request):  # type: ignore[no-untyped-def]
		try:
			call_id = request.headers.get("Call-ID")
			if call_id not in self.calls:
				print("sip 404 before call setup completed", flush=True)
				try:
					ack = self.sip.gen_ack(request)
					self.sip.out.sendto(ack.encode("utf8"), (self.server, self.port))
				except Exception:
					pass
				return
			return orig(self, request)
		except Exception as exc:
			print(f"sip 404 handler {type(exc).__name__}: {exc}", flush=True)

	VoIPPhone._callback_RESP_NotFound = safe  # type: ignore[method-assign]


def authorize_invite(invite: str, challenge: str, user: str, password: str, proxy: bool, branch: str) -> str:
	scheme, _, fields = challenge.partition(" ")
	if scheme.lower() != "digest":
		raise ValueError("Unsupported SIP authentication scheme")
	options = parse_keqv_list(parse_http_list(fields))
	algorithm = options.get("algorithm", "MD5").upper()
	if algorithm not in ("MD5", "SHA-256"):
		raise ValueError("Unsupported SIP digest algorithm")
	hash_text = hashlib.md5 if algorithm == "MD5" else hashlib.sha256
	realm, nonce = options["realm"], options["nonce"]
	uri = invite.split(" ", 2)[1]
	identity = hash_text(f"{user}:{realm}:{password}".encode()).hexdigest()
	destination = hash_text(f"INVITE:{uri}".encode()).hexdigest()
	digest = f"{identity}:{nonce}"
	qop = options.get("qop", "")
	auth_parameters = ""
	if qop:
		if "auth" not in (choice.strip().lower() for choice in qop.split(",")):
			raise ValueError("Unsupported SIP digest protection")
		cnonce = secrets.token_hex(8)
		digest += f":00000001:{cnonce}:auth"
		auth_parameters = f', qop=auth, nc=00000001, cnonce="{cnonce}"'
	response = hash_text(f"{digest}:{destination}".encode()).hexdigest()
	if "opaque" in options:
		auth_parameters += f', opaque="{options["opaque"]}"'
	header = "Proxy-Authorization" if proxy else "Authorization"
	auth = (f'{header}: Digest username="{user}", realm="{realm}", nonce="{nonce}", '
			f'uri="{uri}", response="{response}", algorithm={algorithm}{auth_parameters}')
	invite = re.sub(r"(?<=;branch=)[^;\r\n]+", branch, invite, count=1)
	invite = re.sub(r"(\r\nCSeq: )(\d+)( INVITE)",
					lambda match: f"{match[1]}{int(match[2]) + 1}{match[3]}", invite, count=1)
	return invite.replace("\r\nContent-Length:", f"\r\n{auth}\r\nContent-Length:", 1)


def advertise_rtp_address(message: str, address: str) -> str:
	headers, separator, body = message.partition("\r\n\r\n")
	body = re.sub(r"(?m)^(c=IN IP4 )\S+", lambda match: match[1] + address, body)
	headers = re.sub(r"(?<=\r\nContent-Length: )\d+", str(len(body.encode())), headers)
	return headers + separator + body


SIP_HEADER_NAMES = {
	name.lower().encode(): name.encode() for name in (
		"Via", "From", "To", "Call-ID", "CSeq", "Contact", "Record-Route", "Route", "Content-Type",
		"Content-Length", "WWW-Authenticate", "Proxy-Authenticate", "Authorization", "Content-Encoding",
		"Allow", "Supported", "Require", "Session-Expires",
	)
}
SIP_HEADER_NAMES.update({b"v": b"Via", b"f": b"From", b"t": b"To", b"i": b"Call-ID",
						 b"m": b"Contact", b"c": b"Content-Type", b"l": b"Content-Length", b"e": b"Content-Encoding"})


def normalize_sip_packet(packet: bytes) -> bytes:
	headers, _, body = packet.partition(b"\r\n\r\n")
	start, *lines = re.sub(rb"\r\n[ \t]+", b" ", headers).split(b"\r\n")
	fields = []
	for line in lines:
		name, separator, value = line.partition(b":")
		if separator:
			name = SIP_HEADER_NAMES.get(name.strip().lower(), name.strip())
			value = value.strip()
			if name == b"CSeq":
				value = b" ".join(value.split())
			fields.append(name + b": " + value)
	return b"\r\n".join((start, *fields)) + b"\r\n\r\n" + body


def sip_route_set(raw: bytes) -> tuple[str, ...]:
	lines = raw.split(b"\r\n\r\n", 1)[0].split(b"\r\n")
	return tuple(uri.decode() for line in lines if line.lower().startswith(b"record-route:")
				 for uri in re.findall(rb"<([^>]+)>", line))


def sip_uri(value: str) -> str:
	value = value.strip()
	return value.partition("<")[2].partition(">")[0] if "<" in value else value.split(",", 1)[0]


def sip_hop(uri: str) -> tuple[str, int]:
	match = re.fullmatch(r"sip:(?:[^@]+@)?([^:;?]+)(?::(\d+))?(?:[;?].*)?", uri, re.IGNORECASE)
	if not match:
		raise ValueError("Unsupported SIP route")
	port = int(match[2] or 5060)
	if not 1 <= port <= 65535:
		raise ValueError("Invalid SIP route port")
	return match[1], port


def tagged_address(header: dict) -> str:
	return header["raw"] + (f';tag={header["tag"]}' if header["tag"] else "")


@dataclass
class SipDialog:
	call_id: str
	local: str
	remote: str
	target: str
	routes: tuple[str, ...]
	sequence: int
	remote_sequence: int = 0

	def matches(self, message) -> bool:
		return (message.headers["To"]["tag"].split(";", 1)[0] == self.local.partition(";tag=")[2]
				and message.headers["From"]["tag"].split(";", 1)[0] == self.remote.partition(";tag=")[2])

	def request(self, method: str, sequence: int, address: str, port: int, branch: str) -> tuple[bytes, tuple[str, int]]:
		target = self.target
		routes = self.routes
		hop = routes[0] if routes else target
		if routes and not re.search(r";lr(?:[;=]|$)", routes[0], re.IGNORECASE):
			target, routes = routes[0], (*routes[1:], self.target)
		lines = [f"{method} {target} SIP/2.0", f"Via: SIP/2.0/UDP {address}:{port};branch={branch};rport",
				 "Max-Forwards: 70", f"From: {self.local}", f"To: {self.remote}", f"Call-ID: {self.call_id}",
				 f"CSeq: {sequence} {method}", *(f"Route: <{route}>" for route in routes),
				 "Content-Length: 0", "", ""]
		return "\r\n".join(lines).encode(), sip_hop(hop)


class MediaOfferError(ValueError):
	pass


def select_g711_media(raw: bytes) -> tuple[bytes, AudioOffer | None]:
	"""Normalize the single IPv4 audio stream supported by the handset."""
	head, sep, body = raw.partition(b"\r\n\r\n")
	if not body or not re.search(rb"(?im)^Content-Type: application/sdp(?:[;\s]|$)", head):
		return raw, None
	session, _, media = b"\r\n".join(line.strip() for line in body.splitlines()).partition(b"\r\nm=")
	sections = [section.split(b"\r\n") for section in media.split(b"\r\nm=") if section]
	rejection = "no supported IPv4 audio stream"
	for index, audio in enumerate(sections):
		heading = audio[0].split()
		if len(heading) < 4:
			continue
		media_type, port, transport, *payloads = heading
		if media_type.lower() != b"audio":
			continue
		transport = transport.upper()
		if transport not in (b"RTP/AVP", b"RTP/AVPF", *SECURE_PROFILES):
			rejection = "unsupported RTP profile"
			continue
		crypto = sdes_offer(audio[1:]) if transport in SECURE_PROFILES else None
		if transport in SECURE_PROFILES and crypto is None:
			rejection = "SRTP offer has no supported SDES suite"
			continue
		encodings = {b"0": b"0", b"8": b"8"}
		for line in audio[1:]:
			mapping = re.fullmatch(rb"a=rtpmap:(\d+)\s+(PCMU|PCMA)/8000(?:/1)?", line, re.I)
			if mapping and 96 <= int(mapping[1]) <= 127:
				encodings[mapping[1]] = b"0" if mapping[2].upper() == b"PCMU" else b"8"
		selected = {}
		for payload in payloads:
			codec = encodings.get(payload)
			if codec is not None:
				selected.setdefault(codec, payload)
		if selected and port.isdigit() and int(port) <= 65535:
			break
		rejection = "audio offer has no G.711 codec"
	else:
		raise MediaOfferError(rejection)
	connections = [line for line in audio[1:] if line.startswith(b"c=")]
	if not connections:
		connections = [line for line in session.split(b"\r\n") if line.startswith(b"c=")]
	connection = b" ".join(connections[0].split()) if connections else b""
	address = re.fullmatch(rb"c=IN IP4 ([A-Za-z0-9.-]+)", connection)
	if address is None:
		raise MediaOfferError("missing IPv4 media connection")
	lines = [line for line in session.split(b"\r\n") if line and not line.startswith(b"c=")]
	lines.extend((connection, b"m=" + b" ".join((b"audio", port, b"RTP/AVP", *selected))))
	for payload in selected:
		name = b"PCMU" if payload == b"0" else b"PCMA"
		lines.append(b"a=rtpmap:" + payload + b" " + name + b"/8000")
	direction = next((line for line in audio[1:] if line in (b"a=sendonly", b"a=recvonly", b"a=inactive", b"a=sendrecv")),
					 next((line for line in lines if line in (b"a=sendonly", b"a=recvonly", b"a=inactive", b"a=sendrecv")), b"a=sendrecv"))
	lines = [line for line in lines if line not in (b"a=sendonly", b"a=recvonly", b"a=inactive", b"a=sendrecv")]
	lines.append(direction)
	new_body = b"\r\n".join((*lines, b""))
	head = re.sub(rb"(?<=\r\nContent-Length: )\d+", str(len(new_body)).encode(), head)
	layout = []
	for slot, section in enumerate(sections):
		media_header = section[0].split()
		if len(media_header) < 4:
			raise MediaOfferError("invalid media line")
		layout.append(None if slot == index else b"m=" + b" ".join((media_header[0], b"0", *media_header[2:])))
	return head + sep + new_body, AudioOffer(transport, tuple(selected.items()), tuple(layout), crypto)


@dataclass
class SipServerTransaction:
	peer: tuple[str, int]
	expires: float
	record_routes: tuple[bytes, ...] = ()
	response: bytes | None = None
	answer_tag: str = ""
	next_retry: float | None = None
	retry_interval: float = 0.5


class SipReplySocket:
	"""Route server responses on the socket and transaction that received the request."""

	def __init__(self, sock: socket.socket):
		self.sock = sock
		self.contact: bytes | None = None
		self._lock = threading.Lock()
		self._transactions: dict[tuple[str, str, str, str], SipServerTransaction] = {}

	def __getattr__(self, name):
		return getattr(self.sock, name)

	def accept_request(self, message, source: tuple[str, int]) -> bool:
		via = message.headers["Via"][0]
		sequence = message.headers["CSeq"]["check"]
		call_id = message.headers["Call-ID"]
		method = message.method
		now = time.monotonic()
		with self._lock:
			if method == "ACK":
				tag = message.headers["To"]["tag"]
				for key, transaction in self._transactions.items():
					if key[:3] == (call_id, sequence, "INVITE") and tag and tag == transaction.answer_tag:
						if transaction.next_retry is not None:
							transaction.next_retry = None
							print("sip inbound ACK received", flush=True)
						break
				return False
			key = (call_id, sequence, method, via.get("branch", ""))
			transaction = self._transactions.get(key)
			if transaction is not None:
				if transaction.response is not None:
					self.sock.sendto(transaction.response, transaction.peer)
				return False
			host, source_port = source
			if "rport" in via:
				via["received"] = host
				via["rport"] = str(source_port)
				peer = source
			else:
				sent_by, via_port = via["address"]
				if sent_by != host:
					via["received"] = host
				peer = (host, int(via_port))
			routes = tuple(line for line in message.raw.split(b"\r\n\r\n", 1)[0].split(b"\r\n")
						   if line.lower().startswith(b"record-route:")) if method == "INVITE" else ()
			if method == "BYE":
				for other_key, pending in self._transactions.items():
					if other_key[0] == call_id:
						pending.next_retry = None
			lifetime = 180.0 if method == "INVITE" else 32.0
			self._transactions[key] = SipServerTransaction(peer, now + lifetime, routes)
			return True

	def sendto(self, packet: bytes, destination: tuple[str, int]) -> int:
		if not packet.startswith(b"SIP/2.0 "):
			return self.sock.sendto(packet, destination)
		headers, separator, body = packet.partition(b"\r\n\r\n")
		start_line, *lines = headers.split(b"\r\n")
		fields = {name.lower(): data.strip().decode("utf-8") for line in lines if b":" in line
				  for name, data in (line.split(b":", 1),)}
		sequence, method = fields.get(b"cseq", " ").split(" ", 1)
		via = next((line for line in lines if line.lower().startswith(b"via:")), b"")
		branch = re.search(rb";branch=([^;\s]+)", via, flags=re.IGNORECASE)
		key = (fields.get(b"call-id", ""), sequence, method, branch[1].decode("utf-8") if branch else "")
		status = int(start_line.split()[1])
		with self._lock:
			transaction = self._transactions.get(key)
			if transaction is None:
				return self.sock.sendto(packet, destination)
			if method == "INVITE" and 180 <= status < 300:
				for index, line in enumerate(lines):
					if self.contact is not None and line.lower().startswith(b"contact:"):
						lines[index] = b"Contact: " + self.contact
				lines = [line for line in lines if not line.lower().startswith(b"record-route:")]
				packet = b"\r\n".join((start_line, *lines, *transaction.record_routes)) + separator + body
			sent = self.sock.sendto(packet, transaction.peer)
			transaction.response = packet
			if method == "INVITE" and 200 <= status < 300:
				now = time.monotonic()
				transaction.answer_tag = fields.get(b"to", "").partition(";tag=")[2].split(";", 1)[0]
				transaction.next_retry = now + transaction.retry_interval
				transaction.expires = now + 32.0
				print("sip inbound 200 OK sent to INVITE peer", flush=True)
			return sent

	def retry_answers(self) -> None:
		now = time.monotonic()
		with self._lock:
			for key, transaction in list(self._transactions.items()):
				if now >= transaction.expires:
					if transaction.next_retry is not None:
						print("sip inbound answer timed out waiting for ACK", flush=True)
					del self._transactions[key]
				elif transaction.next_retry is not None and now >= transaction.next_retry:
					transaction.retry_interval = min(4.0, transaction.retry_interval * 2.0)
					transaction.next_retry = now + transaction.retry_interval
					self.sock.sendto(transaction.response, transaction.peer)


class PcmBridge:
	"""Thread-safe 8 kHz PCM queues between the jack and an optional SIP call."""

	def __init__(self) -> None:
		self._lock = threading.Lock()
		self.to_phone: deque[int] = deque(maxlen=16000)
		self.from_phone: deque[int] = deque(maxlen=16000)
		self.from_phone_tones: deque[int] = deque(maxlen=16000)

	def push_to_phone(self, samples: bytes) -> None:
		with self._lock:
			for i in range(0, len(samples) - 1, 2):
				self.to_phone.append(struct.unpack_from("<h", samples, i)[0])

	def take_from_phone(self, n: int) -> bytes:
		with self._lock:
			out = bytearray()
			for _ in range(n):
				s = self.from_phone.popleft() if self.from_phone else 0
				if self.from_phone_tones:
					s = self.from_phone_tones.popleft()
				s = max(-32768, min(32767, int(s)))
				out += struct.pack("<h", s)
			return bytes(out)

	def push_from_phone_i16(self, sample: int) -> None:
		with self._lock:
			self.from_phone.append(int(sample))

	def queue_line_dtmf(self, digit: str) -> None:
		if len(digit) != 1:
			return
		keys = ("123A", "456B", "789C", "*0#D")
		for row, group in enumerate(keys):
			if digit not in group:
				continue
			low = (697, 770, 852, 941)[row]
			high = (1209, 1336, 1477, 1633)[group.index(digit)]
			with self._lock:
				self.from_phone_tones.extend(round(5000 * (math.sin(math.tau * low * i / 8000)
					+ math.sin(math.tau * high * i / 8000))) for i in range(640))
				self.from_phone_tones.extend([0] * 400)
			return

	def clear_line_dtmf(self) -> None:
		with self._lock:
			self.from_phone_tones.clear()

	def take_to_phone_i16(self, n: int) -> list[int]:
		with self._lock:
			out = []
			for _ in range(n):
				out.append(self.to_phone.popleft() if self.to_phone else 0)
			return out


def tone_frame(freq: float, phase: float, n: int = 160, rate: int = 8000, amp: int = 9000):
	parts = []
	for i in range(n):
		sample = int(amp * math.sin(2 * math.pi * freq * (phase + i) / rate)) & 0xFFFF
		parts.append(f"{sample & 0xFF:02x}{(sample >> 8) & 0xFF:02x}")
	return phase + n, "".join(parts)


def dial_tone_frame(phase: float):
	# North American dial tone: 350 Hz + 440 Hz.
	parts = []
	for i in range(160):
		t = (phase + i) / 8000.0
		sample = int(4500 * (math.sin(2 * math.pi * 350 * t) + math.sin(2 * math.pi * 440 * t))) & 0xFFFF
		parts.append(f"{sample & 0xFF:02x}{(sample >> 8) & 0xFF:02x}")
	return phase + 160, "".join(parts)


def connect_jack(timeout: float = 90.0) -> socket.socket:
	deadline = time.time() + timeout
	while time.time() < deadline:
		try:
			return socket.create_connection((HOST, PORT), 1)
		except OSError:
			time.sleep(0.15)
	raise SystemExit("line jack never opened")


def answer_reversal(sock: socket.socket) -> None:
	# Polarity reversal as answer supervision, then mark the line connected.
	sock.sendall(b"V0\n")
	time.sleep(0.15)
	sock.sendall(b"V1\n")
	time.sleep(0.2)
	sock.sendall(b"V\n")
	sock.sendall(b"C1\n")


class SipSide:
	# Reserved direct-call sockets are consumed by the shared RTP start hook.
	_reserved_lock = threading.Lock()
	_reserved_rtp: dict[tuple[str, int], socket.socket] = {}

	def __init__(self, pcm: PcmBridge, direct: LocalVoiceRoute | None = None) -> None:
		self.pcm = pcm
		self.direct = direct
		self._reserved_keys: set[tuple[str, int]] = set()
		self.phone = None
		self.call = None
		self.local_peer: LocalSipPeer | None = None
		self._stop = threading.Event()
		self._thread: threading.Thread | None = None
		self.inbound_pending = threading.Event()
		self._lock = threading.Lock()
		self._local_only = direct is None and ((SIP_DEST.strip().lower() == "local") or (
			os.environ.get("COINLINE_SIP_PEER_ONLY", "0") == "1"
		))

	def destination_for(self, digits: str) -> str | None:
		if self._local_only or SIP_DEST:
			return (SIP_DEST or digits) if digits == EXPECT else None
		if match := re.fullmatch(r"1?([2-9][0-9]{2}[2-9][0-9]{6})", digits):
			return "+1" + match[1]
		if match := re.fullmatch(r"011([1-9][0-9]{6,14})", digits):
			return "+" + match[1]
		return None

	def start(self) -> None:
		if self._local_only:
			print("sip mode local-peer only (no registrar)", flush=True)
			return
		patch_pyvoip_not_found()
		import pyVoIP
		from pyVoIP.RTP import PayloadType, RTPClient, RTPPacketManager, TransmitType
		from pyVoIP.SIP import SIPMessage, SIPMessageType, SIPParseError
		from pyVoIP.VoIP import CallState, PhoneStatus, VoIPPhone

		if "OPTIONS" not in pyVoIP.SIPCompatibleMethods:
			pyVoIP.SIPCompatibleMethods.append("OPTIONS")

		encode_rtp = RTPClient.encode_packet
		parse_rtp = RTPClient.parse_packet

		def encode_g711(stream, payload):
			# The bundled RTP client routes both codecs through its mu-law encoder.
			return stream.encode_pcma(payload) if stream.preference == PayloadType.PCMA else encode_rtp(stream, payload)

		def observe_rtp(stream, packet):
			parse_rtp(stream, packet)
			if not getattr(stream, "coinline_received_media", False):
				stream.coinline_received_media = True
				print("sip RTP packet decoded", flush=True)

		def start_rtp(stream):
			with SipSide._reserved_lock:
				udp = SipSide._reserved_rtp.pop((stream.inIP, stream.inPort), None)
			reserved = udp is not None
			if udp is None:
				udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
			try:
				if not reserved:
					udp.bind((stream.inIP, stream.inPort))
				udp.setblocking(False)
			except OSError:
				udp.close()
				raise
			stream.sin = stream.sout = RtpWireSocket(udp, stream)
			for target in (stream.recv, stream.trans):
				threading.Thread(target=target, daemon=True).start()

		if not getattr(RTPClient, "coinline_hooks_installed", False):
			RTPClient.encode_packet = encode_g711
			RTPClient.parse_packet = observe_rtp
			RTPClient.start = start_rtp
			RTPClient.coinline_hooks_installed = True

		if self.direct is not None:
			host, user, password = HOST, self.direct.from_user, ""
			port, local_port, my_ip, media_ip, rtp_port = self.direct.port, 0, HOST, "", 0
		else:
			host = os.environ["COINLINE_SIP_HOST"]
			user = os.environ["COINLINE_SIP_USER"]
			password = os.environ["COINLINE_SIP_PASSWORD"]
			port = int(os.environ.get("COINLINE_SIP_PORT") or "5060")
			local_port = int(os.environ.get("COINLINE_SIP_LOCAL_PORT") or "5070")
			my_ip = os.environ.get("COINLINE_SIP_MY_IP") or "0.0.0.0"
			media_ip = os.environ.get("COINLINE_SIP_PUBLIC_IP", "")
			rtp_port = int(os.environ.get("COINLINE_SIP_RTP_PORT") or 0)
			print(f"sip registering local={local_port}", flush=True)
		self.phone = VoIPPhone(
			host,
			port,
			user,
			password,
			myIP=my_ip,
			callCallback=self._on_call,
			sipPort=local_port,
			rtpPortLow=rtp_port or 10000,
			rtpPortHigh=rtp_port or 20000,
		)
		# The outbound identity is the registered SIP address. Via and Contact
		# keep the local transport address used by the SIP socket.
		client = self.phone.sip
		if self.direct is not None:
			def direct_ready():
				nonlocal local_port
				# SIPClient.start has already bound its real UDP socket.
				local_port = client.s.getsockname()[1]
				client.myPort = local_port
				self.phone._status = PhoneStatus.REGISTERED
				print("local SIP voice endpoint ready without REGISTER", flush=True)
				return True
			client.register = direct_ready
			def reserve_direct_rtp(blocking=True):
				udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
				udp.bind((HOST, 0))
				key = (HOST, udp.getsockname()[1])
				with SipSide._reserved_lock:
					SipSide._reserved_rtp[key] = udp
				self._reserved_keys.add(key)
				self.phone.assignedPorts.append(key[1])
				return key[1]
			self.phone.request_port = reserve_direct_rtp
		parse_message = client.parse_message
		gen_ack = client.gen_ack
		gen_answer = client.gen_answer
		registration_response = client.trying_timeout_check
		pending_invites = {}
		dialogs = {}
		pending_byes = {}
		reply_socket = None
		signaling_contact = None

		def observe_registration_response(response):
			nonlocal media_ip, signaling_contact
			response = registration_response(response)
			if response.headers.get("CSeq", {}).get("method") == "REGISTER":
				status = int(response.status)
				print(f"sip REGISTER response {status}", flush=True)
				if status == 200:
					via = response.headers.get("Via", [])
					received = via[0].get("received") if via else None
					if received:
						try:
							observed = ipaddress.IPv4Address(received)
							if not media_ip and observed.is_global and str(observed) != my_ip:
								media_ip = str(observed)
								print("sip using registrar-observed public IPv4 for RTP", flush=True)
							mapped_port = int(via[0].get("rport") or 0)
							if 1 <= mapped_port <= 65535 and not observed.is_unspecified and not observed.is_multicast:
								signaling_contact = f"<sip:{user}@{observed}:{mapped_port}>".encode()
								if reply_socket is not None:
									reply_socket.contact = signaling_contact
						except ValueError:
							pass
			return response

		def answer_with_media_address(request, session_id, media, send_type):
			call_id = request.headers["Call-ID"]
			call = self.phone.calls.get(call_id)
			if call_id not in dialogs:
				if call is not None:
					send_type = refresh_media(call, request)
				dialogs[call_id] = SipDialog(call_id, request.headers["To"]["raw"] + ";tag=" + client.tagLibrary[call_id],
											 tagged_address(request.headers["From"]), sip_uri(request.headers["Contact"]),
											 sip_route_set(request.raw), 0, int(request.headers["CSeq"]["check"]))
			answer = gen_answer(request, session_id, media, send_type)
			offer = getattr(request, "coinline_audio_offer", None)
			stream = call.RTPClients[0] if call is not None and call.RTPClients else None
			security = getattr(stream, "coinline_sdes", None)
			if offer is None and stream is not None:
				offer = getattr(stream, "coinline_audio_offer", None)
			answer = sdp_answer(answer, offer, security.answer if security is not None else None)
			return advertise_rtp_address(answer, media_ip) if media_ip else answer

		def acknowledge(message):
			ack = gen_ack(message)
			to_header = message.headers["To"]
			to_value = to_header["raw"] + (f';tag={to_header["tag"]}' if to_header["tag"] else "")
			ack = re.sub(r"(?<=\r\nTo: )[^\r\n]+", lambda _: to_value, ack, count=1)
			if 200 <= int(message.status) < 300:
				contact = message.headers.get("Contact", "")
				if contact:
					target = contact.split("<", 1)[-1].split(">", 1)[0].strip()
					ack = re.sub(r"^ACK [^ ]+", lambda _: f"ACK {target}", ack, count=1)
				ack = re.sub(r"(?<=;branch=)[^;\r\n]+", client.gen_branch(), ack, count=1)
			return ack

		def end_dialog(message):
			call_id = message.headers["Call-ID"]
			dialog = dialogs.pop(call_id, None)
			if dialog is None:
				return
			dialog.sequence += 1
			packet, peer = dialog.request("BYE", dialog.sequence, my_ip, local_port, client.gen_branch())
			client.s.sendto(packet, peer)
			now = time.monotonic()
			pending_byes[(call_id, str(dialog.sequence))] = (packet, peer, now + 0.5, now + 32.0, 0.5)
			print("sip BYE sent on dialog route", flush=True)

		def server_reply(message, status="200 OK"):
			reply = client.gen_ok(message).replace("SIP/2.0 200 OK", f"SIP/2.0 {status}", 1)
			if message.headers["To"]["tag"]:
				reply = re.sub(r"(?<=\r\nTo: )[^\r\n]+", lambda _: tagged_address(message.headers["To"]), reply, count=1)
			client.out.sendto(reply.encode(), (host, port))

		def refresh_media(call, message):
			stream = call.RTPClients[0]
			offer = message.body["m"][0]
			association = {int(pt): PayloadType(int(pt)) for pt in offer["methods"]}
			remote = (message.body["c"][0]["address"], offer["port"])
			if association != stream.assoc or remote != (stream.outIP, stream.outPort):
				# A new RTP sender can start with unrelated sequence numbers and timestamps.
				stream.pmin = RTPPacketManager()
				stream.coinline_received_media = False
			stream.assoc = association
			stream.preference = next(iter(association.values()))
			audio = getattr(message, "coinline_audio_offer", None)
			if audio is not None:
				stream.coinline_payloads = {int(wire): int(codec) for codec, wire in audio.payloads}
				security = getattr(stream, "coinline_sdes", None)
				if audio.crypto is None:
					stream.coinline_sdes = None
				elif security is None or security.offer != audio.crypto:
					stream.coinline_sdes = SdesSessions(audio.crypto)
					print("sip using SDES-SRTP audio", flush=True)
				stream.coinline_audio_offer = audio
			send_type = {
				"sendonly": TransmitType.RECVONLY, "recvonly": TransmitType.SENDONLY, "inactive": TransmitType.INACTIVE,
			}.get(str(message.body.get("a", {}).get("transmit_type", "sendrecv")), TransmitType.SENDRECV)
			if remote[1] and remote[0] != "0.0.0.0":
				stream.outIP, stream.outPort = remote
			else:
				send_type = TransmitType.INACTIVE
			call.coinline_media_direction = send_type
			return send_type

		def observe_request(message):
			call_id = message.headers["Call-ID"]
			method = message.method
			dialog = dialogs.get(call_id)
			call = self.phone.calls.get(call_id)
			if method == "OPTIONS":
				server_reply(message)
				print("sip OPTIONS answered", flush=True)
				return
			if method == "BYE":
				if dialog is None or not dialog.matches(message):
					server_reply(message, "481 Call/Transaction Does Not Exist")
				else:
					dialogs.pop(call_id)
					if call is not None:
						call.bye()
					server_reply(message)
					print("sip remote BYE acknowledged", flush=True)
				return
			if method == "ACK":
				if (dialog is not None and dialog.matches(message) and call is not None and message.body.get("m")
						and dialog.remote_sequence == int(message.headers["CSeq"]["check"])):
					refresh_media(call, message)
				return
			if method == "INVITE" and (dialog is not None or message.headers["To"]["tag"]):
				if dialog is None or not dialog.matches(message) or call is None or call.state != CallState.ANSWERED:
					server_reply(message, "481 Call/Transaction Does Not Exist")
					return
				sequence = int(message.headers["CSeq"]["check"])
				if sequence <= dialog.remote_sequence:
					server_reply(message, "500 Server Internal Error")
					return
				dialog.remote_sequence = sequence
				stream = call.RTPClients[0]
				direction = refresh_media(call, message) if message.body.get("m") else TransmitType.SENDRECV
				answer = answer_with_media_address(message, call.session_id, {stream.inPort: stream.assoc}, direction)
				client.out.sendto(answer.encode(), (host, port))
				if message.headers.get("Contact"):
					dialog.target = sip_uri(message.headers["Contact"])
				print("sip re-INVITE answered on existing RTP port", flush=True)
				return
			if method == "INVITE" and not message.body.get("m"):
				server_reply(message, "488 Not Acceptable Here")
				return
			observe_invite_response(message)
			if method == "INVITE":
				call = self.phone.calls.get(call_id)
				if call is not None and call.RTPClients:
					# Prepare packet protection before the handset can start the RTP threads.
					call.sendmode = refresh_media(call, message)

		def send_invite(number, media, send_type):
			pending_invites.clear()
			call_id = client.gen_call_id()
			session_id = client.sessID.next()
			if self.direct is not None:
				for payloads in media.values():
					payloads.pop(101, None)  # direct ADSI route is in-band PCMU
			invite = client.gen_invite(number, str(session_id), media, send_type, client.gen_branch(), call_id)
			if self.direct is not None:
				invite = invite.replace(f"INVITE sip:{number}@{host} ", f"INVITE sip:{number}@{host}:{port} ", 1)
				invite = re.sub(r"(?m)^(Via:[^\r\n]+)(\r?)$", r"\1;rport\2", invite, count=1)
			if media_ip:
				invite = advertise_rtp_address(invite, media_ip)
			invite = invite.replace(f"\r\nFrom: <sip:{user}@{my_ip}>", f"\r\nFrom: <sip:{user}@{host}>", 1)
			pending_invites[call_id] = invite
			client.out.sendto(invite.encode(), (host, port))
			return SIPMessage(invite.encode()), call_id, session_id

		def observe_invite_response(message):
			if message.type == SIPMessageType.MESSAGE and message.method == "CANCEL":
				with self._lock:
					if self.call is not None and self.call.request.headers.get("Call-ID") == message.headers.get("Call-ID"):
						self.inbound_pending.clear()
			cseq = message.headers.get("CSeq", {})
			if message.type == SIPMessageType.RESPONSE and cseq.get("method") == "BYE":
				if int(message.status) >= 200 and pending_byes.pop((message.headers["Call-ID"], cseq["check"]), None) is not None:
					print(f"sip BYE response {int(message.status)}", flush=True)
				return
			if message.type != SIPMessageType.RESPONSE or cseq.get("method") != "INVITE":
				return parse_message(message)
			status = int(message.status)
			print(f"sip INVITE response {status}", flush=True)
			call_id = message.headers.get("Call-ID")
			if status in (401, 407) and call_id in pending_invites:
				invite = pending_invites.pop(call_id)
				client.out.sendto(client.gen_ack(message).encode(), (host, port))
				if self.direct is not None:
					print("local SIP voice endpoint unexpectedly requested authentication", flush=True)
					return
				proxy = status == 407
				challenge = message.headers.get("Proxy-Authenticate" if proxy else "WWW-Authenticate", "")
				try:
					if not challenge and not proxy:
						challenge = "Digest " + ", ".join(f'{key}="{value}"' for key, value in message.authentication.items())
					retry = authorize_invite(invite, challenge, user, password, proxy, client.gen_branch())
					client.out.sendto(retry.encode(), (host, port))
				except (ValueError, KeyError):
					print("sip INVITE authentication failed", flush=True)
				return
			if status < 200:
				return
			pending_invites.pop(call_id, None)
			if 200 <= status < 300:
				if self.direct is not None:
					contact_host, _ = sip_hop(sip_uri(message.headers["Contact"]))
					if contact_host != HOST or message.body["c"][0]["address"] != HOST or sip_route_set(message.raw):
						print("local SIP response attempted a non-loopback media or dialog route", flush=True)
						return
				sequence = int(cseq["check"])
				if call_id not in dialogs:
					call = self.phone.calls.get(call_id)
					if call is None:
						return
					dialogs[call_id] = SipDialog(call_id, tagged_address(message.headers["From"]),
												 tagged_address(message.headers["To"]), sip_uri(message.headers["Contact"]),
												 tuple(reversed(sip_route_set(message.raw))), sequence)
					call.answered(message)
					refresh_media(call, message)
				ack, peer = dialogs[call_id].request("ACK", sequence, my_ip, local_port, client.gen_branch())
				client.s.sendto(ack, peer)
				return
			parse_message(message)

		def receive_sip():
			nonlocal reply_socket
			if reply_socket is None:
				reply_socket = SipReplySocket(client.s)
				reply_socket.contact = signaling_contact
				client.out = reply_socket
			try:
				reply_socket.retry_answers()
				now = time.monotonic()
				for key, (packet, target, retry, expiry, delay) in list(pending_byes.items()):
					if now >= expiry:
						del pending_byes[key]
						print("sip BYE timed out", flush=True)
					elif now >= retry:
						delay = min(4.0, delay * 2.0)
						pending_byes[key] = (packet, target, now + delay, expiry, delay)
						client.s.sendto(packet, target)
				raw, peer = client.s.recvfrom(8192)
				if self.direct is not None and peer[0] != HOST:
					return
				if raw in (b"\x00\x00\x00\x00", b"\r\n", b"\r\n\r\n"):
					return
				raw = normalize_sip_packet(raw)
				offer = None
				first_line = raw.split(b"\r\n", 1)[0].split()
				kind = first_line[0] if first_line else b""
				if kind in (b"INVITE", b"ACK") or (kind == b"SIP/2.0" and re.search(rb"(?m)^CSeq: \d+ INVITE\r$", raw)):
					try:
						raw, offer = select_g711_media(raw)
						if kind == b"SIP/2.0" and offer is not None and offer.crypto is not None:
							raise MediaOfferError("secure reply to plain RTP offer")
					except ValueError as exc:
						reason = str(exc) if isinstance(exc, MediaOfferError) else "invalid SDP"
						if kind == b"INVITE":
							header = re.sub(rb"(?<=\r\nContent-Length: )\d+", b"0", raw.split(b"\r\n\r\n", 1)[0])
							message = SIPMessage(header + b"\r\n\r\n")
							if reply_socket.accept_request(message, peer):
								server_reply(message, "488 Not Acceptable Here")
								print(f"sip rejected media offer: {reason}", flush=True)
						else:
							print(f"sip rejected media answer: {reason}", flush=True)
						return
				message = SIPMessage(raw)
				message.coinline_audio_offer = offer
				if message.type == SIPMessageType.MESSAGE:
					accepted = reply_socket.accept_request(message, peer)
					if self.direct is not None and message.method == "INVITE" and message.headers.get("Call-ID") not in dialogs:
						if accepted:
							server_reply(message, "486 Busy Here")
						return
					if accepted or message.method == "ACK":
						observe_request(message)
				else:
					client.parse_message(message)
			except BlockingIOError:
				raise
			except (SIPParseError, ValueError, KeyError, IndexError, OSError) as exc:
				print(f"sip packet could not be processed: {type(exc).__name__}", flush=True)

		client.invite = send_invite
		client.recv = receive_sip
		client.gen_ack = acknowledge
		client.bye = end_dialog
		client.gen_answer = answer_with_media_address
		client.parse_message = observe_invite_response
		client.trying_timeout_check = observe_registration_response
		self.phone.start()
		deadline = time.time() + 12
		while time.time() < deadline:
			status = self.phone.get_status()
			if status == PhoneStatus.REGISTERED:
				if self.direct is None:
					print(f"sip status {status}", flush=True)
				break
			time.sleep(0.25)
		else:
			print(f"sip status {self.phone.get_status()}", flush=True)
		self._thread = threading.Thread(target=self._pump, name="sip-pcm", daemon=True)
		self._thread.start()

	def stop(self) -> None:
		self._stop.set()
		self.hangup()
		if self.phone is not None:
			self.phone.stop()
		with SipSide._reserved_lock:
			for key in self._reserved_keys:
				if (udp := SipSide._reserved_rtp.pop(key, None)) is not None:
					udp.close()
		self._reserved_keys.clear()

	def hangup(self) -> None:
		peer, self.local_peer = self.local_peer, None
		if peer is not None:
			peer.stop()
		with self._lock:
			call, self.call = self.call, None
			self.inbound_pending.clear()
		if call is not None:
			from pyVoIP.VoIP import CallState

			try:
				with call.sip.recvLock:
					if call.state == CallState.ANSWERED:
						call.hangup()
					elif call.state == CallState.RINGING:
						call.deny()
			except (OSError, ValueError) as exc:
				print(f"sip local hangup failed: {type(exc).__name__}", flush=True)

	def answered(self) -> bool:
		if self._local_only:
			return self.local_peer is not None
		from pyVoIP.VoIP import CallState

		with self._lock:
			return self.call is not None and self.call.state == CallState.ANSWERED

	def _on_call(self, call) -> None:
		print("sip inbound", flush=True)
		with self._lock:
			self.call = call
			# Do not answer until the handset lifts.
			self.inbound_pending.set()

	def answer_pending(self) -> bool:
		from pyVoIP.VoIP import CallState

		with self._lock:
			call = self.call
		if call is None:
			self.inbound_pending.clear()
			return False
		deadline = time.time() + 10
		while time.time() < deadline:
			try:
				if call.state == CallState.RINGING:
					with call.sip.recvLock:
						call.answer()
					print("sip answered inbound", flush=True)
					self.inbound_pending.clear()
					return True
				if call.state == CallState.ANSWERED:
					self.inbound_pending.clear()
					return True
			except Exception:
				time.sleep(0.05)
				continue
			time.sleep(0.05)
		self.inbound_pending.clear()
		return False

	def dial(self, number: str) -> None:
		if self.direct is not None and number != self.direct.target:
			return
		if number.strip().lower() == "local" or self._local_only:
			self.local_peer = LocalSipPeer(self.pcm)
			ok = self.local_peer.dial()
			if not ok:
				self.local_peer = None
			return
		if self.phone is None:
			return
		from pyVoIP.VoIP import PhoneStatus

		if self.phone.get_status() == PhoneStatus.FAILED:
			print("sip call could not start because registration failed", flush=True)
			return
		print("sip sending outbound INVITE", flush=True)
		try:
			with self.phone.sip.recvLock:
				call = self.phone.call(number)
			with self._lock:
				self.call = call
		except Exception as exc:
			print(f"sip dial error {type(exc).__name__}", flush=True)
			with self._lock:
				self.call = None

	def _pump(self) -> None:
		from pyVoIP.RTP import TransmitType
		from pyVoIP.VoIP import CallState

		logged_mic = None
		next_audio = time.monotonic()
		while not self._stop.is_set():
			with self._lock:
				call = self.call
			if call is None or call.state != CallState.ANSWERED:
				logged_mic = None
				next_audio = time.monotonic()
				time.sleep(0.02)
				continue
			try:
				direction = getattr(call, "coinline_media_direction", TransmitType.SENDRECV)
				# pyVoIP uses unsigned 8-bit PCM. The line jack uses signed 16-bit PCM.
				sip_audio = call.read_audio(160, blocking=False)
				if sip_audio and direction in (TransmitType.SENDRECV, TransmitType.RECVONLY):
					jack_audio = struct.pack(f"<{len(sip_audio)}h", *((sample - 128) << 8 for sample in sip_audio))
					self.pcm.push_to_phone(jack_audio)
				jack_audio = self.pcm.take_from_phone(160)
				if direction in (TransmitType.SENDRECV, TransmitType.SENDONLY):
					sip_audio = bytes(high_byte ^ 0x80 for high_byte in jack_audio[1::2])
					call.write_audio(sip_audio)
					if logged_mic is not call and any(abs(sample - 128) > 1 for sample in sip_audio):
						logged_mic = call
						print("sip microphone signal reached transmit queue", flush=True)
			except Exception:
				time.sleep(0.02)
				continue
			if self.direct is not None:
				now = time.monotonic()
				next_audio = max(now, next_audio + 0.020)
				self._stop.wait(max(0.0, next_audio - now))
			else:
				time.sleep(0.018)


def run_local_or_sip() -> None:
	mode = env_mode()
	local_route = LocalVoiceRoute.parse(os.environ.get("COINLINE_LOCAL_VOICE_ROUTE", ""))
	print(f"co mode {mode}", flush=True)
	pcm = PcmBridge()
	sip: SipSide | None = None
	if mode == "sip":
		sip = SipSide(pcm)
		sip.start()
	direct_sip: SipSide | None = None
	active_sip: SipSide | None = None

	def release_voice() -> None:
		nonlocal active_sip
		if active_sip is not None:
			active_sip.hangup()
		elif sip is not None:
			sip.hangup()
		active_sip = None
		pcm.clear_line_dtmf()

	sock = connect_jack()
	sock.settimeout(0.05)
	# Tip/ring battery present while the jack is up — craft CO check (Failed:41) needs this
	# before answer supervision; do not wait for dial/answer to assert C1.
	sock.sendall(b"C1\n")
	print("jack C1 line present", flush=True)
	buf = b""
	digits = ""
	seized = False
	answered = False
	outbound_attempted = False
	voice_bridged = False
	sip_bridged = False
	offhook = False
	enring = False
	ring_started_at = None
	released_at = None
	remote_released = False
	last_digit_at = None
	modem_relay = False
	mic_frames = 0
	received_audio_frames = 0
	phase = 0.0
	dial_phase = 0.0
	sending_dial_tone = False
	next_audio = None
	audio_period = 160 / 8000
	timeout = float(os.environ.get("COINLINE_CO_TIMEOUT", "180"))
	end = time.monotonic() + timeout if timeout else math.inf
	ring_on = False

	want_inbound = os.environ.get("COINLINE_CO_INBOUND", "0") == "1"

	while time.monotonic() < end:
		try:
			wait = 0.05 if next_audio is None else max(0.0, min(0.05, next_audio - time.monotonic()))
			if select.select([sock], [], [], wait)[0]:
				chunk = sock.recv(16384)
				if not chunk:
					break
				buf += chunk
		except OSError:
			break

		while b"\n" in buf:
			line, buf = buf.split(b"\n", 1)
			text = line.decode("ascii", "replace").strip()
			if text and not text.startswith(("M", "W")) and (mode == "local" or not text.startswith("D")):
				print(f"jack {text}", flush=True)
			if text == "S":
				seized = True
				print("seized", flush=True)
			elif text == "Q1":
				modem_relay = True
			elif text == "Q0":
				# The manager modem can release while the handset keeps the
				# same physical loop seized. Restore the CO before voice dialing.
				if modem_relay and digits == NCC_LOOPBACK:
					answered = voice_bridged = sip_bridged = False
					digits = ""
					last_digit_at = None
					outbound_attempted = False
					sock.sendall(b"V\n")
					print("manager modem released; voice dialing ready", flush=True)
				modem_relay = False
			elif text == "X":
				seized = False
				answered = False
				voice_bridged = False
				sip_bridged = False
				digits = ""
				last_digit_at = None
				outbound_attempted = False
				release_voice()
				print("released", flush=True)
				sock.sendall(b"V\n")
			elif text.startswith("D") and len(text) == 2 and offhook:
				if answered and active_sip is direct_sip and direct_sip is not None:
					pcm.queue_line_dtmf(text[1])
					print("local SIP post-answer DTMF event", flush=True)
				elif seized and not answered:
					digits += text[1]
					last_digit_at = time.monotonic()
					print(f"digit {digits}" if mode == "local" else f"dialed {len(digits)} digits", flush=True)
			elif text.startswith("M") and len(text) > 1:
				mic_frames += 1
				hexpart = text[1:]
				for i in range(0, len(hexpart) - 3, 4):
					lo = int(hexpart[i : i + 2], 16)
					hi = int(hexpart[i + 2 : i + 4], 16)
					pcm.push_from_phone_i16(struct.unpack("<h", bytes((lo, hi)))[0])
			elif text == "H1":
				offhook = True
				print("offhook", flush=True)
				if ring_started_at is not None:
					if ring_on:
						sock.sendall(b"R0\n")
					ring_started_at = None
					ring_on = False
					sip_answered = sip is not None and sip.inbound_pending.is_set() and sip.answer_pending()
					if not answered and (want_inbound or sip_answered):
						answer_reversal(sock)
						answered = True
						voice_bridged = True
						sip_bridged = sip_answered
						active_sip = sip if sip_answered else None
						print("answered inbound", flush=True)
			elif text == "H0":
				offhook = False
				remote_released = False
				digits = ""
				last_digit_at = None
				print("onhook", flush=True)
				if outbound_attempted or answered:
					outbound_attempted = False
					release_voice()
				if answered:
					sock.sendall(b"V\n")
					answered = False
					voice_bridged = False
					sip_bridged = False
			elif text == "E1":
				enring = True
				print("enring", flush=True)
			elif text == "E0":
				enring = False

		now = time.monotonic()
		sip_wants_ring = sip is not None and sip.inbound_pending.is_set()

		local_inbound = want_inbound and (enring or ring_started_at is not None)
		if not offhook and (local_inbound or sip_wants_ring):
			if ring_started_at is None:
				ring_started_at = now
				print("ring start", flush=True)
			# Standard North American ringing: 2 seconds on, 4 seconds off.
			should_ring = (now - ring_started_at) % 6.0 < 2.0
		else:
			ring_started_at = None
			should_ring = False
		if should_ring != ring_on:
			ring_on = should_ring
			sock.sendall(b"R1\n" if ring_on else b"R0\n")

		if sip_bridged and (active_sip is None or not active_sip.answered()):
			answered = voice_bridged = sip_bridged = False
			release_voice()
			remote_released = True
			released_at = now + 1.0
			sock.sendall(b"C0\nV\n")
			print("SIP remote hangup, CO loop current interrupted", flush=True)
		elif released_at is not None and now >= released_at:
			released_at = None
			sock.sendall(b"C1\n")
			print("CO loop current restored", flush=True)

		if (offhook and not answered and not outbound_attempted and last_digit_at is not None
				and now - last_digit_at > 1.5):
			if digits == NCC_LOOPBACK:
				# The handset is lifted for the analog answer-supervision test.
				# On-hook manager calls use the data modem instead.
				if now - last_digit_at > 6.0:
					answer_reversal(sock)
					answered = True
					print("manager answer supervision", flush=True)
			elif local_route is not None and local_route.matches(digits):
				# An exact loopback route is consumed even if the peer is unavailable;
				# never retry it through the ordinary SIP/PSTN destination logic.
				outbound_attempted = True
				try:
					if direct_sip is None:
						direct_sip = SipSide(pcm, direct=local_route)
						direct_sip.start()
					active_sip = direct_sip
					direct_sip.dial(local_route.target)
					print("local SIP exact voice route selected", flush=True)
				except (OSError, ValueError, ImportError, RuntimeError) as exc:
					if direct_sip is not None:
						direct_sip.stop()
					direct_sip = active_sip = None
					print(f"local SIP voice route unavailable: {type(exc).__name__}", flush=True)
			elif sip is not None:
				if destination := sip.destination_for(digits):
					outbound_attempted = True
					active_sip = sip
					sip.dial(destination)
			elif digits == EXPECT:
				outbound_attempted = True
				answer_reversal(sock)
				answered = True
				voice_bridged = True
				print("answered outbound", flush=True)

		if outbound_attempted and not answered and active_sip is not None and active_sip.answered():
			answer_reversal(sock)
			answered = True
			voice_bridged = True
			sip_bridged = True
			print("SIP answered outbound", flush=True)

		dialing = seized and not answered and not digits and not remote_released
		bridged = answered and voice_bridged
		if dialing != sending_dial_tone:
			sock.sendall(b"T1\n" if dialing else b"T0\n")
			sending_dial_tone = dialing
		if not (dialing or bridged):
			next_audio = None
			continue
		now = time.monotonic()
		if next_audio is not None and now < next_audio:
			continue
		# Send one 20 ms frame on its own clock. Reads may be silent or arrive in bursts.
		next_audio = max(now + 0.001, (next_audio if next_audio is not None else now) + audio_period)
		if dialing:
			dial_phase, payload = dial_tone_frame(dial_phase)
		else:
			samples = pcm.take_to_phone_i16(160)
			if mode == "local" and not sip_bridged and not any(samples):
				phase, payload = tone_frame(440.0, phase)
			else:
				payload = struct.pack("<160h", *samples).hex()
			if sip_bridged and any(samples):
				if received_audio_frames == 0:
					print("SIP audio reached the line jack", flush=True)
				received_audio_frames += 1
		try:
			sock.sendall(f"W{payload}\n".encode("ascii"))
		except OSError:
			break

	print(
		f"result seized={int(seized)} sent_digits={len(digits)} answered={int(answered)} "
		f"mic_frames={mic_frames} sip_audio_frames={received_audio_frames} offhook={int(offhook)}",
		flush=True,
	)
	if sip is not None:
		sip.stop()
	if direct_sip is not None:
		direct_sip.stop()


if __name__ == "__main__":
	run_local_or_sip()
