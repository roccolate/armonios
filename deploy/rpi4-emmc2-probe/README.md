# Raspberry Pi 4 EMMC2 sector-0 probe

This directory describes the first physical storage test for ArmoniOS. The
probe is opt-in, runs before the normal MMU/device bootstrap, and never exposes
storage through the generic board API.

## Safety contract

- Use a spare SD card or preserve a backup of its boot partition.
- The ArmoniOS EMMC2 driver is compiled read-only.
- Every write request returns `EMMC_ERR_READ_ONLY`.
- The probe reads exactly sector 0 and prints a bounded summary.
- `BOARD_CAP_STORAGE` remains disabled.

## Build

```sh
make rpi4-emmc2-probe
```

The resulting uncompressed AArch64 image is:

```text
build-rpi4-emmc2-probe/kernel8.img
```

CI also builds this path through `tests/run_board_build_test.sh`, but CI cannot
prove mailbox or SD-card behavior on physical hardware.

## Prepare a test card

Use an existing Raspberry Pi 4 boot partition on a spare card so current
firmware, the BCM2711 DTB, and overlays are already present.

1. Back up its original `config.txt` and kernel image.
2. Copy `build-rpi4-emmc2-probe/kernel8.img` to the boot partition.
3. Merge the lines from this directory's `config.txt` into the boot
   partition's configuration.
4. Keep Device Tree enabled and do not apply `disable-emmc2`.

`dtoverlay=disable-bt` restores the first PL011 UART to GPIO 14 and GPIO 15 on
a Raspberry Pi 4. ArmoniOS uses PL011 UART0 at `0xFE201000`.

## Serial connection

Use a **3.3 V** USB-to-TTL serial adapter:

- Pi pin 8 / GPIO14 / TXD0 -> adapter RX;
- Pi pin 10 / GPIO15 / RXD0 -> adapter TX, optional for this probe;
- Pi ground -> adapter ground;
- 115200 baud, 8 data bits, no parity, 1 stop bit.

Do not connect a 5 V serial signal to Raspberry Pi GPIO.

## Expected markers

A successful firmware and card path should include output shaped like:

```text
EMMC2 clock: <non-zero rate>
EMMC2 probe: begin
EMMC2 probe: init 0
EMMC2 probe: read0 0
EMMC2 probe: first16 <32 hexadecimal digits>
EMMC2 probe: signature <4 hexadecimal digits>
```

`55AA` is common for an MBR or boot sector signature, but the probe records the
actual bytes and does not treat a different value as a driver failure.

Failure localization is explicit:

- `EMMC2 clock: unavailable` means the firmware mailbox transaction failed;
- `EMMC2 probe: init <negative>` means SDHCI/card initialization failed;
- `EMMC2 probe: read0 <negative>` means initialization succeeded but sector-0
  transfer failed.

## Evidence to record

Physical evidence should include:

- Raspberry Pi 4 revision and RAM size;
- firmware/bootloader date when available;
- SD-card make, model, and capacity;
- exact ArmoniOS commit;
- complete UART log from power-on through the probe markers;
- whether the result is repeatable after a cold power cycle.

Do not mark Raspberry Pi storage supported from a single partial marker or from
the fact that the probe image compiled.
