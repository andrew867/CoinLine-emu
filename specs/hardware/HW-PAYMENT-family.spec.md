# Umbrella spec: Card reader, smartcard, coin validator

Hardware IDs: **HW-CARD-001**, **HW-SC-001**, **HW-COIN-001**

## Magstripe (HW-CARD-001)

- **Ports:** `0x52`/`0x53` — `millennium_card*.cpp`.  
- **Docs:** `docs/card-reader-emulation.md`.  
- **Tests:** `test_card_*`, `integration/test_card_call.cpp`.  
- **Acceptance:** Track bytes emerge from model timing rules — **not** pre-canned “approval” without swipe simulation.

## Smartcard (HW-SC-001)

- **Ports:** `0x63` power/clock; MACH bits for presence — `millennium_smartcard*.cpp`.  
- **Tests:** `test_smartcard_*`, `test_card_insert_remove.cpp`.  
- **Unknowns:** Full ISO7816 electrical — simplified memory/ATR/APDU model; document gaps in compat-validation.

## Coin (HW-COIN-001)

- **Ports:** `0x54`/`0x55` — `millennium_coin*.cpp`.  
- **Tests:** `test_coin_*`, `integration/test_coin_call.cpp`.  
- **Acceptance:** Pulse timing from UI insert events matches model parameters.
