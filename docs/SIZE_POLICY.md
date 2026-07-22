# Kernel size policy

ArmoniOS keeps a hard default ceiling of **128 KiB (131072 bytes)** for the
loadable `kernel.bin` image.

The limit covers the kernel plus the KLI1 application blobs embedded in the
boot image. It is a development budget and regression gate, not a target that
new code should automatically consume.

Current reference image:

- size: **109982 bytes**;
- limit: **131072 bytes**;
- available headroom: **21090 bytes**.

Rules:

1. `make size` and the hosted verification workflows must enforce the limit.
2. New features should still report meaningful kernel and per-application size
   changes when they instantiate code or assets.
3. Prefer removing duplication and dead code before requesting another ceiling
   increase.
4. Changing the default limit requires an explicit project decision and an
   update to this document.
5. Temporary diagnostic images may exceed the production ceiling only when
   clearly separated from the normal build and documented as non-release
   artifacts.

The default is defined by the top-level `GNUmakefile`, which delegates all build
rules to `Makefile`. Developers may use `make KERNEL_SIZE_LIMIT=<bytes> size` for
a stricter local experiment without changing the project default.
