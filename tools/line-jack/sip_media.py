# SPDX-License-Identifier: GPL-2.0-or-later
"""Translate negotiated RTP payload numbers and protect SDES-SRTP media."""

from __future__ import annotations

import base64
from dataclasses import dataclass, field
from collections import deque
import re
import secrets
import struct
import threading


SECURE_PROFILES = (b"RTP/SAVP", b"RTP/SAVPF")
SDES_SUITES = (b"AES_CM_128_HMAC_SHA1_80", b"AES_CM_128_HMAC_SHA1_32")
DTMF_DIGITS = "0123456789*#ABCD"


@dataclass(frozen=True)
class TelephoneEventOffer:
	payload: int
	events: frozenset[int]


def telephone_event_offer(lines: list[bytes], payloads: list[bytes]) -> TelephoneEventOffer | None:
	"""Select a dynamic 8 kHz RFC 4733 format and its offered DTMF event set."""
	for payload in payloads:
		if not payload.isdigit() or not 96 <= int(payload) <= 127:
			continue
		if not any(re.fullmatch(rb"a=rtpmap:" + payload + rb"\s+telephone-event/8000(?:/1)?", line, re.I) for line in lines):
			continue
		fmtp = next((line.split(None, 1)[1] for line in lines
					 if line.lower().startswith(b"a=fmtp:" + payload + b" ") and len(line.split(None, 1)) == 2), b"0-15")
		events = set()
		for token in fmtp.split(b","):
			match = re.fullmatch(rb"\s*(\d+)(?:-(\d+))?\s*", token)
			if match:
				start, end = int(match[1]), int(match[2] or match[1])
				events.update(range(max(0, start), min(15, end) + 1))
		if events:
			return TelephoneEventOffer(int(payload), frozenset(events))
	return None


class RtpDtmfSender:
	"""Replace normal RTP frames with RFC 4733 events on the same RTP clock."""
	def __init__(self, offer: TelephoneEventOffer):
		self.offer = offer
		self._lock = threading.Lock()
		self._queue = deque()
		self._active = None
		self._gap_samples = 0

	def queue(self, digit: str, duration_ms: int = 90) -> bool:
		if len(digit) != 1 or digit not in DTMF_DIGITS or DTMF_DIGITS.index(digit) not in self.offer.events:
			return False
		with self._lock:
			if len(self._queue) >= 64:
				return False
			self._queue.append((DTMF_DIGITS.index(digit), max(40, min(8000, duration_ms)) * 8))
		return True

	def packet(self, audio: bytes) -> bytes | None:
		# The G.711 sender emits a 12-byte header and one sample per payload byte.
		if len(audio) <= 12 or audio[0] != 0x80 or audio[1] & 0x7f not in (0, 8):
			return None
		length = len(audio) - 12
		with self._lock:
			if self._gap_samples:
				self._gap_samples = max(0, self._gap_samples - length)
				return None
			first = self._active is None
			if first:
				if not self._queue:
					return None
				event, total = self._queue.popleft()
				self._active = [event, total, 0, audio[4:8], 0]
			event, total, elapsed, timestamp, terminal_packets = self._active
			elapsed = min(total, elapsed + length)
			ended = elapsed == total
			terminal_packets += int(ended)
			# RFC 4733 uses the onset timestamp, increasing duration, marker on
			# the first packet, and three final packets with the E bit set.
			packet = (audio[:1] + bytes((self.offer.payload | (0x80 if first else 0),)) + audio[2:4]
					  + timestamp + audio[8:12] + struct.pack("!BBH", event, 10 | (0x80 if ended else 0), elapsed))
			if terminal_packets == 3:
				self._active = None
				self._gap_samples = 480
			else:
				self._active = [event, total, elapsed, timestamp, terminal_packets]
			return packet


@dataclass(frozen=True)
class SdesOffer:
	tag: bytes
	suite: bytes
	key: bytes = field(repr=False)


@dataclass(frozen=True)
class AudioOffer:
	profile: bytes
	payloads: tuple[tuple[bytes, bytes], ...]
	layout: tuple[bytes | None, ...]
	crypto: SdesOffer | None = None
	telephone_event: TelephoneEventOffer | None = None


def sdes_offer(lines: list[bytes]) -> SdesOffer | None:
	for line in lines:
		match = re.fullmatch(rb"a=crypto:(\d+) (\S+) inline:([A-Za-z0-9+/=]+)(?:\|2\^48)?", line, re.I)
		if not match or match[2] not in SDES_SUITES:
			continue
		try:
			key = base64.b64decode(match[3], validate=True)
		except ValueError:
			continue
		if len(key) == 30:
			return SdesOffer(match[1], match[2], key)
	return None


class SdesSessions:
	def __init__(self, offer: SdesOffer):
		from pylibsrtp import Error, Policy, Session

		self.offer = offer
		key = secrets.token_bytes(30)
		self.answer = b"a=crypto:" + offer.tag + b" " + offer.suite + b" inline:" + base64.b64encode(key)
		profile = (Policy.SRTP_PROFILE_AES128_CM_SHA1_80 if offer.suite == SDES_SUITES[0]
				   else Policy.SRTP_PROFILE_AES128_CM_SHA1_32)
		self.inbound = Session(Policy(key=offer.key, ssrc_type=Policy.SSRC_ANY_INBOUND, srtp_profile=profile))
		self.outbound = Session(Policy(key=key, ssrc_type=Policy.SSRC_ANY_OUTBOUND, srtp_profile=profile))
		self.error = Error


class RtpWireSocket:
	"""Keep the same UDP socket when a SIP re-INVITE changes media."""

	def __init__(self, sock, stream):
		self.sock = sock
		self.stream = stream

	def __getattr__(self, name):
		return getattr(self.sock, name)

	def recv(self, size: int) -> bytes:
		packet = self.sock.recv(size)
		security = getattr(self.stream, "coinline_sdes", None)
		if security is not None:
			try:
				packet = security.inbound.unprotect(packet)
			except security.error as exc:
				raise BlockingIOError from exc
		payloads = getattr(self.stream, "coinline_payloads", {})
		if len(packet) >= 12 and payloads:
			wire_type = packet[1] & 0x7F
			if wire_type not in payloads:
				raise BlockingIOError
			packet = packet[:1] + bytes(((packet[1] & 0x80) | payloads[wire_type],)) + packet[2:]
		return packet

	def sendto(self, packet: bytes, target) -> int:
		sender = getattr(self.stream, "coinline_dtmf_sender", None)
		event = sender.packet(packet) if sender is not None else None
		if event is not None:
			packet = event
		payloads = getattr(self.stream, "coinline_payloads", {})
		if event is None and len(packet) >= 12 and payloads:
			codec = packet[1] & 0x7F
			wire_type = next((wire for wire, actual in payloads.items() if actual == codec), codec)
			packet = packet[:1] + bytes(((packet[1] & 0x80) | wire_type,)) + packet[2:]
		security = getattr(self.stream, "coinline_sdes", None)
		if security is not None:
			try:
				packet = security.outbound.protect(packet)
			except security.error as exc:
				raise OSError("Could not protect RTP packet") from exc
		return self.sock.sendto(packet, target)


def sdp_answer(answer: str, offer: AudioOffer | None, crypto: bytes | None) -> str:
	if offer is None:
		return answer
	headers, sep, body = answer.encode().partition(b"\r\n\r\n")
	session, _, audio = body.partition(b"\r\nm=")
	heading, _, params = audio.partition(b"\r\n")
	media, port, _, *codecs = heading.split()
	wire_payloads = dict(offer.payloads)
	heading = b" ".join((media, port, offer.profile, *(wire_payloads[codec] for codec in codecs)))
	params = re.sub(rb"(?m)^a=rtpmap:(\d+)(?= )", lambda match: b"a=rtpmap:" + wire_payloads[match[1]], params)
	if offer.telephone_event is not None:
		pt = str(offer.telephone_event.payload).encode()
		heading += b" " + pt
		events = b",".join(str(event).encode() for event in sorted(offer.telephone_event.events))
		params += b"a=rtpmap:" + pt + b" telephone-event/8000\r\na=fmtp:" + pt + b" " + events + b"\r\n"
	if crypto is not None:
		params += crypto + b"\r\n"
	audio = b"m=" + heading + b"\r\n" + params
	body = session + b"\r\n" + b"".join(audio if line is None else line + b"\r\n" for line in offer.layout)
	headers = re.sub(rb"(?<=\r\nContent-Length: )\d+", str(len(body)).encode(), headers)
	return (headers + sep + body).decode()
