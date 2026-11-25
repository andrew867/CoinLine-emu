# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise connected DTMF through a real SIP dialog and its loopback RTP socket."""

import math
import queue
import re
import socket
import struct
import threading
import time
import unittest
from unittest.mock import patch

import central_office as office

try:
    import pyVoIP  # noqa: F401
except ImportError:
    pyVoIP = None


class LoopbackPeer:
    def __init__(self, telephone_event=False, info=False, send_audio=False):
        self.telephone_event = telephone_event
        self.info = info
        self.send_audio = send_audio
        self.remote_rtp = None
        self.audio_worker = None
        self.sip = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sip.bind(("127.0.0.1", 0))
        self.sip.settimeout(0.1)
        self.rtp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.rtp.bind(("127.0.0.1", 0))
        self.rtp.settimeout(0.1)
        self.requests = queue.Queue()
        self.stop = threading.Event()
        self.worker = threading.Thread(target=self.run, daemon=True)
        self.worker.start()

    @property
    def port(self):
        return self.sip.getsockname()[1]

    def run(self):
        while not self.stop.is_set():
            try:
                packet, peer = self.sip.recvfrom(32768)
            except socket.timeout:
                continue
            except OSError:
                break
            head, _, body = packet.partition(b"\r\n\r\n")
            first, *lines = head.split(b"\r\n")
            method = first.split(b" ", 1)[0]
            headers = {key.lower(): value.strip() for line in lines if b":" in line
                       for key, value in [line.split(b":", 1)]}
            self.requests.put((method, headers, body))
            if method == b"ACK":
                if self.send_audio and self.remote_rtp is not None and self.audio_worker is None:
                    self.audio_worker = threading.Thread(target=self.transmit_audio, daemon=True)
                    self.audio_worker.start()
                continue
            if method == b"SIP/2.0":
                continue
            if method not in (b"REGISTER", b"INVITE", b"INFO", b"BYE"):
                continue
            result = b""
            if method == b"INVITE":
                if port := re.search(rb"(?m)^m=audio (\d+) RTP/AVP ", body):
                    self.remote_rtp = ("127.0.0.1", int(port[1]))
                result = (b"v=0\r\no=lab 1 1 IN IP4 127.0.0.1\r\ns=Loopback\r\nc=IN IP4 127.0.0.1\r\nt=0 0\r\n"
                          + f"m=audio {self.rtp.getsockname()[1]} RTP/AVP 0".encode()
                          + (b" 101" if self.telephone_event else b"") + b"\r\na=rtpmap:0 PCMU/8000\r\n"
                          + (b"a=rtpmap:101 telephone-event/8000\r\na=fmtp:101 0-15\r\n" if self.telephone_event else b"")
                          + b"a=sendrecv\r\n")
            to = headers[b"to"]
            if b";tag=" not in to:
                to += b";tag=loopback-peer"
            fields = [b"SIP/2.0 200 OK", b"Via: " + headers[b"via"], b"From: " + headers[b"from"],
                      b"To: " + to, b"Call-ID: " + headers[b"call-id"], b"CSeq: " + headers[b"cseq"],
                      f"Contact: <sip:lab@127.0.0.1:{self.port}>".encode(), b"Expires: 300",
                      b"Allow: INVITE, ACK, BYE, INFO"]
            if self.info:
                fields.append(b"Accept: application/sdp, application/dtmf-relay")
            if result:
                fields.append(b"Content-Type: application/sdp")
            fields.append(b"Content-Length: " + str(len(result)).encode())
            self.sip.sendto(b"\r\n".join(fields) + b"\r\n\r\n" + result, peer)

    def transmit_audio(self):
        """Provide real far-end PCMU after answer in native firmware probes."""
        sequence, timestamp = 1, 0
        next_packet = time.monotonic()
        while not self.stop.is_set():
            payload = bytes(office.mulaw_encode(round(4000 * math.sin(math.tau * 440 * (timestamp + i) / 8000)))
                            for i in range(160))
            packet = struct.pack("!BBHII", 0x80, 0, sequence, timestamp, 1234) + payload
            try:
                self.rtp.sendto(packet, self.remote_rtp)
            except OSError:
                break
            sequence = (sequence + 1) & 0xffff
            timestamp = (timestamp + 160) & 0xffffffff
            next_packet += 0.020
            self.stop.wait(max(0.0, next_packet - time.monotonic()))

    def close(self):
        self.stop.set()
        self.worker.join(timeout=1)
        if self.audio_worker is not None:
            self.audio_worker.join(timeout=1)
        self.sip.close()
        self.rtp.close()

    def wait_request(self, method, timeout=5):
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            try:
                item = self.requests.get(timeout=0.1)
            except queue.Empty:
                continue
            if item[0] == method:
                return item
        raise AssertionError(f"Did not observe {method!r} in the loopback dialog")


@unittest.skipIf(pyVoIP is None, "optional SIP runtime is not installed")
class ConnectedDtmfLoopbackTest(unittest.TestCase):
    def run_dialog(self, telephone_event, info, verify):
        peer = LoopbackPeer(telephone_event, info)
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
            probe.bind(("127.0.0.1", 0))
            bind_port = probe.getsockname()[1]
        env = {"COINLINE_SIP_HOST": "127.0.0.1", "COINLINE_SIP_PORT": str(peer.port),
               "COINLINE_SIP_USER": "lab", "COINLINE_SIP_PASSWORD": "loopback-fixture",
               "COINLINE_SIP_MY_IP": "127.0.0.1", "COINLINE_SIP_LOCAL_PORT": str(bind_port),
               "COINLINE_SIP_PUBLIC_IP": "", "COINLINE_SIP_RTP_PORT": "0",
               "COINLINE_SIP_PEER_ONLY": "0", "COINLINE_SIP_DTMF_MODE": "auto"}
        pcm = office.PcmBridge()
        with patch.dict(office.os.environ, env), patch.object(office, "SIP_DEST", ""):
            side = office.SipSide(pcm)
            try:
                side.start()
                side.dial("1234")
                peer.wait_request(b"INVITE")
                peer.wait_request(b"ACK")
                end = time.monotonic() + 3
                while not side.answered() and time.monotonic() < end:
                    time.sleep(0.02)
                self.assertTrue(side.answered())
                verify(side, peer)
                side.hangup()
                peer.wait_request(b"BYE")
            finally:
                side.stop()
                peer.close()

    def test_negotiated_rtp_events(self):
        def verify(side, peer):
            self.assertEqual(side.send_dtmf("5"), "rtp-event")
            events = []
            deadline = time.monotonic() + 3
            while time.monotonic() < deadline and len(events) < 7:
                try:
                    packet, _ = peer.rtp.recvfrom(8192)
                except socket.timeout:
                    continue
                if len(packet) == 16 and packet[1] & 0x7f == 101:
                    events.append(packet)
            self.assertEqual(len(events), 7)
            self.assertEqual([item[1] for item in events], [0x80 | 101] + [101] * 6)
            self.assertTrue(all(item[4:8] == events[0][4:8] for item in events))
            self.assertEqual([struct.unpack("!BBH", item[12:]) for item in events],
                             [(5, 10, 160), (5, 10, 320), (5, 10, 480), (5, 10, 640),
                              (5, 0x8a, 720), (5, 0x8a, 720), (5, 0x8a, 720)])
        self.run_dialog(True, True, verify)

    def test_explicit_legacy_info_support(self):
        def verify(side, peer):
            self.assertEqual(side.send_dtmf("#"), "info")
            _, headers, body = peer.wait_request(b"INFO")
            self.assertEqual(headers[b"content-type"], b"application/dtmf-relay")
            self.assertEqual(body, b"Signal=#\r\nDuration=90\r\n")
        self.run_dialog(False, True, verify)

    def test_inband_pcm_fallback(self):
        def verify(side, peer):
            self.assertEqual(side.send_dtmf("5"), "inband")
            samples = []
            end = time.monotonic() + 1.2
            while time.monotonic() < end:
                try:
                    packet, _ = peer.rtp.recvfrom(8192)
                except socket.timeout:
                    continue
                if (decoded := office.pcmu_rtp_payload(packet)) is not None:
                    samples.extend(office.mulaw_decode(sample) for sample in decoded[1])
            self.assertGreater(sum(abs(sample) for sample in samples), 100000)
            def power(hz):
                return sum(sample * math.cos(math.tau * hz * i / 8000) for i, sample in enumerate(samples))**2 + \
                       sum(sample * math.sin(math.tau * hz * i / 8000) for i, sample in enumerate(samples))**2
            self.assertGreater(power(770), 10 * power(697))
            self.assertGreater(power(1336), 10 * power(1477))
        self.run_dialog(False, False, verify)


if __name__ == "__main__":
    unittest.main()
