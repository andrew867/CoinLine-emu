# Memory map debug

## Physical map (Millennium MAME driver)

| Physical range        | Read                         | Write                                |
|----------------------|------------------------------|--------------------------------------|
| **0x00000–0xBFFFF** | Flash (`flash` region), **or** overlay byte if written | **Persist** to overlay (same range) |
| **0xC0000–0xDFFFF** | `m_phys_ram`                 | `m_phys_ram`                         |
| **0xE0000–0xFFFFF** | Flash high mirror            | Ignored (ROM)                        |
| Profile extras       | NVRAM/table/DLA from JSON    | Model-backed                         |

## Firmware expectation (trace-backed layout)

- **Banked BOOTCODE**: logical **0x5000–0x7FFF** programmed at physical **0x5000–…** in flash (bank segment).
- **SRAM/BSS/STACK**: physical **0xC0002**, **0xC7BAE**, etc.

## Past failure mode

Z180 MMU maps **logical** stack references (e.g. in the **0x5000–0x7FFF** window with **BBR=0x00**) to **physical 0x05xxxx–0x07xxxx**. The previous `.rom()` mapping treated writes as **no-ops**, breaking stack semantics even though **MMU register trace looked healthy**.

## Overlay rationale

We **do not** replace MAME’s MMU. We model **write persistence** in low physical memory **consistent with** observed flash-backed regions plus scratch/stack behavior during bring-up.
