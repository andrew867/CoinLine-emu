# M7C telephony-ready branch analysis

Evidence from integration-tree review (behavior only; no proprietary excerpts):

## Which routine shows “telephony board not responding”

The craft interface paints a fixed visual prompt when the **telephony-up terminal flag** is clear (cover/init paths). That matches the observed two-line VFD stall during boot.

## Which routine clears or avoids that message

- **Error-report response path**: alarm-update clears the “telephony not responding” **alarm condition** when the variable-length **error-report** message is handled (opcode class **0xC4** in the IPC decode switch). Payload must checksum.
- **Telephony-up flag**: set when the **telephony status** variable-length message (**0xC0**) is accepted during the **power-on communications sequence**, which signals the init task that telephony information was received; that path sets the **telephony-up** terminal flag.
- **Alarm clear helper**: clearing the alarm condition can assert **telephony-up** when the alarm bit was previously latched.

## Flags / packets

- **Telephony-up**: terminal flag bit (not VFD text).
- **0xC0**: telephony status; length byte **8**, five payload octets plus checksum (matches **TELEPHONY_STATUS_MSG_LENGTH**).
- **0xC4**: error report; length byte **4**, one status octet plus checksum (**TEL_ERROR_REPORT_MSG_LENGTH**).
- **0xC4 alone** does not substitute for **0xC0** for the “got telephony info during power-on” path; both checksum-valid frames are needed for combined alarm/timer behavior and status ingestion.

## Emulator observations (latest uart runs)

- CSIO parser accepts **0xC4** then **0xC0** with correct checksums.
- **M7B** milestone JSON now logs **after both** frames decode (aligned with parser).
- **M7C** still absent: **VFD** remains on the stall text for the full capture window — parser-ready does not imply craft/VFD has redrawn.

## Exact blocker

**VFD stall persists** while CSIO framing is valid — likely **task ordering** (craft paints stall before telephony-up flips) and/or **missing redraw** until a craft signal (e.g. telephony-up toward craft) or another UI refresh path runs.

## Next hardware-faithful steps

1. **Power-on sequencing (CONTTEL2.C / CONTTELC.C)**: During `TELEPHONY_COMM_PWR_ON_SEQ`, **POWER_ON_ACK (`0x72`)** is the path that immediately queues hook/power/error/status queries and enters `TC_PWR_ON_PROCEEDING`. **POWER_ON_RESET (`0x70`)** triggers **suspend_ip_communications** and `TC_PWR_ON_WAIT_FOR_IDLE` — injecting **`0x70` late (e.g. 100 ms)** after **`0x72`** had already started the query burst can **suspend IP comm mid-handshake** and prevent **TELEPHONY_STATUS** from completing the init-task signal path. The driver must not periodically re-inject **`0x72`** on an idle link for the same reason (duplicate ACK signals duplicate query bursts).
2. Trace or infer whether **init telephony-information** runs before craft paints when **INIS_GOT_TEL_INF** fires (RTOS ordering).
3. Extend **ready-decision traces** with **combined** CSIO flags + normalized VFD rows (no injected display text).

Committed JSON mirror (repo ignores `build/`): [`docs/status/generated/m7c-telephony-ready-branch-analysis.json`](generated/m7c-telephony-ready-branch-analysis.json).

## Why CSI/O-valid frames can still leave the stall banner up

`TERMFG_TELEPHONY_UP` is latched from **`INIS_GOT_TEL_INF`** (INITASK) and/or **alarm clear** paths (TERMSUB2 when **`ALM_TEL_NOT_RESPONDING`** clears after a valid **`TELEPHONY_ERROR_REPORT`**).

The **service/UI refresh path** (`SERVTASK.C` **`service_display_update`**) only recomputes **`TELEPHONY_NOT_RESPONDING_MSG`** vs **`BLANK_DISPLAY_MSG`** when the **service-change task** runs with **`SERVS_UPDATE_LEVEL`** / **`SERVS_CHECK_OOS_MSG`**. A flag flip alone does not rewrite the VFD; **RTOS scheduling / service-level updates** must run (related open milestone **M9**).

Craft (`CRFTTASK.C`) paints the stall during **`CFTIFS_POWER_ON_INIT`** when **`TERMFG_TELEPHONY_UP`** is clear; **`INITASK`** does not **`xsignal`** craft when **`INIS_GOT_TEL_INF`** arrives — recovery often depends on **alarm/service** paths or later tasks.
