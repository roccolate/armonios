# Kernel size policy

ArmoniOS keeps a hard default ceiling of **128 KiB (131072 bytes)** for the
loadable QEMU `kernel.bin` image.

The image includes the kernel and the compiled KLI1 application blobs embedded in
bootfs. The ceiling is a development budget and regression gate, not a target that
new code should automatically consume.

## Source of truth

The default is defined by the top-level `GNUmakefile`, which delegates build rules
to `Makefile`.

The current image size is intentionally not copied into this document. It changes
as code and embedded applications change. Query the exact current value with:

```sh
make BOARD=qemu_virt size
```

A release or evidence record may preserve the measured size for one exact commit.
The live policy document should preserve only the limit and the method used to
measure it.

## Rules

1. Normal QEMU builds and hosted verification must enforce the 131072-byte limit.
2. Every feature that instantiates kernel code, runtime code, or embedded assets
   should report its meaningful image-size delta.
3. Per-application growth should also be inspected; archive extraction and
   `--gc-sections` do not replace measurement.
4. Prefer removing duplication, dead code, and obsolete compatibility paths
   before requesting a larger ceiling.
5. Changing the default requires an explicit project decision, updated build
   configuration, this document, current-state limits, and verification evidence.
6. Diagnostic or stress images may use a different budget only when clearly
   separated from the production build and described as non-release artifacts.
7. A successful build below the ceiling proves only size compliance, not boot or
   runtime correctness.

Developers may run a stricter local experiment without changing repository policy:

```sh
make KERNEL_SIZE_LIMIT=<bytes> BOARD=qemu_virt size
```

## Why the limit exists

The size gate protects the project's small-system character and makes accidental
code or asset growth visible. It also forces architectural choices to account for
the fact that compiled-in applications share the same loadable image budget.

The limit may evolve deliberately as the system becomes more capable, but it must
never drift through an undocumented build workaround.
