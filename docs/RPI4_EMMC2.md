# Raspberry Pi 4 EMMC2 storage track

## Status

- **Controller core:** IMPLEMENTED; HOST-VERIFIED.
- **Firmware mailbox clock protocol:** IMPLEMENTED; HOST-VERIFIED.
- **EL2 to EL1 boot entry:** IMPLEMENTED; QEMU-VERIFIED.
- **Opt-in sector-0 probe image:** IMPLEMENTED; BUILD-VERIFIED when CI passes.
- **Normal Raspberry Pi 4 storage integration:** FAIL-CLOSED.
- **Physical clock response, card initialization, and sector reads:** UNVERIFIED.
- **Writes:** intentionally disabled.

The BCM2711 EMMC2 block is an SDHCI-compatible controller used for the
Raspberry Pi 4 SD-card path. ArmoniOS keeps this track separate from the QEMU
release line: code can be implemented and host-tested from the controller and
firmware contracts, but hardware behavior is not claimed until a dated
physical test produces serial evidence.

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

## Firmware clock query

`drivers/firmware/rpi_mailbox.c` implements the synchronous property-channel
transaction needed to request the EMMC2 base clock:

- waits for space in the mailbox write FIFO;
- sends one 16-byte-aligned property message on channel 8;
- uses `GET_CLOCK_RATE` with firmware clock ID 12 (`EMMC2`);
- waits for the matching channel/address response;
- validates both the global firmware status and the per-tag response length;
- rejects zero or malformed rates and bounded-poll timeouts.

`board_early_init()` performs this query immediately after UART initialization,
before normal MMU bring-up. A successful response is printed as
`EMMC2 clock: <rate>`; a failure prints `EMMC2 clock: unavailable` and does not
change board capabilities or make storage reachable.

The physical mailbox address translation and firmware response remain
unverified until a real Raspberry Pi 4 serial boot records the marker.

## Opt-in physical probe

`make rpi4-emmc2-probe` builds
`build-rpi4-emmc2-probe/kernel8.img` with
`ARMONIOS_RPI4_EMMC2_PROBE=1`.

Only that build performs the early diagnostic sequence:

1. query the EMMC2 base clock through firmware;
2. initialize the SDHCI/card path at `RPI4_EMMC_BASE`;
3. read exactly sector 0;
4. print the first 16 bytes and bytes 510-511 over UART.

The probe uses static kernel buffers and the read-only controller. It does not
advertise `BOARD_CAP_STORAGE`, does not route the device through VFS/FAT, and
does not call a write operation. The normal `make BOARD=rpi4` image excludes
the probe path at preprocessing time.

Boot configuration, serial wiring, markers, and evidence requirements live in
`deploy/rpi4-emmc2-probe/`.

## Automated regressions

`bash tests/run_emmc_sdhci_test.sh` builds the production SDHCI driver with
`-Wall -Wextra -Werror` against a simulated register bank. It verifies:

- the complete initialization command order;
- SDHC sector addressing;
- SDSC byte addressing and CMD16;
- RCA and clock state;
- 512-byte PIO data transfer;
- repeated CMD17 reads for a two-sector request;
- rejection of invalid LBA/count ranges before a command is issued;
- polling status enabled without signal interrupts;
- write rejection.

`bash tests/run_rpi_mailbox_test.sh` builds the production mailbox code against
a simulated FIFO. It verifies:

- the exact property message and EMMC2 clock ID;
- property channel and aligned message address encoding;
- global and per-tag firmware response handling;
- write-FIFO and response-FIFO timeout paths;
- invalid clock IDs and misaligned addresses.

`tests/run_board_build_test.sh` builds both the normal RPi4 image and the
opt-in `kernel8.img`, enforcing the configured kernel-size limit on both.
These regressions are part of `bash tools/verify.sh`.

## Why the board remains fail-closed

`drivers/boards/rpi4/board.c` always returns zero capabilities and failure from
the generic storage entry points. EMMC2 MMIO is not included in the normal
post-MMU board map. The probe is an early, compile-time-selected diagnostic,
not a storage backend.

The base clock is no longer guessed from the SDHCI capability register. It is
requested from firmware and passed to the controller only in the opt-in probe.

## Next hardware milestones

1. boot the probe on a Raspberry Pi 4 and record the firmware clock marker;
2. record EMMC2 initialization and sector-0 markers across cold boots;
3. add finer command-stage diagnostics only where the physical log needs them;
4. parse an MBR or superfloppy FAT boot sector read-only;
5. expose `BOARD_CAP_STORAGE` only after repeatable physical reads;
6. add four-bit mode and CMD18/CMD12 after single-block reads are stable;
7. implement writes only after read-only FAT mounting and disposable-card tests
   are repeatable.

No physical clock, card-read, FAT-mount, or write claim is made by this file.
