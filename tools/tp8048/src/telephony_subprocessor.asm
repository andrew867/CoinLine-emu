; -----------------------------------------------------------------------------
; telephony_subprocessor.asm
;
; Behavioral TP firmware for a PCD3349A / MCS-48 backend.
;
; This is an original, deterministic, executable 8048-class firmware. It is
; NOT a recovered mask ROM image. It runs inside the emulator and preserves
; the CP-visible TP contract while moving logic out of ad-hoc host C++.
;
; The program assumes the C++ wrapper maps abstract host services into port
; callbacks:
;
;   P0 input/write: CP/TP serial data latch abstraction, one accepted byte at a time
;   P1 input:       sampled KEYMATRIX / hook bits from TP-side front panel
;   P2 input/write: control/status bits, bit-level mapping supplied by wrapper
;   T0/T1:          optional edge sources, wrapper may translate CSI/O byte ready
;
; Timing target:
;   PCD3349A/8048 core clock = 3.579545 MHz or 3.58 MHz.
;   Timer ISR establishes coarse 10 ms service tick by wrapper reload policy.
;
; Assembly dialect:
;   MCS-48 style. Some assemblers will need MOVD syntax and ORG/vector syntax
;   adjusted. Keep symbols and control flow stable when porting.
; -----------------------------------------------------------------------------

        INCLUDE "telephony_subprocessor_symbols.inc"

        ORG 000H
RESET:  JMP START

        ORG 003H
EXTINT: JMP EXT_IRQ                 ; CP byte-ready or wrapper interrupt

        ORG 007H
TIMER:  JMP TIMER_IRQ               ; periodic tick from wrapper/core timer

; -----------------------------------------------------------------------------
; STARTUP
; -----------------------------------------------------------------------------
        ORG 010H
START:
        DIS I
        DIS TCNTI

        ; quasi-bidirectional ports idle high unless wrapper overrides mask options
        MOV A,#0FFH
        OUTL BUS,A
        OUTL P1,A
        OUTL P2,A

        CALL CLEAR_RAM

        MOV R0,#TP_STATE
        MOV A,#ST_RESET_PENDING
        MOV @R0,A

        MOV R0,#FLAGS0
        MOV A,#F_LINE_OK
        MOV @R0,A
        MOV R0,#FLAGS1
        MOV A,#F_SECURITY_OK
        ORL A,#F_LINE_OK          ; default handset continuity OK for QUERY_HANDSET_CONTINUITY
        ORL A,#F_RESERVED         ; arm QUERY_PWR_INTERRUPT_STATUS -> POWER_FAILURE (07Ch) until consumed
        MOV @R0,A

        MOV A,#00H
        MOV R0,#TONE_MODE
        MOV @R0,A

        EN TCNTI
        ; STRT T required: deferred hook steady bytes (HOOK_SEQ_REM) decrement in TIMER_DONE.
        ; Without a running internal timer, EN TCNTI never vectors and ~80ms hook follow-up never sends.
        STRT T
        EN I

MAIN_LOOP:
        CALL SERVICE_CP_RX
        CALL SAMPLE_HOST_FLAGS
        CALL SCAN_FRONT_PANEL
        CALL UPDATE_TONE_FROM_STATE
        CALL SERVICE_RUNTIME_HEALTH
        CALL SERVICE_TONE_INTENT
        CALL SERVICE_TX
        JMP MAIN_LOOP

; -----------------------------------------------------------------------------
; INTERRUPTS
; -----------------------------------------------------------------------------
EXT_IRQ:
        ; Wrapper should have latched one CP->TP byte into P0 before interrupt.
        INS A,BUS
        CALL RX_PUSH_A
        RETR

TIMER_IRQ:
        ; Defer POWER_ON_RESET / POWER_ON_ACK until the timer is running (TP "ready" for IPC timing).
        MOV R0,#FLAGS0
        MOV A,@R0
        ANL A,#F_BOOT_ACK_SENT
        JNZ TI_AFTER_BOOT_ACK
        CALL QUEUE_BOOT_ACK
TI_AFTER_BOOT_ACK:
        ; Coarse service tick. The wrapper/core owns exact reload from 3.58 MHz.
        MOV R0,#TICK_10MS
        INC @R0
        MOV A,@R0
        XRL A,#0AH
        JNZ TIMER_DONE
        MOV A,#00H
        MOV @R0,A
        MOV R0,#TICK_100MS
        INC @R0
        MOV A,@R0
        XRL A,#0AH
        JNZ TIMER_DONE
        MOV A,#00H
        MOV @R0,A
        MOV R0,#TICK_1S
        INC @R0
TIMER_DONE:
        ; Deferred steady-state hook opcode (~HOOK_STATE_DELAY_IRQs after transition).
        MOV R0,#HOOK_SEQ_REM
        MOV A,@R0
        JZ TD_HOOKSEQ_DONE
        DEC A
        MOV @R0,A
        JNZ TD_HOOKSEQ_DONE
        MOV R0,#HOOK_SEQ_BYTE
        MOV A,@R0
        CALL TX_PUSH_A
TD_HOOKSEQ_DONE:
        RETR

; -----------------------------------------------------------------------------
; TELEPHONY CONFIG FRAME (CONTTELC.C) — placed early so internal JZ/JNZ stay
; within one 256-byte code page (MCS-48 branch rule).
; -----------------------------------------------------------------------------
; telephony_config_msg_encode builds a fixed 126-byte frame:
;   byte0 = 0xC0 (TELEPHONY_CONFIG_MSG_HDR / CMD_CONFIG)
;   byte1 = 126   (TELEPHONY_CONFIG_MSG_LENGTH)
;   bytes2..124   repdial+screen+POTS+dial spd (123 octets summed by CP into byte125)
;   byte125       sum8 checksum (sum of indices 2..124 inclusive only)
CFG_VALIDATE_FINISH:
        MOV R0,#CFG_LEN_ADDR
        MOV A,@R0
        XRL A,#TELE_CFG_LEN
        JZ CFG_LEN_OK
        CALL CFG_REJECT_FINISH
        RET
CFG_LEN_OK:
        MOV R0,#TMP_SUM
        MOV A,#00H
        MOV @R0,A
        MOV R2,#CFG_PAY0
CFG_SUMLOOP:
        MOV A,R2
        XRL A,#07DH
        JZ CFG_CHKCOMPARE
        MOV A,R2
        ADD A,#CFG_BLK
        MOV R0,A
        MOV A,@R0
        MOV R3,A
        MOV R0,#TMP_SUM
        MOV A,@R0
        ADD A,R3
        MOV @R0,A
        INC R2
        JMP CFG_SUMLOOP

CFG_CHKCOMPARE:
        MOV R0,#TMP_SUM
        MOV A,@R0
        MOV R1,A
        MOV R0,#CFG_CHK_ADDR
        MOV A,@R0
        XRL A,R1
        JZ CFG_DO_ACCEPT_FINISH
        CALL CFG_REJECT_FINISH
        RET

CFG_DO_ACCEPT_FINISH:
        MOV R0,#FLAGS0
        MOV A,@R0
        ORL A,#F_CFG_ACTIVE
        MOV @R0,A
        MOV A,#RESP_CONFIG_ACK
        CALL TX_PUSH_A
        CALL MARK_RUNTIME_GOOD
        RET

CFG_ACCEPT_FINISH EQU CFG_DO_ACCEPT_FINISH

CFG_REJECT_FINISH:
        MOV R0,#FLAGS0
        MOV A,@R0
        ANL A,#07FH
        MOV @R0,A
        MOV R0,#CFG_NEED
        MOV A,#00H
        MOV @R0,A
        ; CONTTELC.C: TELEPHONY_CHECKSUM_ERROR prompts telephony_config_update (re-send config).
        MOV A,#TELEPHONY_CHECKSUM_ERROR
        CALL QUEUE_ERROR_REPORT_FRAMED
        RET

        ORG     01C0H
MAP_KEY_NIBBLE_TO_OPCODE:
        ; Digit 0..9 via MOVP table on same ROM page (ASL MCS-48 page rule on JZ chains).
        MOV A,R2
        ADD A,#0F6H
        JC      MKUNK
        MOV A,R2
        ADD A,#0E8H
        MOVP    A,@A
        RET
MKUNK:
        MOV A,#KEY_RELEASE
        RET

        ORG     01E8H
DIGTAB:
        DB      KEY_0, KEY_1, KEY_2, KEY_3, KEY_4
        DB      KEY_5, KEY_6, KEY_7, KEY_8, KEY_9

KEY_BUFFER_APPEND:
        ; Keep last eight logical nibbles for internal trace/craft detector.
        RET

        ORG     0200H               ; CP parser after MAP_KEY/MOVTAB (avoid backward ORG).

; -----------------------------------------------------------------------------
; CP COMMAND PARSER
; -----------------------------------------------------------------------------
; CONTTELC.C telephony_config_msg_encode builds a fixed 126-byte frame:
;   byte0 = 0xC0 (TELEPHONY_CONFIG_MSG_HDR / CMD_CONFIG)
;   byte1 = 126   (TELEPHONY_CONFIG_MSG_LENGTH)
;   bytes2..124   repdial+screen+POTS+dial spd (123 octets summed by CP into byte125)
;   byte125       sum8 checksum (sum of indices 2..124 inclusive only)
;
; While CFG_NEED<>0 bytes are routed only into CFG_BLK continuation (never as hook/status).
SERVICE_CP_RX:
        CALL RX_EMPTY
        JNZ CP_RX_GOT_BYTE
        RET
CP_RX_GOT_BYTE:
        CALL RX_POP_A
        MOV R7,A

        MOV R0,#CFG_NEED
        MOV A,@R0
        JZ CP_RX_ROUTE_SINGLE
        JMP CP_RX_CFG_DATA

CP_RX_ROUTE_SINGLE:
        MOV R0,#CP_CMD
        MOV A,R7
        MOV @R0,A

        MOV A,R7
        XRL A,#CMD_HOOK_STATUS
        JZ CMD_HOOK
        MOV A,R7
        XRL A,#CMD_TEL_STATUS
        JZ CMD_STATUS
        MOV A,R7
        XRL A,#CMD_NEXT_CALL_IDLE
        JZ CMD_ACCEPT_ONLY
        MOV A,R7
        XRL A,#CMD_POWER_STAT
        JZ CMD_POWER
        MOV A,R7
        XRL A,#CMD_VER_QUERY
        JZ CMD_VERSION
        MOV A,R7
        XRL A,#CMD_CLR_STAT
        JZ CMD_ACCEPT_ONLY
        MOV A,R7
        XRL A,#CMD_CLR_DTMF
        JZ CMD_ACCEPT_ONLY
        MOV A,R7
        XRL A,#CMD_CO_LINE
        JZ CMD_COLINE
        MOV A,R7
        XRL A,#CMD_ERROR_RPT
        JZ CMD_ERROR
        MOV A,R7
        XRL A,#CMD_CLR_ERR
        JZ CMD_ACCEPT_ONLY
        MOV A,R7
        XRL A,#CMD_KEY_MATRIX
        JZ CMD_KEYMAT
        MOV A,R7
        XRL A,#CMD_HANDSET
        JZ CMD_HANDSET_Q
        MOV A,R7
        XRL A,#CMD_REL_RELAY
        JZ CMD_REL_RELEASE
        MOV A,R7
        XRL A,#CMD_SEIZE_RELAY
        JNZ CP_RX_AFTER_SEIZE_RELAY
        JMP RX_SEIZE_RELAY
CP_RX_AFTER_SEIZE_RELAY:
        MOV A,R7
        XRL A,#CMD_CHGFDBK
        JZ CMD_ACCEPT_ONLY
        MOV A,R7
        XRL A,#CMD_CONFIG
        JZ CP_RX_CFG_HDR

        MOV R0,#CP_CMD
        MOV A,@R0
        CALL HANDLE_RUNTIME_CONTROL
        RET

CMD_HOOK:
        CALL EMIT_HOOK_STATE
        CALL MARK_RUNTIME_GOOD
        RET

CMD_STATUS:
        CALL QUEUE_STATUS_FRAME
        CALL MARK_RUNTIME_GOOD
        RET

CMD_ACCEPT_ONLY:
        ; NEXT_CALL_IDLE, CLEAR_*, DTMF feedback: CP expects no immediate TP frame.
        CALL UPDATE_TONE_FROM_STATE
        RET

CMD_REL_RELEASE:
        ; RELEASE_HOOK_SWITCH_RELAY (3Ch): hook sense returns to terminal/local path — allow OOS SIT/NIS again.
        MOV R0,#RELAY_CP_SEIZED
        MOV A,#00H
        MOV @R0,A
        CALL UPDATE_TONE_FROM_STATE
        RET
RX_SEIZE_RELAY:
        ; SEIZE_HOOK_SWITCH_RELAY (3Dh): CP owns hook relay — suppress local OOS tone cue in this model.
        MOV R0,#RELAY_CP_SEIZED
        MOV A,#01H
        MOV @R0,A
        CALL UPDATE_TONE_FROM_STATE
        RET

CMD_COLINE:
        ; QUERY_CO_LINE_STATUS — derive IPCODES 080h/082h/084h/086h from linectrl on P2 (host contract).
        IN A,P2
        MOV R1,A
        MOV A,R1
        ANL A,#01H
        JZ CO_LINE_BRK
        MOV A,R1
        ANL A,#04H
        JZ CO_LINE_C3
        MOV A,#086H
        JMP CO_LINE_PUSH
CO_LINE_C3:
        MOV A,#084H
        JMP CO_LINE_PUSH
CO_LINE_BRK:
        MOV A,R1
        ANL A,#04H
        JZ CO_LINE_I1
        MOV A,#082H
        JMP CO_LINE_PUSH
CO_LINE_I1:
        MOV A,#080H
CO_LINE_PUSH:
        CALL TX_PUSH_A
        CALL MARK_RUNTIME_GOOD
        RET

CMD_VERSION:
        CALL QUEUE_VERSION_FRAME
        CALL MARK_RUNTIME_GOOD
        RET

CMD_KEYMAT:
        IN A,P1
        ANL A,#00FH
        XRL A,#00FH
        JNZ KM_ACTIVE
        MOV A,#KEY_MATRIX_IDLE
        JMP KM_DONE
KM_ACTIVE:
        MOV A,#KEY_MATRIX_ACTIVE
KM_DONE:
        CALL TX_PUSH_A
        CALL MARK_RUNTIME_GOOD
        RET

CMD_HANDSET_Q:
        MOV R0,#FLAGS1
        MOV A,@R0
        ANL A,#F_LINE_OK
        JZ HS_BAD
        MOV A,#HANDSET_CONT_OK
        JMP HS_PUSH
HS_BAD:
        MOV A,#HANDSET_CONT_BAD
HS_PUSH:
        CALL TX_PUSH_A
        CALL MARK_RUNTIME_GOOD
        RET

CMD_POWER:
        ; If FLAGS1.F_RESERVED set, report POWER_FAILURE (07CH) once then clear (RTC / long power-fail model).
        MOV R0,#FLAGS1
        MOV A,@R0
        ANL A,#F_RESERVED
        JZ CMD_PWR_OK
        MOV A,#RESP_PWR_FAIL
        CALL TX_PUSH_A
        MOV R0,#FLAGS1
        MOV A,@R0
        ANL A,#07FH
        MOV @R0,A
        CALL MARK_RUNTIME_GOOD
        RET
CMD_PWR_OK:
        MOV A,#RESP_POWER_OK
        CALL TX_PUSH_A
        CALL MARK_RUNTIME_GOOD
        RET

CMD_ERROR:
        CALL QUEUE_ERROR_FRAME
        CALL MARK_RUNTIME_GOOD
        RET

CP_RX_CFG_HDR:
        ; First octet (0xC0) staged; expecting 125 more from CP CSI/O stream.
        MOV R0,#FLAGS0
        MOV A,@R0
        ORL A,#F_CFG_ACTIVE
        MOV @R0,A
        MOV R0,#CFG_BLK
        MOV A,R7
        MOV @R0,A
        MOV R0,#CFG_NEED
        MOV A,#07DH                 ; 125 remaining
        MOV @R0,A
        MOV R0,#CFG_IDX
        MOV A,#001H                 ; next write offset
        MOV @R0,A
        RET

CP_RX_CFG_DATA:
        ; Store next frame byte; when CFG_NEED hits 0, validate and ACK (or error).
        MOV R0,#CFG_IDX
        MOV A,@R0
        MOV R3,A
        MOV A,R3
        ADD A,#CFG_BLK
        MOV R0,A
        MOV A,R7
        MOV @R0,A
        MOV R0,#CFG_IDX
        INC @R0
        MOV R0,#CFG_NEED
        MOV A,@R0
        DEC A
        MOV @R0,A
        JNZ CPRX_LEAVE
        CALL CFG_VALIDATE_FINISH
CPRX_LEAVE:
        RET

HANDLE_RUNTIME_CONTROL:
        JMP     HRC_BODY

; -----------------------------------------------------------------------------
; FRONT PANEL SCAN AND DEBOUNCE
; -----------------------------------------------------------------------------
SCAN_FRONT_PANEL:
        IN A,P1
        MOV R0,#TMP0
        MOV @R0,A

        ; Hook sample, wrapper maps hook into a stable bit, active policy configured.
        CALL SAMPLE_HOOK
        CALL SAMPLE_KEYPAD
        RET

SAMPLE_HOST_FLAGS:
        ; P2.6 = terminal OOS / SERVS messaging visible (host); P2.7 = voice path active — mirrors tp_input_snapshot.
        IN A,P2
        MOV R4,A
        MOV R0,#FLAGS0
        MOV A,@R0
        ANL A,#0CFH
        MOV R5,A
        MOV A,R4
        ANL A,#040H
        JZ SHF_NO_OOS
        MOV A,R5
        ORL A,#F_OOS
        MOV R5,A
SHF_NO_OOS:
        MOV A,R4
        ANL A,#080H
        JZ SHF_NO_VOICE
        MOV A,R5
        ORL A,#F_VOICE_ACTIVE
        MOV R5,A
SHF_NO_VOICE:
        MOV R0,#FLAGS0
        MOV A,R5
        MOV @R0,A
        RET

SAMPLE_HOOK:
        ; Placeholder bit convention: P1.7 clear = off-hook.
        MOV R0,#TMP0
        MOV A,@R0
        ANL A,#080H
        JZ HOOK_SAMPLE_OFF
HOOK_SAMPLE_ON:
        MOV A,#00H
        JMP HOOK_SAMPLE_APPLY
HOOK_SAMPLE_OFF:
        MOV A,#F_HOOK_OFF
HOOK_SAMPLE_APPLY:
        MOV R0,#HOOK_LAST
        XRL A,@R0
        JZ HOOK_STABLE_SAME
        ; changed, reset debounce counter
        MOV R0,#HOOK_CNT
        MOV A,#00H
        MOV @R0,A
        ; Bouncing hook cancels pending steady-state byte from prior edge.
        MOV R0,#HOOK_SEQ_REM
        MOV A,#00H
        MOV @R0,A
        RET
HOOK_STABLE_SAME:
        MOV R0,#HOOK_CNT
        INC @R0
        MOV A,@R0
        XRL A,#04H                 ; about 40 ms if ticked at 10 ms externally
        JNZ HOOK_DONE
        CALL ACCEPT_HOOK_STATE
HOOK_DONE:
        RET

ACCEPT_HOOK_STATE:
        MOV R0,#HOOK_LAST
        MOV A,@R0
        JZ ACCEPT_ONHOOK
ACCEPT_OFFHOOK:
        MOV R0,#FLAGS1
        MOV A,@R0
        ORL A,#F_HOOK_OFF
        MOV @R0,A
        MOV A,#RESP_HOOK_TRANS_OFF
        CALL TX_PUSH_A
        MOV R0,#HOOK_SEQ_BYTE
        MOV A,#RESP_HOOK_OFF
        MOV @R0,A
        MOV R0,#HOOK_SEQ_REM
        MOV A,#HOOK_STATE_DELAY_IRQS
        MOV @R0,A
        CALL UPDATE_TONE_FROM_STATE
        RET
ACCEPT_ONHOOK:
        MOV R0,#FLAGS1
        MOV A,@R0
        ANL A,#0FEH
        MOV @R0,A
        MOV A,#RESP_HOOK_TRANS_ON
        CALL TX_PUSH_A
        MOV R0,#HOOK_SEQ_BYTE
        MOV A,#RESP_HOOK_ON
        MOV @R0,A
        MOV R0,#HOOK_SEQ_REM
        MOV A,#HOOK_STATE_DELAY_IRQS
        MOV @R0,A
        CALL UPDATE_TONE_FROM_STATE
        RET

SAMPLE_KEYPAD:
        ; P1.6=1 packed wire opcode (host resolves KEYMATRIX/softkeys to IPCODES TP→CP bytes).
        MOV R0,#TMP0
        MOV A,@R0
        MOV R2,A
        ANL A,#040H
        JZ SK_LEGACY
        MOV A,R2
        ANL A,#03FH
        MOV R4,A
        IN A,P2
        MOV R3,A
        ANL A,#030H
        MOV R5,A
        ADD A,R5
        ADD A,R5
        ORL A,R4
        MOV R1,A
        MOV R0,#KEY_LAST
        MOV A,@R0
        XRL A,R1
        JZ PK_SAME
        MOV A,R1
        MOV @R0,A
        MOV R0,#KEY_CNT
        MOV A,#00H
        MOV @R0,A
        RET
PK_SAME:
        MOV R0,#KEY_CNT
        INC @R0
        MOV A,@R0
        XRL A,#03H
        JNZ KEY_DONE
        MOV A,R1
        CALL ACCEPT_PACKED_OPCODE
        JMP KEY_DONE
SK_LEGACY:
        ; Legacy nibble 0..9 -> DIGTAB (IPCODES wire bytes via MOVP table).
        MOV A,R2
        ANL A,#00FH
        XRL A,#00FH
        JZ NO_KEY_SAMPLE
        MOV A,R2
        ANL A,#00FH
        MOV R1,A
        MOV R0,#KEY_LAST
        MOV A,@R0
        XRL A,R1
        JZ KEY_SAME
        MOV A,R1
        MOV @R0,A
        MOV R0,#KEY_CNT
        MOV A,#00H
        MOV @R0,A
        RET
KEY_SAME:
        MOV R0,#KEY_CNT
        INC @R0
        MOV A,@R0
        XRL A,#03H                 ; debounce threshold, wrapper may tune
        JNZ KEY_DONE
        MOV A,R1
        CALL ACCEPT_KEY_NIBBLE
KEY_DONE:
        RET
NO_KEY_SAMPLE:
        MOV R0,#KEY_HELD
        MOV A,@R0
        JZ NO_KEY_DONE
        MOV A,#KEY_RELEASE
        CALL TX_PUSH_A
        MOV R0,#KEY_HELD
        MOV A,#00H
        MOV @R0,A
NO_KEY_DONE:
        RET

ACCEPT_PACKED_OPCODE:
        MOV A,R1
        CALL TX_PUSH_A
        CALL KEY_BUFFER_APPEND
        MOV R0,#KEY_HELD
        MOV A,#01H
        MOV @R0,A
        MOV A,R1
        CALL UPDATE_TONE_FOR_DTMF_KEY
        RET

ACCEPT_KEY_NIBBLE:
        ; Nibble 0..9 maps to KEY_0..KEY_9 (IPCODES wire values via DIGTAB).
        MOV R2,A
        CALL MAP_KEY_NIBBLE_TO_OPCODE
        CALL TX_PUSH_A
        CALL KEY_BUFFER_APPEND
        MOV R0,#KEY_HELD
        MOV A,#01H
        MOV @R0,A
        CALL UPDATE_TONE_FOR_DTMF_KEY
        RET

; -----------------------------------------------------------------------------
; RUNTIME HEALTH AND RESPONSES
; -----------------------------------------------------------------------------
MARK_RUNTIME_GOOD:
        MOV R0,#MISS_CNT
        MOV A,#00H
        MOV @R0,A
        MOV R0,#FLAGS0
        MOV A,@R0
        ORL A,#F_READY
        ANL A,#0F7H                ; clear F_FAULT
        MOV @R0,A
        MOV R0,#TP_STATE
        MOV A,#ST_RUNTIME_ACTIVE
        MOV @R0,A
        RET

SERVICE_RUNTIME_HEALTH:
        ; This intentionally does NOT spam unsolicited C0 frames. It only manages
        ; timeout state. CP-initiated query/response resets miss counter.
        MOV R0,#TICK_1S
        MOV A,@R0
        XRL A,#03H
        JNZ HEALTH_DONE
        MOV A,#00H
        MOV @R0,A
        MOV R0,#MISS_CNT
        INC @R0
        MOV A,@R0
        XRL A,#03H
        JNZ HEALTH_DONE
        MOV R0,#FLAGS0
        MOV A,@R0
        ORL A,#F_FAULT
        ANL A,#0FBH                ; clear ready
        MOV @R0,A
        MOV R0,#TP_STATE
        MOV A,#ST_TIMEOUT_LATCHED
        MOV @R0,A
        CALL UPDATE_TONE_FROM_STATE
HEALTH_DONE:
        RET

QUEUE_BOOT_ACK:
        ; Match CP IPC semantics: sample hook before debounce timers run.
        ; P1.7 high = on-hook -> POWER_ON_RESET (070H). P1.7 low = off-hook -> POWER_ON_ACK (072H).
        IN A,P1
        ANL A,#080H
        JZ BOOT_QUEUE_ACK
        MOV A,#RESP_BOOT_RESET
        JMP BOOT_QUEUE_PUSH
BOOT_QUEUE_ACK:
        MOV A,#RESP_BOOT_PWRON_ACK
BOOT_QUEUE_PUSH:
        CALL TX_PUSH_A
        MOV R0,#FLAGS0
        MOV A,@R0
        ORL A,#F_BOOT_ACK_SENT
        MOV @R0,A
        MOV R0,#TP_STATE
        MOV A,#ST_BOOT_ACK_VISIBLE
        MOV @R0,A
        RET

QUEUE_STATUS_FRAME:
        ; TELEPHONY_STATUS (0xC0): CONTTELC.C telephony_misc_msg_decode — 2nd byte == TELEPHONY_STATUS_MSG_LENGTH (8).
        ; Frame = opcode + len + 5-byte TELEPHONY_STATUS body + checksum; chk = ip_rx_code + len + sum(body) mod 256.
        MOV A,#FRAME_STATUS
        CALL TX_PUSH_A
        MOV A,#LEN_STATUS
        CALL TX_PUSH_A
        MOV A,#00H                 ; total_fail_to_pots lo
        CALL TX_PUSH_A
        MOV A,#00H                 ; total_fail_to_pots hi
        CALL TX_PUSH_A
        MOV A,#00H                 ; call_duration lo
        CALL TX_PUSH_A
        MOV A,#00H                 ; call_duration hi
        CALL TX_PUSH_A
        MOV A,#00H                 ; call_state idle / cleared
        CALL TX_PUSH_A
        MOV A,#0C8H                ; chk for C0+08+all-zero payload (matches queue_raw_frame)
        CALL TX_PUSH_A
        RET

        ; TELEPHONY_VERSION_NUMBER (0xC2): 2nd byte == TEL_VERSION_NUM_MSG_LENGTH (14); payload = 14-3 = 11 bytes.
        ; Label-style ASCII "0772470" + NUL pad to 11; chk = opcode + len + sum(payload) mod 256 (CONTTELC.C).
QUEUE_VERSION_FRAME:
        MOV A,#FRAME_EXT_STATUS
        CALL TX_PUSH_A
        MOV A,#LEN_EXT_STATUS
        CALL TX_PUSH_A
        MOV A,#030H                ; '0'
        CALL TX_PUSH_A
        MOV A,#037H                ; '7'
        CALL TX_PUSH_A
        MOV A,#037H                ; '7'
        CALL TX_PUSH_A
        MOV A,#032H                ; '2'
        CALL TX_PUSH_A
        MOV A,#034H                ; '4'
        CALL TX_PUSH_A
        MOV A,#037H                ; '7'
        CALL TX_PUSH_A
        MOV A,#030H                ; '0'
        CALL TX_PUSH_A
        MOV A,#00H
        CALL TX_PUSH_A
        MOV A,#00H
        CALL TX_PUSH_A
        MOV A,#00H
        CALL TX_PUSH_A
        MOV A,#00H
        CALL TX_PUSH_A
        MOV A,#03BH                ; additive checksum (C2+0EH + payload, uint8_t rolling sum)
        CALL TX_PUSH_A
        RET

        ; TELEPHONY_ERROR_REPORT (0xC4): 2nd byte == TEL_ERROR_REPORT_MSG_LENGTH (4); body = status + checksum.
        ; checksum = ip_rx_code + len + status (mod 256) per telephony_misc_msg_decode (CONTTELC.C).
QUEUE_ERROR_REPORT_FRAMED:
        MOV R2,A                   ; status octet
        MOV A,#FRAME_ERROR_REPORT
        CALL TX_PUSH_A
        MOV A,#LEN_ERROR
        CALL TX_PUSH_A
        MOV A,R2
        CALL TX_PUSH_A
        MOV A,#FRAME_ERROR_REPORT
        ADD A,#LEN_ERROR
        ADD A,R2
        CALL TX_PUSH_A
        RET

QUEUE_ERROR_FRAME:
        MOV A,#00H                 ; no error bits
        CALL QUEUE_ERROR_REPORT_FRAMED
        RET

EMIT_HOOK_STATE:
        ; QUERY_HOOK_SWITCH_STATE: same transition-then-steady sequence as physical hook.
        MOV R0,#FLAGS1
        MOV A,@R0
        ANL A,#F_HOOK_OFF
        JZ EMIT_ON
        MOV A,#RESP_HOOK_TRANS_OFF
        CALL TX_PUSH_A
        MOV R0,#HOOK_SEQ_BYTE
        MOV A,#RESP_HOOK_OFF
        MOV @R0,A
        MOV R0,#HOOK_SEQ_REM
        MOV A,#HOOK_STATE_DELAY_IRQS
        MOV @R0,A
        RET
EMIT_ON:
        MOV A,#RESP_HOOK_TRANS_ON
        CALL TX_PUSH_A
        MOV R0,#HOOK_SEQ_BYTE
        MOV A,#RESP_HOOK_ON
        MOV @R0,A
        MOV R0,#HOOK_SEQ_REM
        MOV A,#HOOK_STATE_DELAY_IRQS
        MOV @R0,A
        RET

        ORG     0300H               ; Keep tone + SERVICE_TONE_INTENT branch chains on one 256-byte ROM page.

; -----------------------------------------------------------------------------
; TONE / DTMF
; -----------------------------------------------------------------------------
UPDATE_TONE_FROM_STATE:
        ; No dial-tone intent in ROM (voiceware path); OOS SIT/NIS when off-hook + OOS until CP SEIZE relay (3Dh).
        MOV R0,#FLAGS1
        MOV A,@R0
        ANL A,#F_HOOK_OFF
        JNZ UTS_OFFHK
        MOV A,#TONE_NONE
        JMP SET_TONE_MODE
UTS_OFFHK:
        MOV R0,#FLAGS0
        MOV A,@R0
        ANL A,#F_VOICE_ACTIVE
        JNZ UTS_SILENCE
        MOV R0,#FLAGS0
        MOV A,@R0
        ANL A,#F_FAULT
        JNZ UTS_NIS
        MOV R0,#FLAGS0
        MOV A,@R0
        ANL A,#F_OOS
        JZ UTS_SILENCE
        MOV R0,#RELAY_CP_SEIZED
        MOV A,@R0
        JNZ UTS_SILENCE
        MOV A,#TONE_NIS
        JMP SET_TONE_MODE
UTS_SILENCE:
        MOV A,#TONE_NONE
        JMP SET_TONE_MODE
UTS_NIS:
        MOV A,#TONE_NIS
SET_TONE_MODE:
        MOV R0,#TONE_MODE
        MOV @R0,A
        RET

SERVICE_TONE_INTENT:
        MOV R0,#TMP3
        MOV A,@R0
        ANL A,#TONE_GEN_OFF_Q
        JNZ STI_SILENT
        MOV R0,#TONE_MODE
        MOV A,@R0
        JNZ STI_NONZERO
        MOV A,#00H
        MOV R0,#TONE_MODE
        MOV @R0,A
        RET
STI_NONZERO:
        ANL A,#0FH
        XRL A,#TONE_NIS
        JNZ STI_TRY_DTMF
        MOV A,#TONE_NIS_HIGH_REG
        ; MOVD DER_HGF,A
        MOV A,#TONE_NIS_LOW_REG
        ; MOVD DER_LGF,A
        RET
STI_TRY_DTMF:
        MOV R0,#TONE_MODE
        MOV A,@R0
        ANL A,#0FH
        XRL A,#TONE_DTMF
        JNZ STI_RET_INST
        MOV R0,#TMP1
        MOV A,@R0
        ; MOVD DER_LGF,A
        MOV R0,#TMP2
        MOV A,@R0
        ; MOVD DER_HGF,A
        RET
STI_SILENT:
        MOV A,#00H
        RET
STI_RET_INST:
        RET

UPDATE_TONE_FOR_DTMF_KEY:
        MOV R3,A
        CALL SET_DTMF_FROM_PAD_OPCODE
        MOV R0,#TONE_MODE
        MOV A,#TONE_DTMF
        MOV @R0,A
        RET

; -----------------------------------------------------------------------------
; TX/RX RING BUFFERS
; -----------------------------------------------------------------------------
RX_EMPTY:
        MOV R0,#RX_HEAD
        MOV A,@R0
        MOV R0,#RX_TAIL
        XRL A,@R0
        RET                         ; Z means empty

RX_PUSH_A:
        MOV R1,A
        MOV R0,#RX_HEAD
        MOV A,@R0
        ADD A,#RX_BUF
        MOV R0,A
        MOV A,R1
        MOV @R0,A
        MOV R0,#RX_HEAD
        INC @R0
        MOV A,@R0
        ANL A,#0FH
        MOV @R0,A
        RET

RX_POP_A:
        MOV R0,#RX_TAIL
        MOV A,@R0
        ADD A,#RX_BUF
        MOV R0,A
        MOV A,@R0
        MOV R1,A
        MOV R0,#RX_TAIL
        INC @R0
        MOV A,@R0
        ANL A,#0FH
        MOV @R0,A
        MOV A,R1
        RET

TX_PUSH_A:
        MOV R1,A
        MOV R0,#TX_HEAD
        MOV A,@R0
        ADD A,#TX_BUF
        MOV R0,A
        MOV A,R1
        MOV @R0,A
        MOV R0,#TX_HEAD
        INC @R0
        MOV A,@R0
        ANL A,#00FH
        MOV @R0,A
        MOV R0,#FLAGS0
        MOV A,@R0
        ORL A,#F_TX_PENDING
        MOV @R0,A
        RET

SERVICE_TX:
        MOV R0,#TX_HEAD
        MOV A,@R0
        MOV R0,#TX_TAIL
        XRL A,@R0
        JZ TX_EMPTY
        ; Write next byte to P0. Wrapper clocks it to CP when CP requests/CSI/O clocks.
        MOV R0,#TX_TAIL
        MOV A,@R0
        ADD A,#TX_BUF
        MOV R0,A
        MOV A,@R0
        OUTL BUS,A
        MOV R0,#TX_TAIL
        INC @R0
        MOV A,@R0
        ANL A,#00FH
        MOV @R0,A
        RET
TX_EMPTY:
        MOV R0,#FLAGS0
        MOV A,@R0
        ANL A,#0BFH                ; clear F_TX_PENDING
        MOV @R0,A
        RET

CLEAR_RAM:
        ; Clear internal RAM indices 020h..07fh inclusive (requires 0080 - 020 = 096 iter).
        MOV R0,#020H
        MOV R2,#060H
CLR_LOOP:
        MOV A,#00H
        MOV @R0,A
        INC R0
        DEC R2
        MOV A,R2
        JZ CLR_DONE
        JMP CLR_LOOP
CLR_DONE:
        RET

; -----------------------------------------------------------------------------
; Extended CP control + DTMF register pairs (same ROM page for MOVP table access).
; -----------------------------------------------------------------------------
        ORG     0B00H
DTMF_PAIR_TAB:
        DB      DTMF_LOW_0, DTMF_HIGH_0
        DB      DTMF_LOW_1, DTMF_HIGH_1
        DB      DTMF_LOW_2, DTMF_HIGH_2
        DB      DTMF_LOW_3, DTMF_HIGH_3
        DB      DTMF_LOW_4, DTMF_HIGH_4
        DB      DTMF_LOW_5, DTMF_HIGH_5
        DB      DTMF_LOW_6, DTMF_HIGH_6
        DB      DTMF_LOW_7, DTMF_HIGH_7
        DB      DTMF_LOW_8, DTMF_HIGH_8
        DB      DTMF_LOW_9, DTMF_HIGH_9
        DB      DTMF_LOW_0, DTMF_HIGH_0
        DB      DTMF_LOW_0, DTMF_HIGH_0
        DB      DTMF_LOW_0, DTMF_HIGH_0
        DB      DTMF_LOW_0, DTMF_HIGH_0
        DB      DTMF_LOW_0, DTMF_HIGH_0
        DB      DTMF_LOW_0, DTMF_HIGH_0

; R4 = byte offset into DTMF_PAIR_TAB on this ROM page (ORG 0B00H).
LOAD_DTMF_FROM_INDEX:
        MOV A,R4
        MOVP A,@A
        MOV R0,#TMP1
        MOV @R0,A
        MOV A,R4
        INC A
        MOVP A,@A
        MOV R0,#TMP2
        MOV @R0,A
        RET

SET_DTMF_FROM_PAD_OPCODE:
        MOV R0,#TMP3
        MOV A,@R0
        ANL A,#P_PAD_DTMF_DIS
        JNZ SF_RET
        MOV A,R3
        ADD A,#0E0H
        JNC SF_RET
        MOV A,R3
        ADD A,#0E0H
        MOV R4,A
        CALL LOAD_DTMF_FROM_INDEX
SF_RET:
        RET

HRC_BODY:
        MOV R3,A
        MOV A,R3
        XRL A,#CP_PAUSE
        JZ HRCX_RET
        MOV A,R3
        ADD A,#0F0H
        JNC HRC_AFTER_CP_DTMF
        MOV A,R3
        ADD A,#0F0H
        MOV R4,A
        MOV A,R4
        ADD A,#0F0H
        JC HRC_AFTER_CP_DTMF
        MOV A,R4
        ADD A,R4
        MOV R4,A
        CALL LOAD_DTMF_FROM_INDEX
        MOV R0,#TONE_MODE
        MOV A,#TONE_DTMF
        MOV @R0,A
HRCX_RET:
        RET
HRC_AFTER_CP_DTMF:
        MOV A,R3
        XRL A,#CP_PAD_DIS
        JZ HRC_PAD_DIS
        MOV A,R3
        XRL A,#CP_PAD_EN
        JZ HRC_PAD_EN
        MOV A,R3
        XRL A,#CP_TONE_OFF
        JZ HRC_TONE_OFF
        MOV A,R3
        ADD A,#0E0H
        JNC HRC_AFTER_PV
        MOV A,R3
        ADD A,#0DCH
        JNC HRC_AFTER_PV
        MOV A,R3
        ADD A,#0E0H
        MOV R0,#VOL_IDX
        MOV @R0,A
        RET
HRC_AFTER_PV:
        MOV A,R3
        XRL A,#024H
        JZ HRC_VOL_DOWN
        MOV A,R3
        XRL A,#025H
        JZ HRC_VOL_UP
        MOV A,R3
        ADD A,#0C0H
        JNC HRC_TAIL_TONE
        MOV A,R3
        ADD A,#0B9H
        JNC HRC_TAIL_TONE
        MOV A,R3
        ADD A,#0C0H
        MOV R4,A
        MOV A,R4
        ADD A,R4
        ADD A,R4
        ADD A,R4
        ADD A,R4
        MOV R7,A
        MOV R0,#TP_STATE
        MOV A,@R0
        ANL A,#0FH
        ORL A,R7
        MOV @R0,A
        RET
HRC_VOL_DOWN:
        MOV R0,#VOL_IDX
        MOV A,@R0
        JZ HRCX_RET
        DEC A
        MOV @R0,A
        RET
HRC_VOL_UP:
        MOV R0,#VOL_IDX
        MOV A,@R0
        XRL A,#03H
        JZ HRCX_RET
        INC A
        MOV @R0,A
        RET
HRC_PAD_DIS:
        MOV R0,#TMP3
        MOV A,@R0
        ORL A,#P_PAD_DTMF_DIS
        MOV @R0,A
        RET
HRC_PAD_EN:
        MOV R0,#TMP3
        MOV A,@R0
        ANL A,#0FEH
        MOV @R0,A
        RET
HRC_TONE_OFF:
        MOV R0,#TMP3
        MOV A,@R0
        ORL A,#TONE_GEN_OFF_Q
        MOV @R0,A
        RET
HRC_TAIL_TONE:
        CALL UPDATE_TONE_FROM_STATE
        RET

        END
