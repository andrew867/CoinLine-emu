# Boot-critical gap register (summary)

**Canonical JSON:** [`boot-critical-gap-register.json`](boot-critical-gap-register.json)

This register lists **only** items on the boot path. Full catalog remains in [`hardware-gap-register.json`](hardware-gap-register.json).

## Status legend

Same categories as the hardware gap register: `complete_verified`, `partial`, `unknown`, etc.

## Priority order

1. INT / TMR / RES tracing enough to see **EI vs poll** and timer programming.  
2. ASCI/modem **STAT0** and line defaults **without** forged host traffic.  
3. MACH PIO **readback** matching firmware branches to display init.  
4. Voiceware **only** if traces show boot waits on busy/ready.  
5. NVRAM **only** if nvram/table trace shows touches before first `0x60`.  
6. VFD handler fixes **only** when firmware touches display ports or source proves missing status behavior.
