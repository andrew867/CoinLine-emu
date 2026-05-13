# PCD3349A Opcode Coverage Matrix

This matrix tracks implemented instruction families for the TP 8048 core (`millennium_am8048_core`), aligned to documented MCS-48 behavior used by the PCD3349A.

## Coverage Summary

- Implemented: ALU/immediate/register/indirect arithmetic and logic families.
- Implemented: Branch, call, return, and bit-test control-flow families.
- Implemented: Register bank/memory bank, PSW/carry/F0/F1 flag operations.
- Implemented: Timer/counter control, timer flag branch, external/timer interrupt entry/return.
- Implemented: Port BUS/P1/P2 byte operations and P4-P7 nibble operations.
- Implemented: Program table operations (`MOVP`, `MOVP3`, `JMPP`).
- Implemented: External data-space operations (`MOVX A,@R0/R1`, `MOVX @R0/R1,A`) with P2 high-nibble addressing.

## Implemented Families

- **Data move**
  - `MOV A,#imm`, `MOV Rn,#imm`, `MOV Rn,A`, `MOV A,Rn`
  - `MOV @R0/@R1,#imm`, `MOV @R0/@R1,A`, `MOV A,@R0/@R1`
  - `MOV A,PSW`, `MOV PSW,A`, `MOV A,T`, `MOV T,A`
- **Arithmetic / logic**
  - `ADD`, `ADDC`, `ANL`, `ORL`, `XRL` (imm, Rn, @Rn forms)
  - `DA A`, `CLR A`, `CPL A`, `INC A`, `DEC A`
- **Rotate / exchange**
  - `RL`, `RLC`, `RR`, `RRC`, `SWAP`
  - `XCH A,Rn`, `XCH A,@R0/@R1`, `XCHD A,@R0/@R1`
- **Control flow**
  - `JMP`, `CALL`, `RET`, `RETR`
  - `JNZ`, `JZ`, `JC`, `JNC`, `JTF`, `JNI`
  - `JT0`, `JNT0`, `JT1`, `JNT1`
  - `JB0..JB7`, `JF0`, `JF1`, `DJNZ Rn,addr`
  - `JMPP @A`
- **Ports and nibble ports**
  - `INS A,BUS`, `IN A,P1/P2`
  - `OUTL BUS/P1/P2,A`
  - `ORL BUS/P1/P2,#imm`, `ANL BUS/P1/P2,#imm`
  - `MOVD A,P4..P7`, `MOVD P4..P7,A`
  - `ORLD P4..P7,A`, `ANLD P4..P7,A`
- **Program/data table**
  - `MOVP A,@A`, `MOVP3 A,@A`
  - `MOVX A,@R0/@R1`, `MOVX @R0/@R1,A`
- **Mode / interrupt / timer control**
  - `NOP`, `IDL`
  - `EN I`, `DIS I`, `EN TCNTI`, `DIS TCNTI`
  - `STRT T`, `STRT CNT`, `STOP TCNT`, `ENT0 CLK`
  - `SEL MB0/MB1`, `SEL RB0/RB1`
  - `CLR C`, `CPL C`, `CLR F0`, `CPL F0`, `CLR F1`, `CPL F1`

## Validation Status

- Conformance vectors exist for:
  - Quasi-bidirectional port readback
  - T1 edge-driven counter behavior
  - Idle wake on external interrupt
  - `JNI`/`JF0`/`JF1` branch truth
  - Rotate/carry, `XCHD`, `DA`
  - `JB` + `DEC @Rn`
  - `ORL/ANL` immediate port ops
  - `MOVP`/`MOVP3`
  - `JMPP`
  - `ORLD`/`ANLD`
  - `MOVX` round-trip with P2 high-nibble addressing

Any future opcode additions or semantic changes should update this matrix and corresponding conformance vectors in `tests/devices/test_tp8048_core_conformance.cpp`.
