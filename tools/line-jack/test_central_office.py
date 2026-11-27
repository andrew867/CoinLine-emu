# SPDX-License-Identifier: GPL-2.0-or-later
import base64
import json
import math
import socket
import struct
import sys
import threading
import time
import unittest
from types import SimpleNamespace
from unittest.mock import Mock, patch

import central_office as office
from sip_media import RtpDtmfSender, RtpWireSocket, TelephoneEventOffer, sdp_answer


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
    def test_direct_pcmu_packet_payload_and_bounded_queue(self):
        payload = bytes([0xff, 0x7f]) * 80
        header = struct.pack("!BBHII", 0x80, 0, 42, 160, 1234)
        self.assertEqual(office.pcmu_rtp_payload(header + payload), (42, payload))
        self.assertIsNone(office.pcmu_rtp_payload(header[:4]))
        self.assertIsNone(office.pcmu_rtp_payload(header[:1] + bytes([8]) + header[2:] + payload))
        padded = bytes([0xa0]) + header[1:] + payload + b"\x00\x02"
        self.assertEqual(office.pcmu_rtp_payload(padded), (42, payload))
        pcm = office.PcmBridge()
        pcm.push_to_phone(struct.pack("<10h", *range(10)), max_pending=5)
        self.assertEqual(pcm.take_to_phone_i16(5), list(range(5, 10)))
        pcm.push_to_phone(struct.pack("<2h", 1, 2))
        self.assertIsNone(pcm.take_to_phone_frame(3))
        pcm.push_to_phone(struct.pack("<h", 3))
        self.assertEqual(pcm.take_to_phone_frame(3), [1, 2, 3])

    def test_direct_pcmu_media_uses_the_reserved_socket_both_directions(self):
        pcm = office.PcmBridge()
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as local, socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as peer:
            local.bind(("127.0.0.1", 0))
            peer.bind(("127.0.0.1", 0))
            local.setblocking(False)
            peer.settimeout(0.1)
            stream = SimpleNamespace(outIP="127.0.0.1", outPort=peer.getsockname()[1],
                                     outSequence=1, outTimestamp=0, outSSRC=1234, NSD=True)
            worker = threading.Thread(target=office.run_direct_rtp, args=(stream, local, pcm), daemon=True)
            worker.start()
            try:
                packet = struct.pack("!BBHII", 0x80, 0, 1, 0, 5678) + bytes([office.mulaw_encode(3000)]) * 160
                peer.sendto(packet, local.getsockname())
                pcm.queue_line_dtmf("A")
                received_tone = False
                deadline = time.monotonic() + 1.0
                while time.monotonic() < deadline and not received_tone:
                    try:
                        wire, source = peer.recvfrom(8192)
                    except socket.timeout:
                        continue
                    self.assertEqual(source, local.getsockname())
                    decoded = office.pcmu_rtp_payload(wire)
                    self.assertIsNotNone(decoded)
                    received_tone = any(office.mulaw_decode(value) != 0 for value in decoded[1])
                self.assertTrue(received_tone)
                received_audio = False
                while time.monotonic() < deadline and not received_audio:
                    received_audio = any(pcm.take_to_phone_i16(160))
                    if not received_audio:
                        time.sleep(0.01)
                self.assertTrue(received_audio)
            finally:
                stream.NSD = False
                worker.join(1.0)

    def test_direct_answer_phases_leave_media_clock_unblocked(self):
        sock = Mock()
        pulse = office.DirectAnswerReversal()
        pulse.start(sock, 0.0)
        pulse.poll(sock, 0.10)
        self.assertEqual([call.args[0] for call in sock.sendall.call_args_list], [b"V0\n"])
        # The loop remains available to deliver audio between polarity phases.
        sock.sendall(b"Waudio\n")
        pulse.poll(sock, 0.15)
        sock.sendall(b"Waudio\n")
        pulse.poll(sock, 0.35)
        self.assertEqual([call.args[0] for call in sock.sendall.call_args_list],
                         [b"V0\n", b"Waudio\n", b"V1\n", b"Waudio\n", b"V\nC1\n"])
        pulse.poll(sock, 1.0)
        self.assertEqual(sock.sendall.call_count, 5)

    def test_reset_voice_queues_removes_previous_dialog_audio(self):
        pcm = office.PcmBridge()
        pcm.push_to_phone(struct.pack("<h", 123))
        pcm.push_from_phone_i16(123)
        pcm.queue_line_dtmf("A")
        pcm.reset_voice_queues()
        self.assertEqual(pcm.take_to_phone_i16(4), [0] * 4)
        self.assertEqual(pcm.take_from_phone(4), bytes(8))

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

    def test_multiple_exact_routes_and_lan_peer_constraints(self):
        routes = [{"digits": ["2374", "7095550174"], "target": "2374", "port": 5070, "host": "10.1.2.3"},
                  {"digits": ["6501", "17095550161"], "target": "6501", "port": 5070, "host": "10.1.2.3"}]
        parsed = office.parse_local_voice_routes(json.dumps(routes))
        self.assertEqual((parsed["6501"].target, parsed["17095550161"].host), ("6501", "10.1.2.3"))
        self.assertIs(parsed["6501"], parsed["17095550161"])
        self.assertNotIn("6504", parsed)
        for digits, target in (("2374", "2374"), ("17095550161", "6501")):
            calls, _ = self.run_office("sip", [(0.0, outpulse(digits)), (2.0, None), (2.0, None)],
                                       route=json.dumps(routes), fail_direct=True)
            self.assertEqual(calls, [target])  # a failed service must not become a carrier call
        calls, _ = self.run_office("local", [(0.0, outpulse("2374")), (2.0, None),
                                     (0.1, b"H0\nX\n"), (0.1, outpulse("6501")), (2.0, None)],
                                   route=json.dumps(routes))
        self.assertEqual(calls, ["2374", "6501"])
        with self.assertRaises(ValueError):
            office.parse_local_voice_routes(json.dumps(routes + [routes[0]]))
        with self.assertRaises(ValueError):
            office.parse_local_voice_routes(json.dumps([{**routes[0], "host": "8.8.8.8"}]))
        with self.assertRaises(ValueError):
            office.parse_local_voice_routes(json.dumps([{**routes[0], "host": "127.0.0.1", "digits": [office.NCC_LOOPBACK]}]))
        self.assertEqual(office.direct_bind_address("127.0.0.1", 5070), "127.0.0.1")

    def test_post_answer_dtmf_is_pcm_with_extended_digits(self):
        pcm = office.PcmBridge()
        pcm.queue_line_dtmf("A")
        samples = struct.unpack("<960h", pcm.take_from_phone(960))
        self.assertTrue(any(samples[:480]))
        self.assertEqual(samples[480:], (0,) * 480)
        def power(hz):
            return sum(samples[i] * math.cos(math.tau * hz * i / 8000) for i in range(480))**2 + \
                   sum(samples[i] * math.sin(math.tau * hz * i / 8000) for i in range(480))**2
        self.assertGreater(power(697), power(770) * 10)
        self.assertGreater(power(1633), power(1477) * 10)

    def test_connected_dtmf_selects_one_transport(self):
        transmit = SimpleNamespace(SENDRECV="sendrecv", SENDONLY="sendonly")
        call_state = SimpleNamespace(ANSWERED="answered")
        modules = {"pyVoIP.RTP": SimpleNamespace(TransmitType=transmit),
                   "pyVoIP.VoIP": SimpleNamespace(CallState=call_state)}
        with patch.dict(sys.modules, modules), patch.object(office, "SIP_DEST", ""), patch.dict(
                office.os.environ, {"COINLINE_SIP_PEER_ONLY": "0", "COINLINE_SIP_DTMF_MODE": "auto"}):
            pcm = office.PcmBridge()
            side = office.SipSide(pcm)
            sender = RtpDtmfSender(TelephoneEventOffer(105, frozenset(range(12))))
            side.call = SimpleNamespace(state="answered", RTPClients=[SimpleNamespace(coinline_dtmf_sender=sender)],
                                        sip=SimpleNamespace(recvLock=threading.RLock()), coinline_dtmf_info=True)
            side._send_dtmf_info = Mock(return_value=True)
            self.assertEqual(side.send_dtmf("1"), "rtp-event")
            self.assertEqual(pcm.take_from_phone(160), bytes(320))
            side._send_dtmf_info.assert_not_called()
            # A is not negotiated in this event set. Explicit peer support permits INFO.
            self.assertEqual(side.send_dtmf("A"), "info")
            side._send_dtmf_info.assert_called_once_with(side.call, "A", 90)
            side.call.coinline_dtmf_info = False
            self.assertEqual(side.send_dtmf("D"), "inband")
            self.assertTrue(any(pcm.take_from_phone(720)))
            side.call.coinline_media_direction = "inactive"
            self.assertIsNone(side.send_dtmf("5"))
            direct = office.SipSide(office.PcmBridge(), office.LocalVoiceRoute.parse(
                '{"digits":["2374"],"target":"2374","port":5069}'))
            self.assertEqual(direct.send_dtmf("A"), "inband")

    def test_connected_digit_dispatch_is_not_a_second_dial(self):
        jack = ScriptedJack([(0.0, outpulse("2025550123")), (2.0, None),
                             (0.1, b"D5\nD#\n"), (0.0, b"")])
        with (
            patch.object(office, "MODE", "sip"), patch.object(office, "SIP_DEST", ""),
            patch.dict(office.os.environ, {"COINLINE_SIP_PEER_ONLY": "0", "COINLINE_CO_INBOUND": "0",
                                           "COINLINE_LOCAL_VOICE_ROUTE": ""}),
            patch.object(office, "connect_jack", return_value=jack),
            patch.object(office, "select", SimpleNamespace(select=jack.select)),
            patch.object(office, "time", SimpleNamespace(monotonic=lambda: jack.now)),
            patch.object(office, "answer_reversal"), patch.object(office.SipSide, "start"),
            patch.object(office.SipSide, "answered", return_value=True),
            patch.object(office.SipSide, "dial") as dial,
            patch.object(office.SipSide, "send_dtmf", return_value="rtp-event") as dtmf,
        ):
            office.run_local_or_sip()
        self.assertEqual([call.args[0] for call in dial.call_args_list], ["+12025550123"])
        self.assertEqual([call.args[0] for call in dtmf.call_args_list], ["5", "#"])

    def test_rfc4733_uses_audio_sequence_timestamp_and_end_redundancy(self):
        sender = RtpDtmfSender(TelephoneEventOffer(110, frozenset(range(12))))
        self.assertTrue(sender.queue("#", 90))
        self.assertFalse(sender.queue("A"))
        self.assertTrue(sender.queue("0", 90))
        stream = SimpleNamespace(coinline_dtmf_sender=sender, coinline_payloads={112: 0})
        sock = Mock()
        wire = RtpWireSocket(sock, stream)
        packets = []
        for index in range(11):
            audio = struct.pack("!BBHII", 0x80, 0, 100 + index, 5000 + index * 160, 42) + bytes(160)
            wire.sendto(audio, ("127.0.0.1", 12345))
            packets.append(sock.sendto.call_args.args[0])
        events = packets[:7]
        self.assertEqual([packet[1] for packet in events], [0x80 | 110] + [110] * 6)
        self.assertEqual([struct.unpack("!H", packet[2:4])[0] for packet in events], list(range(100, 107)))
        self.assertTrue(all(packet[4:8] == struct.pack("!I", 5000) for packet in events))
        self.assertTrue(all(packet[8:12] == struct.pack("!I", 42) for packet in events))
        self.assertEqual([struct.unpack("!BBH", packet[12:]) for packet in events],
                         [(11, 10, 160), (11, 10, 320), (11, 10, 480), (11, 10, 640),
                          (11, 0x8a, 720), (11, 0x8a, 720), (11, 0x8a, 720)])
        self.assertEqual([packet[1] for packet in packets[7:10]], [112] * 3)
        self.assertEqual(packets[10][1], 0x80 | 110)
        self.assertEqual(packets[10][12], 0)

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

    def test_install_answer_survives_early_manager_relay_release(self):
        for mode in ("local", "sip"):
            with self.subTest(mode=mode):
                calls, reversals = self.run_office(mode, [
                    (0.0, b"Q1\n" + outpulse(office.NCC_LOOPBACK)),
                    (0.4, b"Q0\n"),
                    (5.8, None),
                    (0.1, b"H0\nX\n"),
                ])
                self.assertEqual(calls, [])
                self.assertEqual(reversals, 1)

    def test_new_dial_after_early_manager_release_supersedes_pending_answer(self):
        calls, reversals = self.run_office("sip", [
            (0.0, b"Q1\n" + outpulse(office.NCC_LOOPBACK)),
            (0.4, b"Q0\n"),
            (0.2, b"Q1\n" + b"".join(f"D{d}\n".encode() for d in "2025550123")),
            (1.6, None),
            (5.0, None),
        ])
        self.assertEqual(calls, ["+12025550123"])
        self.assertEqual(reversals, 0)

    def test_hook_or_loop_release_cancels_pending_manager_answer(self):
        for release in (b"H0\n", b"X\n"):
            with self.subTest(release=release):
                calls, reversals = self.run_office("sip", [
                    (0.0, b"Q1\n" + outpulse(office.NCC_LOOPBACK)),
                    (0.4, b"Q0\n"), (0.1, release), (6.2, None),
                ])
                self.assertEqual(calls, [])
                self.assertEqual(reversals, 0)

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

    def test_dtmf_info_requires_explicit_peer_capability_and_keeps_dialog(self):
        self.assertFalse(office.supports_dtmf_info(b"SIP/2.0 200 OK\r\nAllow: INVITE, INFO\r\n\r\n"))
        self.assertTrue(office.supports_dtmf_info(b"SIP/2.0 200 OK\r\nAllow: INVITE, INFO\r\nAccept: application/sdp, application/dtmf-relay\r\n\r\n"))
        dialog = office.SipDialog("test", "<sip:lab@example>;tag=local", "<sip:peer@example>;tag=remote",
                                  "sip:peer@192.0.2.31:5092", ("sip:192.0.2.42:5074;lr",), 7)
        body = b"Signal=#\r\nDuration=90\r\n"
        packet, target = dialog.request("INFO", 8, "192.0.2.2", 5087, "z9hG4bK-info", "application/dtmf-relay", body)
        head, content = packet.split(b"\r\n\r\n", 1)
        self.assertEqual(target, ("192.0.2.42", 5074))
        self.assertIn(b"CSeq: 8 INFO\r\n", head)
        self.assertIn(b"Content-Type: application/dtmf-relay\r\n", head)
        self.assertIn(b"Content-Length: " + str(len(body)).encode(), head)
        self.assertEqual(content, body)

    def test_sdp_retains_negotiated_telephone_event_without_exposing_it_as_pcm(self):
        body = (b"v=0\r\no=lab 1 1 IN IP4 192.0.2.1\r\ns=Lab\r\nc=IN IP4 192.0.2.1\r\nt=0 0\r\n"
                b"m=audio 8002 RTP/AVP 112 110\r\na=rtpmap:112 PCMA/8000\r\n"
                b"a=rtpmap:110 telephone-event/8000\r\na=fmtp:110 0-11,16\r\n")
        raw = b"INVITE sip:lab SIP/2.0\r\nContent-Type: application/sdp\r\nContent-Length: " + str(len(body)).encode() + b"\r\n\r\n" + body
        normalized, offer = office.select_g711_media(raw)
        self.assertIn(b"m=audio 8002 RTP/AVP 8\r\n", normalized)
        self.assertNotIn(b"telephone-event", normalized)
        self.assertEqual(offer.telephone_event, TelephoneEventOffer(110, frozenset(range(12))))
        answer = sdp_answer(normalized.decode(), offer, None)
        self.assertIn("m=audio 8002 RTP/AVP 112 110\r\n", answer)
        self.assertIn("a=rtpmap:110 telephone-event/8000\r\n", answer)

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
