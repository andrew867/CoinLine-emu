# SPDX-License-Identifier: GPL-2.0-or-later
import base64
import json
import math
import struct
import unittest
from types import SimpleNamespace
from unittest.mock import Mock, patch

import central_office as office


def outpulse(number):
    return b"S\nH1\n" + "".join(f"D{digit}\n" for digit in number).encode("ascii")


class ScriptedJack:
    def __init__(self, steps):
        self.steps = iter(steps)
        self.now = 0.0
        self.data = None
        self.writes = []
        self.sendall = Mock(side_effect=lambda payload: self.writes.append((self.now, payload)))
        self.settimeout = Mock()

    def select(self, *_):
        elapsed, self.data = next(self.steps)
        self.now += elapsed
        return ([self] if self.data is not None else []), [], []

    def recv(self, _):
        return self.data


class OutboundRoutingTest(unittest.TestCase):
    def run_office(self, mode, steps, target="", inbound=False, route="", fail_direct=False):
        jack = ScriptedJack(steps + [(0.0, b"")])
        with (
            patch.object(office, "MODE", mode),
            patch.object(office, "EXPECT", "5550123"),
            patch.object(office, "SIP_DEST", target),
            patch.dict(office.os.environ, {"COINLINE_SIP_PEER_ONLY": "0", "COINLINE_CO_INBOUND": str(int(inbound)),
                                           "COINLINE_LOCAL_VOICE_ROUTE": route}),
            patch.object(office, "connect_jack", return_value=jack),
            patch.object(office, "select", SimpleNamespace(select=jack.select)),
            patch.object(office, "time", SimpleNamespace(monotonic=lambda: jack.now)),
            patch.object(office, "answer_reversal") as reversal,
            patch.object(office.SipSide, "start"),
            patch.object(office.SipSide, "answered", return_value=False),
            patch.object(office.SipSide, "dial", side_effect=(OSError("unavailable") if fail_direct else None)) as dial,
        ):
            office.run_local_or_sip()
        self.writes = [call.args[0] for call in jack.sendall.call_args_list]
        self.timed_writes = jack.writes
        return [call.args[0] for call in dial.call_args_list], reversal.call_count

    def test_exact_local_route_has_precedence_and_no_fallback(self):
        route = json.dumps({"digits": ["2374", "7095550174"], "target": "2374", "port": 5069})
        calls, _ = self.run_office("sip", [(0.0, outpulse("7095550174")), (2.0, None), (2.0, None)],
                                   route=route, fail_direct=True)
        self.assertEqual(calls, ["2374"])
        calls, _ = self.run_office("sip", [(0.0, outpulse("7095550175")), (2.0, None)], route=route)
        self.assertEqual(calls, ["+17095550175"])
        calls, _ = self.run_office("local", [(0.0, outpulse("2374")), (2.0, None)], route=route)
        self.assertEqual(calls, ["2374"])

    def test_loopback_manager_number_cannot_be_voice_alias(self):
        with self.assertRaises(ValueError):
            office.LocalVoiceRoute.parse(json.dumps({"digits": [office.NCC_LOOPBACK], "target": "2374", "port": 5069}))

    def test_post_answer_dtmf_is_pcm_with_extended_digits(self):
        pcm = office.PcmBridge()
        pcm.queue_line_dtmf("A")
        samples = struct.unpack("<1040h", pcm.take_from_phone(1040))
        self.assertTrue(any(samples[:640]))
        self.assertEqual(samples[640:], (0,) * 400)
        def power(hz):
            return sum(samples[i] * math.cos(math.tau * hz * i / 8000) for i in range(640))**2 + \
                   sum(samples[i] * math.sin(math.tau * hz * i / 8000) for i in range(640))**2
        self.assertGreater(power(697), power(770) * 10)
        self.assertGreater(power(1633), power(1477) * 10)

    def test_sip_calls_each_outpulsed_number(self):
        calls, reversals = self.run_office("sip", [
            (0.0, outpulse("2025550123")),
            (1.6, None),
            (2.0, None),
            (0.0, b"H0\nX\n" + outpulse("12025550124")),
            (1.6, None),
            (0.0, b"H0\nX\n" + outpulse("011442079460958")),
            (1.6, None),
        ])
        self.assertEqual(calls, ["+12025550123", "+12025550124", "+442079460958"])
        self.assertEqual(reversals, 0)

    def test_manager_supervision_stays_on_the_jack(self):
        calls, reversals = self.run_office("sip", [
            (0.0, outpulse(office.NCC_LOOPBACK)),
            (2.0, None),
            (4.1, None),
        ])
        self.assertEqual(calls, [])
        self.assertEqual(reversals, 1)

    def test_manager_releases_without_handset_loop_release(self):
        calls, reversals = self.run_office("sip", [
            (0.0, b"Q1\n" + outpulse(office.NCC_LOOPBACK)),
            (6.1, None),
            (0.0, b"Q0\n" + b"".join(f"D{d}\n".encode() for d in "2025550123")),
            (1.6, None),
        ])
        self.assertEqual(calls, ["+12025550123"])
        self.assertEqual(reversals, 1)

    def test_jack_labels_dial_tone_until_the_first_digit(self):
        self.run_office("local", [(0.0, b"S\nH1\n"), (0.1, b"D5\n")])
        tone_on = self.writes.index(b"T1\n")
        audio = next(i for i, data in enumerate(self.writes) if data.startswith(b"W"))
        tone_off = self.writes.index(b"T0\n")
        self.assertLess(tone_on, audio)
        self.assertLess(audio, tone_off)

    def test_fixed_target_and_local_office(self):
        steps = [(0.0, outpulse("2025550123")), (1.6, None),
                 (0.0, b"H0\nX\n" + outpulse("5550123")), (1.6, None)]
        self.assertEqual(self.run_office("sip", steps, "+12025550124"), (["+12025550124"], 0))
        self.assertEqual(self.run_office("local", steps), ([], 1))

    def test_ring_cadence_and_answer_in_silent_interval(self):
        _, reversals = self.run_office("local", [
            (0.0, b"H0\nE1\n"), (2.0, None), (4.0, None), (2.0, None), (1.0, b"H1\n"), (4.0, None),
        ], inbound=True)
        ring_edges = [(when, data) for when, data in self.timed_writes if data in (b"R1\n", b"R0\n")]
        self.assertEqual(ring_edges, [(0.0, b"R1\n"), (2.0, b"R0\n"), (6.0, b"R1\n"), (8.0, b"R0\n")])
        self.assertEqual(reversals, 1)

    def test_sdp_advertises_media_address_without_changing_sip_contact(self):
        body = "v=0\r\nc=IN IP4 10.0.0.2\r\nm=audio 12000 RTP/AVP 0\r\n"
        message = f"SIP/2.0 200 OK\r\nContact: <sip:lab@10.0.0.2>\r\nContent-Length: {len(body)}\r\n\r\n{body}"
        headers, updated = office.advertise_rtp_address(message, "192.0.2.10").split("\r\n\r\n")
        self.assertIn("Contact: <sip:lab@10.0.0.2>", headers)
        self.assertIn(f"Content-Length: {len(updated)}", headers)
        self.assertIn("c=IN IP4 192.0.2.10\r\n", updated)


class SipServerResponseTest(unittest.TestCase):
    def test_bye_keeps_dialog_route_tags_and_uses_a_new_local_sequence(self):
        dialog = office.SipDialog(
            "test", "<sip:lab@example>;tag=local", "<sip:caller@example>;tag=remote",
            "sip:caller@192.0.2.31:5092", ("sip:192.0.2.42:5074;lr", "sip:192.0.2.43;lr"), 0,
            remote_sequence=57530,
        )
        packet, peer = dialog.request("BYE", 1, "192.0.2.2", 5087, "z9hG4bK-new")
        self.assertEqual(peer, ("192.0.2.42", 5074))
        self.assertTrue(packet.startswith(b"BYE sip:caller@192.0.2.31:5092 SIP/2.0\r\n"))
        self.assertIn(b"From: <sip:lab@example>;tag=local\r\n", packet)
        self.assertIn(b"To: <sip:caller@example>;tag=remote\r\n", packet)
        self.assertIn(b"CSeq: 1 BYE\r\nRoute: <sip:192.0.2.42:5074;lr>\r\nRoute: <sip:192.0.2.43;lr>", packet)

    def test_normalize_compact_headers_and_select_the_audio_media_connection(self):
        body = (b"v=0\r\no=fixture 1 1 IN IP4 192.0.2.1\r\ns=Test\r\nc=IN IP4 192.0.2.1\r\nt=0 0\r\n"
                b"m=video 8000 RTP/AVP 99\r\nm=audio 8002 RTP/AVP 101 8 0\r\nc=IN IP4 192.0.2.2\r\na=sendonly\r\n")
        raw = (b"INVITE sip:lab SIP/2.0\r\ni: fixture\r\ncsEQ:\t99   INVITE\r\nc: application/sdp\r\nl: "
               + str(len(body)).encode() + b"\r\n\r\n" + body)
        packet, media_layout = office.select_g711_media(office.normalize_sip_packet(raw))
        hdr, sdp = packet.split(b"\r\n\r\n")
        self.assertIn(b"Call-ID: fixture\r\nCSeq: 99 INVITE", hdr)
        self.assertIn(b"Content-Length: " + str(len(sdp)).encode(), hdr)
        self.assertIn(b"c=IN IP4 192.0.2.2\r\nm=audio 8002 RTP/AVP 8 0", sdp)
        self.assertNotIn(b"m=video", sdp)
        self.assertEqual(media_layout.layout, (b"m=video 0 RTP/AVP 99", None))

    def test_select_secure_stream_or_fall_back_to_plain_in_dual_offer(self):
        def invite(suite):
            key = base64.b64encode(bytes(30))
            body = (b"v=0\r\no=lab 1 1 IN IP4 192.0.2.1\r\ns=Lab\r\nc=IN IP4 192.0.2.1\r\nt=0 0\r\n"
                    b"m=audio 11000 RTP/SAVP 0 8 101\r\na=crypto:1 " + suite + b" inline:" + key
                    + b"\r\nm=audio 11002 RTP/AVP 0 8 101\r\n")
            return (b"INVITE sip:lab SIP/2.0\r\nContent-Type: application/sdp\r\nContent-Length: "
                    + str(len(body)).encode() + b"\r\n\r\n" + body)

        _, secure = office.select_g711_media(invite(b"AES_CM_128_HMAC_SHA1_80"))
        self.assertEqual(secure.profile, b"RTP/SAVP")
        self.assertEqual(secure.layout, (None, b"m=audio 0 RTP/AVP 0 8 101"))
        _, plain = office.select_g711_media(invite(b"UNSUPPORTED"))
        self.assertEqual(plain.profile, b"RTP/AVP")
        self.assertEqual(plain.layout, (b"m=audio 0 RTP/SAVP 0 8 101", None))

    def test_inbound_answer_uses_invite_socket_and_stays_stable_until_ack(self):
        udp = Mock()
        replies = office.SipReplySocket(udp)
        replies.contact = b"<sip:lab@192.0.2.20:5099>"
        registrar = ("192.0.2.30", 5060)
        caller = ("192.0.2.31", 5072)
        via = {"address": ("192.0.2.31", "5060"), "branch": "z9hG4bK-test", "rport": None}
        invite = SimpleNamespace(method="INVITE", raw=b"INVITE sip:lab SIP/2.0\r\n"
                                 b"Record-Route: <sip:edge.example;lr>\r\n\r\n",
                                 headers={"Call-ID": "example", "CSeq": {"check": "1"}, "Via": [via]})
        answer = (b"SIP/2.0 200 OK\r\nVia: SIP/2.0/UDP 192.0.2.31:5060;branch=z9hG4bK-test\r\n"
                  b"To: <sip:lab@example>;tag=answer\r\nCall-ID: example\r\nCSeq: 1 INVITE\r\n"
                  b"Contact: <sip:lab@10.0.0.2:5087>\r\nContent-Length: 0\r\n\r\n")
        with patch.object(office.time, "monotonic", return_value=0.0) as clock:
            self.assertTrue(replies.accept_request(invite, caller))
            self.assertEqual((via["received"], via["rport"]), ("192.0.2.31", "5072"))
            replies.sendto(answer, registrar)
            final, target = udp.sendto.call_args.args
            self.assertEqual(target, caller)
            self.assertIn(b"Record-Route: <sip:edge.example;lr>", final)
            self.assertIn(b"Contact: <sip:lab@192.0.2.20:5099>", final)
            self.assertFalse(replies.accept_request(invite, caller))
            udp.sendto.assert_called_with(final, caller)
            clock.return_value = 0.5
            replies.retry_answers()
            self.assertEqual(udp.sendto.call_count, 3)
            ack = SimpleNamespace(method="ACK", headers={"Call-ID": "example", "CSeq": {"check": "1"},
                                  "Via": [via], "To": {"tag": "answer"}})
            self.assertFalse(replies.accept_request(ack, caller))
            clock.return_value = 4.0
            replies.retry_answers()
            self.assertEqual(udp.sendto.call_count, 3)


if __name__ == "__main__":
    unittest.main()
