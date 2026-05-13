# Test plan: Z180 / MMU / IRQ / timer / DMA / reset family

## Unit tests

| CMake target | File |
| ------------ | ---- |
| `test_z180_mmu_translation` | `tests/devices/test_z180_mmu_translation.cpp` |
| `test_mmu_register_readback` | `tests/devices/test_mmu_register_readback.cpp` |
| `test_memory_map_boot_regions` | `tests/devices/test_memory_map_boot_regions.cpp` |
| `test_stack_ram_mapping` | `tests/devices/test_stack_ram_mapping.cpp` |
| `test_z180_itc_il_registers` | `tests/devices/test_z180_itc_il_registers.cpp` |
| `test_z180_timer_registers` | `tests/devices/test_z180_timer_registers.cpp` |
| `test_z180_interrupt_trace` | `tests/devices/test_z180_interrupt_trace.cpp` |
| `test_interrupt_vector_loop` | `tests/devices/test_interrupt_vector_loop.cpp` |
| `test_z180_dma` | `tests/devices/test_z180_dma.cpp` |

## Integration / MAME traces

Enable env vars per mission Phase 5; collect `mmu-translation-trace.jsonl`, `interrupt-trace.jsonl`, `timer-trace.jsonl`, `memory-trace.jsonl`, `reset-trace.jsonl`.

## Failure criteria

- Tests pass but traces show impossible MMU state (address mismatch without firmware write).  
- Fake IRQ injection (must not appear in driver).

## Commands

```bash
ctest --output-on-failure -R 'test_z180|test_mmu|test_memory_map|test_stack'
```
