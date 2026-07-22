# Raspberry Pi 4 EMMC2 sector-0 probe

This directory describes the first physical storage test for ArmoniOS. The
probe is opt-in, runs before the normal MMU/device bootstrap, and never exposes
storage through the generic board API.

## Safety contract

- Use a spare SD card or preserve a backup of its boot partition.
- The ArmoniOS EMMC2 driver is compiled read-only.
- Every write request returns `EMMC_ERR_READ_ONLY`.
- The probe reads sector 0 and, only when needed, one FAT32 boot sector.
- It validates geometry read-only and never lists files or mounts the volume.
- `BOARD_CAP_STORAGE` remains disabled.
- The Raspberry Pi 4 `broken-cd` policy only allows commands to be attempted;
  it does not assert hot-plug support or prove that a card is inserted.

## Build locally

```sh
make rpi4-emmc2-probe
bash tools/package_rpi4_emmc2_probe.sh
```

The raw uncompressed AArch64 image is:

```text
build-rpi4-emmc2-probe/kernel8.img
```

The reproducible package is:

```text
build-rpi4-emmc2-probe/package/
├── kernel8.img
├── config.txt
├── README.md
├── COMMIT
└── SHA256SUMS
```

`COMMIT` records the exact Git revision. Verify the package before copying it:

```sh
cd build-rpi4-emmc2-probe/package
sha256sum -c SHA256SUMS
```

The packaging command rejects an image larger than 108000 bytes before copying
or hashing it. Override `KERNEL_SIZE_LIMIT` only for an intentional size-policy
experiment, not for a normal probe build.

## Download the CI package

The `Verify ArmoniOS` workflow publishes an artifact named:

```text
rpi4-emmc2-probe-<full-commit-sha>
```

The artifact contains the same five files as the local package and is retained
for 30 days. The workflow still runs the host, size, stack, and QEMU gates after
building the package; an artifact is not physical-hardware evidence.

## Prepare a test card

Use an existing Raspberry Pi 4 boot partition on a spare card so current
firmware, the BCM2711 DTB, and overlays are already present.

1. Back up its original `config.txt` and kernel image.
2. Verify `SHA256SUMS` from the local or CI package.
3. Copy the package's `kernel8.img` to the boot partition.
4. Merge the package's `config.txt` into the boot partition configuration.
5. Record the exact value from the package's `COMMIT` file.
6. Keep Device Tree enabled and do not apply `disable-emmc2`.

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
EMMC2 probe: broken-cd assume-present
EMMC2 probe: init 0
EMMC2 probe: read0 0
EMMC2 s0 <32 hexadecimal digits> <4 hexadecimal digits>
EMMC2 fat <layout> <type> <base-lba> <sectors-per-cluster> <sectors-per-fat> <root-cluster> <total-sectors>
```

The clock query translates the early ARM physical message buffer through the
BCM2711 `/soc` DMA alias (`+ 0xc0000000`) before sending property channel 8.
The buffer is static, 16-byte aligned, below 1 GiB, and queried before normal
MMU/cache bring-up.

Raspberry Pi 4 marks EMMC2 as `broken-cd`. The probe therefore treats the card
as present for initialization instead of waiting on unreliable native
`PRESENT_STATE` card-detect bits. All command/data inhibit bits, interrupts,
responses, and sector data remain real controller values. A missing card should
fail through the command or timeout markers after the assume-present marker.

`EMMC2 s0` records the first 16 bytes and bytes 510-511. `55AA` is common
for an MBR or FAT boot sector, but the raw bytes remain evidence rather than a
success condition.

`EMMC2 fat` uses positional fields to save image space. `layout` is `0` for a
superfloppy BPB at LBA 0 and `1` for a primary MBR partition. `type` is `00` for
superfloppy or the MBR type byte (`0B`, `0C`, `1B`, or `1C`). The remaining
values come from the existing FAT32 mount validator. `EMMC2 fat none` means no
valid superfloppy BPB or supported primary FAT32 partition was found;
`EMMC2 fat bad <lba>` means MBR discovery succeeded but that partition's BPB or
geometry was rejected.

## Failure telemetry

When `init` or `read0` returns a negative result, the probe prints one bounded
line:

```text
EMMC2 diag c <command|-1> a <argument> r <read-offset> p <present> k <clock-reset> h <host-power> i <last-interrupt> f <actual-clock-hz>
```

The fields are deliberately compact to keep `kernel8.img` under the 108000-byte
limit:

- `c`: last SD command index; `-1` means no command reached the controller;
- `a`: last command argument;
- `r`: last MMIO offset read by the SDHCI driver;
- `p`: raw `PRESENT_STATE` before the two `broken-cd` bits are supplied;
- `k`: current clock/reset register;
- `h`: current host-control/power register pair;
- `i`: last non-zero `INT_STATUS`, retained after acknowledgement;
- `f`: actual card clock selected by the driver.

The adapter observes real 32-bit MMIO accesses. It does not synthesize a command
response or interrupt. The only synthetic values remain the two `broken-cd`
presence bits returned from reads of offset `0x24`; the raw value is reported in
`p`.

Useful interpretations:

- `c -1` and `r 0x2c`: controller reset or clock stabilization;
- `c -1` and `r 0x24`: card-detect or command/data-inhibit wait before CMD0;
- `r 0x30` after a command: response, data-ready, or data-end interrupt wait;
- `c 17`: the sector-zero read reached CMD17.

Relevant `i` error bits are:

- `0x00010000`: command timeout;
- `0x00020000`: command CRC;
- `0x00040000`: command end-bit;
- `0x00080000`: command index;
- `0x00100000`: data timeout;
- `0x00200000`: data CRC;
- `0x00400000`: data end-bit;
- `0x00800000`: bus-power error.

Failure localization is explicit:

- `EMMC2 clock: unavailable -1` means local address, alias, or argument
  validation failed;
- `EMMC2 clock: unavailable -2` means the mailbox FIFO or matching firmware
  response timed out;
- `EMMC2 clock: unavailable -3` means firmware returned a malformed, rejected,
  or zero clock response;
- absence of `EMMC2 probe: broken-cd assume-present` means the expected opt-in
  probe image was not reached;
- `EMMC2 probe: init <negative>` means SDHCI/card initialization failed and is
  followed by `EMMC2 diag`;
- `EMMC2 probe: read0 <negative>` means initialization succeeded but sector-0
  transfer failed and is followed by `EMMC2 diag`.

## Evidence to record

Physical evidence should include:

- Raspberry Pi 4 revision and RAM size;
- firmware/bootloader date when available;
- SD-card make, model, and capacity;
- the value in `COMMIT`;
- the verified `kernel8.img` SHA-256;
- complete UART log from power-on through the probe markers;
- whether the result is repeatable after a cold power cycle.

Do not mark Raspberry Pi storage supported from a single partial marker or from
the fact that the probe image compiled.
