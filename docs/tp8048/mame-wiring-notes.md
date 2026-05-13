# MAME Wiring Notes for PCD3349A Backend

## Use MAME's existing MCS-48 family if available

Before writing a local interpreter, check the local MAME tree for MCS-48/8048 devices. Prefer an existing CPU core. The wrapper contract in `src/millennium_am8048_core_contract.h` is intentionally narrow so either implementation can fit.

## Device configuration

Target clock:

```cpp
constexpr XTAL TP_XTAL = XTAL(3'579'545);
```

or board-profile override:

```cpp
constexpr uint32_t TP_XTAL_HZ = 3'580'000;
```

## Backend selector

Compile-time:

```cpp
#ifndef COINLINE_ENABLE_TP8048_BACKEND
#define COINLINE_ENABLE_TP8048_BACKEND 1
#endif
```

Runtime trace:

```json
{"tp_backend":"legacy"}
{"tp_backend":"pcd3349a_8048"}
```

## CP to TP

Existing CSI/O qualifier should call:

```cpp
m_tp_backend->receive_cp_byte(byte);
```

Then step TP to current CP cycle:

```cpp
m_tp_backend->run_until_cp_cycle(machine().time().as_ticks(cp_hz), cp_hz);
```

Then drain TX:

```cpp
while (m_tp_backend->has_tx_byte()) {
    ipcomm_queue_rx_byte(m_tp_backend->pop_tx_byte(), "tp8048");
}
```

## Inputs

Feed snapshots into backend at deterministic points:

```cpp
tp_input_snapshot s;
s.keymatrix = ioport("KEYMATRIX")->read();
s.linectrl = ioport("LINECTRL")->read();
s.softkeys = ioport("TERMINAL21_SOFTKEYS")->read_safe(0xffff);
s.secmask = ioport("SECMASK")->read();
s.oos_visible = m_vfd && m_vfd->contains_oos_text();
s.voice_active = m_voiceware && m_voiceware->is_playing();
s.cp_cycle = m_maincpu->total_cycles();
m_tp_backend->set_input_snapshot(s);
```

## Tone output

Convert backend tone mode into existing audio route:

- `none`: no local TP tone.
- `dialtone`: 350 + 440 Hz.
- `nis`: 480 + 620 Hz.
- `dtmf`: use last key's PCD3349A HGF/LGF register pair if available.

## A/B parity

Do not remove the legacy backend until:

- Stage 1 boot/readiness parity passes.
- Stage 2 OOS idle stability passes.
- Stage 3 A6 craft-only passes.
- Stage 4 full acceptance A1-A6 passes.

