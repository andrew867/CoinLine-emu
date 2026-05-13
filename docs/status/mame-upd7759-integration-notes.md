# MAME uPD7759 (NEC) integration

- **Device instance**: `voicew_upd` — `UPD7759` in `millennium_state::millennium()`.
- **ROM**: `m_voicew_upd->set_device_rom_tag(":voicew")`; region `voicew` is **2 MiB** with two 1 MiB images.
- **Voiceware bridge**: `millennium_voiceware_device` uses `required_device<upd7759_device> m_upd{*this, "^:voicew_upd"}` and drives `port_w` + `start_w` / `reset_w` + `set_rom_bank` for the lower nibble bank latch.
- **Output**: `SPEAKER` `vwspk` with `add_route(ALL_OUTPUTS, "vwspk", 1.0)`.
- **MAME `busy_r`**: returns **true when the synthesis core is idle**; `playing` in trace uses `!busy_r()`.
