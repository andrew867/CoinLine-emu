# Telephony symbol map status

There is **no committed address-resolution artifact** inside `coinline-emu` that binds symbol names such as `ip_rx_buffer`, `TERMFG_TELEPHONY_UP`, or the telephony message task to fixed addresses.

Machine-generated placeholder: [`build/generated/telephony-symbol-map.json`](../../build/generated/telephony-symbol-map.json).

To attach watch-based traces locally, **export a map** from your firmware build and feed addresses through a future `COINLINE_*` watch env (not implemented in this change set).
