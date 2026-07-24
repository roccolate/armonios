# ArmoniOS SDK

This directory is the source template for the generated SDK bundle. Run:

```sh
make sdk
```

The generated bundle is written to `build/sdk/` and contains:

```text
include/armonios/abi/   public kernel ABI
include/libkarm/        freestanding runtime headers
lib/crt0.o              process entry object
lib/libkarm.a            static userland runtime
lib/kli1_header.o        shared KLI1 header object
lib/kli1_end.o           shared KLI1 tail marker
linker/image.ld          KLI1 linker contract
tools/                   KLI1 validation helpers
examples/hello-console/  external console example
```

## Current scope

The console SDK can compile a KLI1 image outside the ArmoniOS repository. The
kernel can load a regular KLI1 file from an absolute VFS path without embedding
or linking that application into `kernel.bin`.

Applications produced by this SDK are trusted sideloaded applications. ArmoniOS
does not yet claim a hardened sandbox for arbitrary third-party binaries.

## KLI1 constraints

- AArch64 little-endian flat image.
- One public entry is used by the current example.
- No mutable static `.data` or `.bss`; use the public memory API.
- No runtime relocator; absolute pointers requiring fixups are rejected.
- The current kernel process slot accepts images up to 8192 bytes.
- The current FAT32 test/distribution path uses 8.3 names.

## Build the example

From a copied SDK directory:

```sh
make -C examples/hello-console
```

The result is:

```text
examples/hello-console/build/HELLO.KLI
```

The example Makefile uses only files inside the generated SDK tree.

## Run the example in ArmoniOS

From the ArmoniOS source tree:

```sh
make external-kli-image
make qemu-external-kli
```

Then open the graphical Shell and run:

```text
run /fat/HELLO.KLI
```

`external-kli-image` builds the SDK example and places it in a separate FAT32
image. Creating that disk image does not rebuild or relink `kernel.bin`.
