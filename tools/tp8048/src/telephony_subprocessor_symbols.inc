; -----------------------------------------------------------------------------
; telephony_subprocessor_symbols.inc
; Behavioral PCD3349A / MCS-48 TP firmware constants.
; Syntax is intentionally conservative MCS-48 style. Some assemblers may require
; EQU syntax tweaks.
; -----------------------------------------------------------------------------

; Logical RAM map, internal 224-byte RAM
RX_HEAD     EQU 020H
RX_TAIL     EQU 021H
TX_HEAD     EQU 022H
TX_TAIL     EQU 023H
TP_STATE    EQU 024H
FLAGS0      EQU 025H
FLAGS1      EQU 026H
MISS_CNT    EQU 027H
RELAY_CP_SEIZED EQU 028H       ; 0 = RELEASE_HOOK_SWITCH_RELAY (local/OOS path); non-zero after SEIZE_HOOK_SWITCH_RELAY
TICK_10MS   EQU 029H
TICK_100MS  EQU 02AH
TICK_1S     EQU 02BH
HOOK_STABLE EQU 02CH
HOOK_LAST   EQU 02DH
HOOK_CNT    EQU 02EH
KEY_LAST    EQU 02FH
KEY_STABLE  EQU 030H
KEY_CNT     EQU 031H
KEY_HELD    EQU 032H
KEY_REL_HI  EQU 033H         ; upper nibble: mute/sidetone scratch for behavioural model
KEY_REL_LO  EQU 034H
VOL_IDX     EQU KEY_REL_LO   ; amplified volume step 0..3 (PRESET / UP / DOWN)
MUTE_FLAGS  EQU KEY_REL_HI   ; host-visible mute scratch (TX/RX)
TONE_MODE   EQU 035H
CP_CMD      EQU 036H
; Multi-byte TP config (CONTTELC.C telephony_config_msg_encode): header 0xC0, len 126, 124-byte body.
CFG_NEED    EQU 037H        ; bytes remaining in current C0 frame (0 = idle)
CFG_IDX     EQU 038H        ; next write offset 0..125 into CFG_BLK
TMP0        EQU 039H
TMP1        EQU 03AH
TMP2        EQU 03BH
TMP3        EQU 03CH        ; scratch; also PAD_FLAGS (DTMF pad enable, tone gen off) when CFG ingest idle
TMP_SUM     EQU 03DH        ; scratch: running sum for config checksum verify
; Hook sequencing (CONTTELC telephony_msg_decode_task): CP expects transition then steady state.
; After RESP_HOOK_TRANS_*, defer RESP_HOOK_ON/OFF by HOOK_STATE_DELAY_IRQs timer IRQs (~50–100 ms).
HOOK_SEQ_REM   EQU 03EH     ; 0 = idle; countdown per TIMER_IRQ
HOOK_SEQ_BYTE  EQU 03FH     ; second opcode (ON_HOOK_STATE / OFF_HOOK_STATE)

; Simple ring buffer layout (TX shrunk to free 0x60.. for 126-byte config image)
RX_BUF      EQU 040H      ; 16 bytes: CP -> TP accepted bytes
TX_BUF      EQU 050H      ; 16 bytes: TP -> CP queued bytes
; CONTTELC.H TELEPHONY_CONFIG_MSG_LENGTH = 126 (hdr + len field + 124 data bytes incl. trailing sum8)
CFG_BLK       EQU 060H              ; base of mirrored frame (126 bytes occupy 060h..0ddh)
TELE_CFG_LEN  EQU 07EH             ; 126 decimal, expected in CFG_BLK+1
CFG_CHK_ADDR  EQU CFG_BLK+07DH     ; index 125: CP sum8 over bytes [2..124]
CFG_LEN_ADDR  EQU CFG_BLK+01H      ; telephony wire length byte (must be TELE_CFG_LEN)
CFG_PAY0      EQU 002H             ; first summed payload byte index

; TP states
ST_RESET_PENDING     EQU 00H
ST_BOOT_ACK_VISIBLE  EQU 01H
ST_INIT_DIALOGUE     EQU 02H
ST_RUNTIME_ACTIVE    EQU 03H
ST_TIMEOUT_LATCHED   EQU 04H
ST_CRAFT_WINDOW      EQU 05H

; FLAGS0 bits
F_LINK_UP       EQU 01H
F_BOOT_ACK_SENT EQU 02H
F_READY         EQU 04H
F_FAULT         EQU 08H
F_OOS           EQU 10H
F_VOICE_ACTIVE  EQU 20H
F_TX_PENDING    EQU 40H
F_CFG_ACTIVE    EQU 80H

; FLAGS1 bits
F_HOOK_OFF      EQU 01H
F_LINE_OK       EQU 02H
F_SERVICE       EQU 04H
F_SECURITY_OK   EQU 08H
F_KEY_PENDING   EQU 10H
F_CRAFT_SEEN    EQU 20H
F_TONE_ACTIVE   EQU 40H
F_RESERVED      EQU 80H

; CP -> TP command bytes (single-byte queries / controls)
CMD_HOOK_STATUS EQU 030H
CMD_TEL_STATUS  EQU 031H
CMD_NEXT_CALL_IDLE EQU 032H
CMD_POWER_STAT  EQU 033H
CMD_VER_QUERY   EQU 034H
CMD_CLR_STAT    EQU 035H
CMD_CLR_DTMF    EQU 036H
CMD_CO_LINE     EQU 037H
CMD_ERROR_RPT   EQU 038H
CMD_CLR_ERR     EQU 039H
CMD_KEY_MATRIX  EQU 03AH
CMD_HANDSET     EQU 03BH
CMD_REL_RELAY   EQU 03CH
CMD_SEIZE_RELAY EQU 03DH
CMD_CHGFDBK     EQU 03EH     ; CHANGE_DTMF_FEEDBACK_CONTROL — accepted, no TP reply
CMD_CONFIG      EQU 0C0H

; TP -> CP single-byte power-on codes (IPCODES.H: POWER_ON_RESET / POWER_ON_ACK)
; On-hook idle after TP reset completes -> RESET (070H). Off-hook / call-survivor path -> ACK (072H).
RESP_BOOT_PWRON_ACK EQU 072H ; POWER_ON_ACK
RESP_BOOT_RESET     EQU 070H ; POWER_ON_RESET
RESP_CONFIG_ACK     EQU 074H
RESP_POWER_OK       EQU 07AH
RESP_PWR_FAIL       EQU 07CH
RESP_CO_LINE_1      EQU 080H ; nominal CO line status / line idle reply family
RESP_HOOK_ON        EQU 06CH
RESP_HOOK_OFF       EQU 06EH
RESP_HOOK_TRANS_ON  EQU 060H
RESP_HOOK_TRANS_OFF EQU 062H
RESP_LINE_CONNECT   EQU 066H
RESP_LINE_BREAK     EQU 064H
KEY_MATRIX_IDLE     EQU 088H
KEY_MATRIX_ACTIVE   EQU 08AH
HANDSET_CONT_OK     EQU 08CH
HANDSET_CONT_BAD    EQU 08EH
KEY_RELEASE         EQU 05EH

; Timer IRQs between hook transition and hook steady-state (~80 ms if IRQ ~10 ms)
HOOK_STATE_DELAY_IRQS EQU 008H

; Framed TP→CP messages — length byte (2nd octet) MUST equal CONTTELC.H value:
;   telephony_misc_msg_decode() compares rd_cbuffer() to TELEPHONY_STATUS_MSG_LENGTH /
;   TEL_ERROR_REPORT_MSG_LENGTH / TEL_VERSION_NUM_MSG_LENGTH (total frame size incl. opcode).
; CONTTELC.H lines 159–161
TELEPHONY_STATUS_MSG_LENGTH   EQU 008H
TEL_ERROR_REPORT_MSG_LENGTH   EQU 004H
TEL_VERSION_NUM_MSG_LENGTH    EQU 00EH

; Framed TP responses (IPCODES.H / CONTTELC)
FRAME_STATUS         EQU 0C0H
FRAME_EXT_STATUS     EQU 0C2H
FRAME_ERROR_REPORT   EQU 0C4H
LEN_STATUS           EQU TELEPHONY_STATUS_MSG_LENGTH
LEN_ERROR            EQU TEL_ERROR_REPORT_MSG_LENGTH
LEN_EXT_STATUS       EQU TEL_VERSION_NUM_MSG_LENGTH

; CONTTELC.H — telephony_error_status bits (TP -> CP in 0xC4 frame)
TELEPHONY_CHECKSUM_ERROR EQU 001H

; Keypad TP→CP wire opcodes (IPCODES.H — digit 'A' is 0x20; digit 1 is 0x22; 0 is 0x34; * / # / B–D follow header)
KEY_A EQU 020H
KEY_0 EQU 034H
KEY_1 EQU 022H
KEY_2 EQU 024H
KEY_3 EQU 026H
KEY_4 EQU 028H
KEY_5 EQU 02AH
KEY_6 EQU 02CH
KEY_7 EQU 02EH
KEY_8 EQU 030H
KEY_9 EQU 032H
KEY_STAR EQU 036H
KEY_OCTO EQU 038H
KEY_B EQU 03AH
KEY_C EQU 03CH
KEY_D EQU 03EH

; Tone modes (low nibble of TONE_MODE; high nibble optional flags)
TONE_NONE     EQU 00H
TONE_DIAL     EQU 01H
TONE_NIS      EQU 02H
TONE_DTMF     EQU 03H

; PAD_FLAGS (TMP3) bits — cleared in CLEAR_RAM; refreshed by CP commands 2D–2F
P_PAD_DTMF_DIS    EQU 01H     ; 1 = dial-pad DTMF suppressed (CP DIAL_PAD_DTMF_DISABLE)
TONE_GEN_OFF_Q    EQU 02H     ; TONE_GENERATOR_OFF latched
SIDETONE_ON_Q     EQU 04H     ; SIDETONE_SUPPRESSION_ON (behavioural latch)

; CP→TP control opcode bases (IPCODES.H)
CP_DTMF_MIN   EQU 10H
CP_DTMF_MAX   EQU 1FH
CP_PAUSE      EQU 2CH
CP_PAD_DIS    EQU 2DH
CP_PAD_EN     EQU 2EH
CP_TONE_OFF   EQU 2FH
CP_PRESET_VOL EQU 20H
CP_CALL_IDLE  EQU 40H
CP_CALL_MAX   EQU 46H

; PCD3349A derivative registers, write-only via MOVD Dx,A on supported assemblers
DER_HGF EQU 01H
DER_LGF EQU 02H

; DTMF register values from PCD3349A table at 3.58 MHz
DTMF_LOW_0 EQU 0A3H
DTMF_LOW_1 EQU 0DDH
DTMF_LOW_2 EQU 0DDH
DTMF_LOW_3 EQU 0DDH
DTMF_LOW_4 EQU 0C8H
DTMF_LOW_5 EQU 0C8H
DTMF_LOW_6 EQU 0C8H
DTMF_LOW_7 EQU 0B5H
DTMF_LOW_8 EQU 0B5H
DTMF_LOW_9 EQU 0B5H
DTMF_HIGH_0 EQU 072H
DTMF_HIGH_1 EQU 07FH
DTMF_HIGH_2 EQU 072H
DTMF_HIGH_3 EQU 067H
DTMF_HIGH_4 EQU 07FH
DTMF_HIGH_5 EQU 072H
DTMF_HIGH_6 EQU 067H
DTMF_HIGH_7 EQU 07FH
DTMF_HIGH_8 EQU 072H
DTMF_HIGH_9 EQU 067H

; Approximate TP tone registers from musical/modem table, used as first-pass emulation
; NIS target is 480 + 620 Hz, approximated by nearby musical/modem values.
TONE_NIS_LOW_REG  EQU 0FFH     ; placeholder, backend may synthesize exact 480 Hz in C++
TONE_NIS_HIGH_REG EQU 0F8H     ; D#5 around 622.5 Hz
TONE_DIAL_LOW_REG EQU 0FFH     ; backend may synthesize exact 350 Hz in C++
TONE_DIAL_HIGH_REG EQU 0EAH    ; E5 around 659.5 Hz, not final dialtone, trace only
