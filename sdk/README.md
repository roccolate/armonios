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

This is the first console SDK foundation. It can compile a KLI1 image outside the
ArmoniOS repository, but the operating system does not yet load arbitrary KLI
files from FAT32. Until the VFS loader lands, generated files are build artifacts
used to prove the public toolchain and binary contract.

Applications produced by this SDK are trusted sideloaded applications. ArmoniOS
does not yet claim a hardened sandbox for arbitrary third-party binaries.

## KLI1 constraints

- AArch64 little-endian flat image.
- One public entry is used by the current example.
- No mutable static `.data` or `.bss`; use the public memory API.
- No runtime relocator; absolute pointers requiring fixups are rejected.
- The current kernel process slot accepts images up to 8192 bytes.

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
