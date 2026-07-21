# Raspberry Pi 4 EMMC2 storage track

## Status

- **Controller core:** IMPLEMENTED; HOST-VERIFIED.
- **Raspberry Pi 4 board integration:** FAIL-CLOSED.
- **Physical card initialization and sector reads:** UNVERIFIED.
- **Writes:** intentionally disabled.

The BCM2711 EMMC2 block is an SDHCI-compatible controller used for the
Raspberry Pi 4 SD-card path. ArmoniOS keeps this track separate from the QEMU
release line: code can be implemented and host-tested from the controller
contract, but hardware behavior is not claimed until a dated physical test
produces serial evidence.

## Implemented controller slice

`drivers/storage/emmc.c` currently provides a polling, PIO, read-only SD path:

1. 32-bit-only register access suitable for the BCM2711 integration;
2. host reset, 3.3 V power selection, timeout setup, and clock programming;
3. identification at 400 kHz and transfer clock selection up to 25 MHz;
4. CMD0, CMD8, CMD55/ACMD41, CMD2, CMD3, CMD9, and CMD7 initialization;
5. SD v1/SDSC and SD v2/SDHC addressing;
6. CMD16 for byte-addressed cards;
7. single-block CMD17 PIO reads, repeated for multi-sector requests;
8. CID, CSD, OCR, RCA, capacity mode, and actual-clock state in the device;
9. explicit timeout, command, data, unsupported-card, and read-only errors.

The BCM2711 integration requires combined 32-bit writes for the adjacent
block-size/block-count and transfer-mode/command register pairs. The driver
performs those combined writes and never emits 8- or 16-bit MMIO accesses; its
byte and half-word helpers are 32-bit read/modify/write operations.

## Host regression

`bash tests/run_emmc_sdhci_test.sh` builds the production driver with
`-Wall -Wextra -Werror` against a simulated SDHCI register bank. It verifies:

- the complete initialization command order;
- SDHC sector addressing;
- SDSC byte addressing and CMD16;
- RCA and clock state;
- 512-byte PIO data transfer;
- repeated CMD17 reads for a two-sector request;
- rejection of invalid LBA/count ranges before a command is issued;
- polling status enabled without signal interrupts;
- write rejection.

The standalone regression is part of `bash tools/verify.sh`.

## Why the board remains fail-closed

`drivers/boards/rpi4/board.c` does not advertise `BOARD_CAP_STORAGE`, does not
map EMMC2 MMIO, and returns failure from the generic storage entry points.
This prevents an unverified driver from being reached during ordinary RPi4
boot or from exposing FAT writes.

The controller base clock must come from firmware/Device Tree information.
BCM2711 must not rely on the SDHCI capability register as the authoritative
base-clock source. Board wiring will therefore remain disabled until the clock
query and physical read probe are implemented together.

## Next hardware milestones

1. implement the mailbox property call or DT clock handoff for EMMC2;
2. add an opt-in RPi4 storage-probe build that maps only the EMMC2 register
   range and prints every initialization stage over UART;
3. boot on a Raspberry Pi 4 and record CMD0-through-CMD7 completion;
4. read sector 0 and print a bounded hexadecimal/signature marker;
5. parse an MBR or superfloppy FAT boot sector read-only;
6. expose `BOARD_CAP_STORAGE` only after repeatable physical reads;
7. add four-bit mode and CMD18/CMD12 after single-block reads are stable;
8. implement writes only after read-only FAT mounting and disposable-card tests
   are repeatable.

No physical boot, card-read, FAT-mount, or write claim is made by this file.
