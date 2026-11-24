# SPDX-License-Identifier: GPL-2.0-or-later
"""Translate negotiated RTP payload numbers and protect SDES-SRTP media."""

from __future__ import annotations

import base64
from dataclasses import dataclass, field
import re
import secrets


SECURE_PROFILES = (b"RTP/SAVP", b"RTP/SAVPF")
SDES_SUITES = (b"AES_CM_128_HMAC_SHA1_80", b"AES_CM_128_HMAC_SHA1_32")


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
		payloads = getattr(self.stream, "coinline_payloads", {})
		if len(packet) >= 12 and payloads:
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
	if crypto is not None:
		params += crypto + b"\r\n"
	audio = b"m=" + heading + b"\r\n" + params
	body = session + b"\r\n" + b"".join(audio if line is None else line + b"\r\n" for line in offer.layout)
	headers = re.sub(rb"(?<=\r\nContent-Length: )\d+", str(len(body)).encode(), headers)
	return (headers + sep + body).decode()
