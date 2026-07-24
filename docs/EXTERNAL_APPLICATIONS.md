# External applications

ArmoniOS can build a KLI1 application outside the repository, store it as an
ordinary FAT32 file, load it through the VFS, and execute it as an isolated EL0
process without embedding that application in `kernel.bin`.

The current demonstrated route is:

```text
external C source
  -> generated ArmoniOS SDK
  -> libkarm.a + crt0.o
  -> generic KLI1 packaging objects
  -> HELLO.KLI
  -> FAT32 regular file
  -> descriptor-free VFS path read
  -> KLI1 validator
  -> existing process/VM/stack spawn lifecycle
  -> EL0 execution
```

This is a trusted sideloading foundation. It is not yet a hardened sandbox for
arbitrary downloaded binaries.

## Public SDK

Generate the console SDK with:

```sh
make sdk
```

The generated bundle is written to `build/sdk/`:

```text
build/sdk/
├── include/
│   ├── armonios/abi/
│   └── libkarm/
├── lib/
│   ├── crt0.o
│   ├── libkarm.a
│   ├── kli1_header.o
│   └── kli1_end.o
├── linker/
│   └── image.ld
├── tools/
│   └── check_kli1_relocations.sh
├── examples/
│   └── hello-console/
└── README.md
```

The SDK contains no kernel-private headers. Its permanent gate copies the bundle
to an isolated directory and rebuilds the example using only that copy.

## Build an external KLI1 application

The bundled example can be built from the generated SDK:

```sh
make -C build/sdk/examples/hello-console \
    SDK="$PWD/build/sdk"
```

The output is:

```text
build/sdk/examples/hello-console/build/HELLO.KLI
```

The example links statically against `libkarm.a`. There is no dynamic loader or
shared-library dependency.

## Create the FAT32 application disk

Build the SDK example and place it in a separate FAT32 image:

```sh
make external-kli-image
```

This creates:

```text
build/external-kli.img
```

The image contains the external application as the ordinary root 8.3 file:

```text
/fat/HELLO.KLI
```

Creating this disk does not rebuild or relink `kernel.bin`. The permanent gate
records both the hash and modification time of `kernel.bin`, creates the FAT32
image, parses the filesystem independently, follows the file cluster chain, and
compares the stored bytes with the SDK artifact.

## Run the application

Start the visible QEMU desktop with the external-application disk:

```sh
make qemu-external-kli
```

In the graphical Shell, run:

```text
run /fat/HELLO.KLI
```

The historical `run editor` and other built-in application names still resolve
to embedded `/armonios/<name>` images. An absolute path supplied to `run` is
canonicalized and resolved through the VFS-backed loader.

At the syscall layer an application may invoke the same route directly:

```c
long pid = kli_spawn("/fat/HELLO.KLI", 0);
```

`SYS_SPAWN_ARGV` is supported by the same image-source resolver and process
installation path.

## Loader behavior

The external loader preserves the existing bounded process lifecycle:

1. canonicalize and classify the application path;
2. allocate a process slot and keep it `BLOCKED`;
3. allocate and zero the fixed image pages;
4. query VFS metadata and require a regular file;
5. reject an empty, truncated, or oversized image;
6. read the complete file through mount-owned direct path reads;
7. validate the KLI1 header, exact image size, entry index, and instruction
   alignment;
8. create the page table and stack;
9. copy optional argv data into the new stack;
10. transfer owned pages to the process;
11. mark the process `READY` only after all installation steps succeed.

Any failure before the final transition follows the existing process and PMM
rollback paths.

## Descriptor-free VFS reads

The loader does not borrow a file descriptor from the calling application.
Mounted filesystems may provide a `read_path` callback used by kernel-internal
callers. FAT32 implements this callback with a local `fat32_file_t`, so loading
an application does not consume:

- one of the caller's local descriptors;
- one of the global VFS open-file handles;
- one of FAT32's bounded persistent materialized-file slots.

Static VFS nodes retain precedence over mount callbacks.

## KLI1 contract

External applications currently obey these constraints:

- AArch64 little-endian flat image;
- public 80-byte KLI1 header;
- maximum of eight declared entries;
- selected entry must be inside the image and four-byte aligned;
- complete file size must equal the KLI1 `image_size` field;
- current image-slot maximum is 8192 bytes;
- no mutable static `.data` or `.bss`;
- no runtime relocator;
- absolute pointers, GOT references, TLS relocations, and other unsupported
  fixups are rejected by the SDK relocation gate;
- mutable state must use stack storage or public mapped memory.

The current fixed slot is a deliberate first milestone, not a permanent package
size target.

## Automated runtime evidence

The dedicated QEMU smoke uses a separate non-production build. It replaces only
the first boot image with the SDK-built `/fat/HELLO.KLI`; normal builds retain
the embedded Panel startup path.

The stronger runtime fixture uses an external parent application that:

1. starts in EL0 from the FAT32 KLI;
2. calls `SYS_SPAWN_ARGV` on `/fat/HELLO.KLI`;
3. yields while the VFS-loaded child runs;
4. waits for the child;
5. verifies a clean child exit;
6. exits cleanly itself;
7. returns control to EL1 without a kernel panic.

The workflow requires serial markers from both parent and child rather than
inferring execution from build artifacts.

## Current boundaries

The external platform does not yet include:

- `libarmdesk.a` in the generated SDK;
- an external graphical window example;
- variable-size KLI process images;
- long FAT filenames;
- package metadata, signatures, dependencies, or installation transactions;
- capability-based syscall restrictions;
- fault-contained `copyin`/`copyout` for hostile binaries;
- a package manager or application store;
- stable compatibility guarantees beyond the documented public ABI and KLI1
  contract.

The next platform milestone is to promote the compiled `libarmdesk` object model
and build a windowed external application through the same SDK/FAT32 loader path.
