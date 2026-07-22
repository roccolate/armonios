# Raspberry Pi 4 AArch64 boot entry

## Status

- **EL1 entry from an EL1 loader:** IMPLEMENTED; QEMU-VERIFIED.
- **EL2 to EL1 transition:** IMPLEMENTED; QEMU-VERIFIED with virtualization enabled.
- **Secondary-core policy:** IMPLEMENTED as park-on-entry.
- **Physical Raspberry Pi 4 serial boot:** UNVERIFIED.

ArmoniOS is an EL1 kernel. Its exception vectors, MMU code, timer setup, and
EL0 return path all use EL1 architectural state. A 64-bit platform loader may
enter the image at EL2, so `_start` must normalize the execution level before
calling any C code.

## Entry contract

`boot/start.S` now performs this sequence:

1. preserve the DTB pointer from `x0`;
2. read `MPIDR_EL1` and park every core except affinity-0;
3. mask debug, SError, IRQ, and FIQ exceptions;
4. accept only EL1 or EL2 entry;
5. when entered at EL2:
   - set `HCR_EL2.RW` so EL1/EL0 execute AArch64;
   - permit EL1 physical counter and timer access through `CNTHCTL_EL2`;
   - clear `CNTVOFF_EL2` and `VTTBR_EL2`;
   - preserve required `SCTLR_EL1` state while clearing MMU and cache enables;
   - set `SPSR_EL2` for masked EL1h;
   - return through `ERET` to the common EL1 entry;
6. install the EL1 stack and vector table;
7. clear BSS and call `kernel_main(dtb)`.

Unsupported exception levels halt before shared state is touched.

## QEMU runtime gate

`bash tools/qemu_el2_entry_test.sh` starts QEMU `virt` with
`virtualization=on`, boots the normal kernel image, and requires:

- `MMU: active`;
- `display: windows`;
- `panel: ready`;
- no kernel panic marker.

Reaching the panel exercises the EL1 exception-return path into an EL0 KLI1
application, rather than proving only that the assembly compiled.

The gate is part of `bash tools/verify.sh`.

## Physical evidence boundary

The QEMU gate proves the architectural transition under QEMU. It does not
prove Raspberry Pi firmware entry, DTB placement, UART routing, or secondary
core behavior on a physical board. Those remain UNVERIFIED until a dated Pi 4
serial log exists.

The next hardware image may now safely add the opt-in EMMC2 sector-0 probe on
top of this entry contract. Normal `BOARD=rpi4` remains fail-closed for
storage.
