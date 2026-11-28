#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Compare completed, isolated dual-flash snapshots without changing them."""

import argparse
import hashlib
import json
from pathlib import Path

from firmware_download_plan import firmware_applied_target

BLOCK_BYTES = 4096
CHIP_BYTES = 524288
IMAGE_BYTES = CHIP_BYTES * 2
BLOCK_COUNT = IMAGE_BYTES // BLOCK_BYTES


def digest(data):
    return hashlib.sha256(data).hexdigest()


def image(path):
    data = path.read_bytes()
    if len(data) != IMAGE_BYTES:
        raise ValueError("A mapped dual-flash image must contain exactly 1 MiB.")
    return data


def persisted(path, seed_sha256):
    raw = path.read_bytes()
    if len(raw) != 64 + IMAGE_BYTES or raw[:64] != seed_sha256.encode("ascii"):
        raise ValueError("Persistent flash length or seed binding does not match the original image.")
    return raw[64:]


def block(data, index):
    return data[index * BLOCK_BYTES:(index + 1) * BLOCK_BYTES]


def json_lines(path):
    records = []
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip():
            continue
        try:
            records.append(json.loads(line))
        except json.JSONDecodeError as error:
            raise ValueError(f"Invalid JSONL record {number} in {path.name}: {error}") from error
    return records


def flash_trace_qualification(path, changed_blocks):
    records = json_lines(path)
    summaries = [record for record in records if record.get("event") == "summary"]
    program = [record for record in records if record.get("event") == "program_begin"]
    program_complete = [record for record in records if record.get("event") == "program_complete"]
    erase = [record for record in records if record.get("event") == "erase_begin"]
    erase_complete = [record for record in records if record.get("event") == "erase_complete"]
    program_blocks = sorted({record.get("block_4k") for record in program
                             if type(record.get("block_4k")) is int})
    errors = [record for record in program if record.get("program_error") is True]
    summary = summaries[-1] if summaries else {}
    counts_match = bool(summary) and all((
        summary.get("program_begin") == len(program),
        summary.get("program_complete") == len(program_complete),
        summary.get("erase_begin") == len(erase),
        summary.get("erase_complete") == len(erase_complete),
    ))
    complete = (bool(program) and len(program) == len(program_complete)
                and bool(erase) and len(erase) == len(erase_complete))
    changed_programmed = set(changed_blocks).issubset(program_blocks)
    return {
        "program_begin": len(program),
        "program_complete": len(program_complete),
        "erase_begin": len(erase),
        "erase_complete": len(erase_complete),
        "program_blocks": program_blocks,
        "program_errors": len(errors),
        "summary_counts_match": counts_match,
        "changed_blocks_programmed": changed_programmed,
        "qualified": counts_match and complete and changed_programmed and not errors,
    }


def boot_qualification(path):
    ready = [record for record in json_lines(path) if record.get("milestone") == "protocol_ready"]
    return bool(ready and ready[-1].get("telephony_ready") is True
                and ready[-1].get("ready_sequence_completed") is True)


def receiver_qualification(events_path, keypad_path):
    actions = [line.split("\t", 2)[-1].strip()
               for line in events_path.read_text(encoding="utf-8").splitlines()
               if "\tinput\t" in line]
    try:
        off = actions.index("hook off")
        on = actions.index("hook on", off + 1)
    except ValueError:
        return False
    keypad = json_lines(keypad_path)
    reported = any(record.get("event") == "tp_key_event_reported_to_cp"
                   and record.get("reason") == "line_connection" for record in keypad)
    return on > off and reported


def production_job_qualification(path, expected_firmware_id, target_sha256, requested_blocks):
    evidence = json.loads(path.read_text(encoding="utf-8"))
    requested = sorted(set(requested_blocks))
    return {
        "source": evidence.get("source"),
        "firmware_id": evidence.get("firmware_id"),
        "artifact_sha256": evidence.get("artifactSha256"),
        "requested_blocks": evidence.get("requestedBlocks"),
        "delivered_blocks": evidence.get("deliveredBlocks"),
        "xmodem_packets": evidence.get("xmodemPackets"),
        "qualified": (evidence.get("source") == "coinline-production"
                      and evidence.get("phase") == "code-server-transfer-complete"
                      and evidence.get("success") is True
                      and evidence.get("firmware_id") == expected_firmware_id
                      and evidence.get("artifactSha256") == target_sha256
                      and sorted(evidence.get("requestedBlocks", [])) == requested
                      and sorted(evidence.get("deliveredBlocks", [])) == requested
                      and type(evidence.get("xmodemPackets")) is int
                      and evidence.get("xmodemPackets") > 0),
    }


def version_display_qualification(path, expected_version):
    records = json_lines(path)
    matching = [record for record in records
                if any(expected_version in row for row in record.get("text", [])
                       if isinstance(row, str))]
    return {
        "expected_version": expected_version,
        "matching_snapshots": len(matching),
        "qualified": bool(matching),
    }


def summarize(seed, before, after, expected=None, expected_blocks=None, *,
              flash_trace=None, post_restart=None, boot_ready=False,
              receiver_ready=False, production_job=None, version_display=None):
    for data in (seed, before, after, expected):
        if data is not None and len(data) != IMAGE_BYTES:
            raise ValueError("Every compared image must be exactly 1 MiB.")
    if expected_blocks is not None and expected is None:
        raise ValueError("Expected block indices require an independently supplied target image.")
    requested = None
    expected_active = None
    if expected is not None:
        indices = range(BLOCK_COUNT) if expected_blocks is None else expected_blocks
        if any(type(index) is not int or not 0 <= index < BLOCK_COUNT for index in indices):
            raise ValueError("Block indices must be integers in the mapped dual-flash range.")
        requested = sorted(set(indices))
        if not requested:
            raise ValueError("Select at least one expected block.")
        expected_active = (expected if expected_blocks is None
                           else firmware_applied_target(before, expected, requested))
    changed = [index for index in range(BLOCK_COUNT) if block(before, index) != block(after, index)]
    result = {
        "block_bytes": BLOCK_BYTES,
        "image_bytes": IMAGE_BYTES,
        "seed_sha256": digest(seed),
        "before_sha256": digest(before),
        "after_sha256": digest(after),
        "changed_blocks": changed,
        "chips": [
            {"chip": index, "before_sha256": digest(before[index * CHIP_BYTES:(index + 1) * CHIP_BYTES]),
             "after_sha256": digest(after[index * CHIP_BYTES:(index + 1) * CHIP_BYTES])}
            for index in range(2)
        ],
        "expected": None,
        "service_and_restart_qualified": False,
        "qualified": False,
    }
    if expected is not None:
        mismatches = [index for index in requested if block(expected, index) != block(after, index)]
        result["expected"] = {
            "image_sha256": digest(expected),
            "requested_blocks": requested,
            "mismatched_blocks": mismatches,
            "matches_requested_blocks": not mismatches,
            "changed_outside_requested_blocks": sorted(set(changed) - set(requested)),
            "full_image_matches": expected == after,
            "expected_active_sha256": digest(expected_active),
            "active_image_matches_retention_policy": expected_active == after,
        }
    flash_qualified = bool(flash_trace and flash_trace.get("qualified") is True)
    post_restart_matches = bool(expected_active is not None and post_restart == expected_active)
    production_qualified = bool(production_job and production_job.get("qualified") is True)
    version_qualified = bool(version_display and version_display.get("qualified") is True)
    result["qualification"] = {
        "active_image_matches_registered_retention_policy": bool(
            result["expected"] and result["expected"]["active_image_matches_retention_policy"]),
        "guest_flash_operations": flash_trace,
        "production_job": production_job,
        "post_restart_flash_matches_active_image": post_restart_matches,
        "post_restart_boot_ready": bool(boot_ready),
        "post_restart_receiver_cycle": bool(receiver_ready),
        "post_restart_version_display": version_display,
    }
    result["service_and_restart_qualified"] = (
        post_restart_matches and boot_ready and receiver_ready and version_qualified
    )
    result["qualified"] = (
        result["qualification"]["active_image_matches_registered_retention_policy"]
        and flash_qualified and production_qualified and result["service_and_restart_qualified"]
    )
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seed", type=Path, required=True, help="original mapped flash input")
    parser.add_argument("--before", type=Path, help="initial saved program_flash, if different from seed")
    parser.add_argument("--after", type=Path, required=True, help="saved program_flash from the completed lab run")
    parser.add_argument("--expected-image", type=Path, help="independently approved mapped target image")
    parser.add_argument("--expected-block", type=int, action="append", help="expected zero-based 4 KiB block; repeat as needed")
    parser.add_argument("--download-plan", type=Path,
                        help="registered source-to-target block plan from firmware_download_plan.py")
    parser.add_argument("--flash-trace", type=Path, help="guest-issued NOR operation JSONL from the transfer run")
    parser.add_argument("--post-restart-flash", type=Path,
                        help="saved program_flash after a separate post-update restart")
    parser.add_argument("--post-restart-boot-trace", type=Path, help="boot JSONL from the post-update restart")
    parser.add_argument("--post-restart-events", type=Path, help="action events from the post-update restart")
    parser.add_argument("--post-restart-keypad-trace", type=Path,
                        help="firmware-visible keypad JSONL from the post-update restart")
    parser.add_argument("--production-job-evidence", type=Path,
                        help="production CoinLine completed-job JSON")
    parser.add_argument("--post-restart-vfd-snapshots", type=Path,
                        help="native VFD JSONL containing the target version banner")
    parser.add_argument("--expected-firmware-id", help="registered target identifier")
    parser.add_argument("--expected-version", help="version expected from the registered target")
    parser.add_argument("--output", type=Path, required=True, help="new JSON evidence report")
    args = parser.parse_args()
    seed = image(args.seed)
    initial = persisted(args.before, digest(seed)) if args.before else seed
    final = persisted(args.after, digest(seed))
    expected = image(args.expected_image) if args.expected_image else None
    plan = json.loads(args.download_plan.read_text(encoding="utf-8")) if args.download_plan else None
    requested_blocks = plan.get("requested_blocks") if plan else args.expected_block
    if plan:
        if expected is None:
            parser.error("--download-plan requires --expected-image")
        if plan.get("kind") != "registered-image-delta":
            parser.error("--download-plan is not a registered image delta")
        if plan.get("source_sha256") != digest(initial) or plan.get("target_sha256") != digest(expected):
            parser.error("--download-plan source or target digest does not match the supplied images")
    flash = flash_trace_qualification(args.flash_trace, [index for index in range(BLOCK_COUNT)
                                      if block(initial, index) != block(final, index)]) if args.flash_trace else None
    restarted = persisted(args.post_restart_flash, digest(seed)) if args.post_restart_flash else None
    boot_ready = boot_qualification(args.post_restart_boot_trace) if args.post_restart_boot_trace else False
    receiver_ready = receiver_qualification(args.post_restart_events, args.post_restart_keypad_trace) \
        if args.post_restart_events and args.post_restart_keypad_trace else False
    production = production_job_qualification(args.production_job_evidence,
                                               args.expected_firmware_id, digest(expected), requested_blocks) \
        if (args.production_job_evidence and args.expected_firmware_id and expected is not None
            and requested_blocks) else None
    version = version_display_qualification(args.post_restart_vfd_snapshots, args.expected_version) \
        if args.post_restart_vfd_snapshots and args.expected_version else None
    report = summarize(seed, initial, final, expected, requested_blocks,
                       flash_trace=flash, post_restart=restarted, boot_ready=boot_ready,
                       receiver_ready=receiver_ready, production_job=production,
                       version_display=version)
    if plan and report["expected"] and plan.get("expected_active_sha256") != report["expected"]["expected_active_sha256"]:
        parser.error("--download-plan expected active digest does not match the evidence calculation")
    # Do not overwrite earlier evidence or any input file.
    with args.output.open("x", encoding="utf-8") as output:
        json.dump(report, output, indent=2)
        output.write("\n")
    return 0 if report["qualified"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
