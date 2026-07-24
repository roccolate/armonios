#!/usr/bin/env python3
from pathlib import Path

STATUS = "docs/V03_STORAGE_VFS_STATUS.md"


def replace_once(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text()
    if text.count(old) != 1:
        raise SystemExit(f"expected exactly one match in {path}: {old!r}")
    p.write_text(text.replace(old, new, 1))


def insert_after_title(path: str, title: str) -> None:
    p = Path(path)
    text = p.read_text()
    marker = f"{title}\n"
    note = (
        f"{title}\n\n"
        f"> Post-audit v0.3 storage/VFS implementation status: "
        f"[`V03_STORAGE_VFS_STATUS.md`](V03_STORAGE_VFS_STATUS.md).\n"
    )
    if "V03_STORAGE_VFS_STATUS.md" in text:
        return
    if text.count(marker) != 1:
        raise SystemExit(f"title mismatch in {path}")
    p.write_text(text.replace(marker, note, 1))


insert_after_title("docs/CURRENT_STATE.md", "# Current State")
insert_after_title("docs/ROADMAP.md", "# Roadmap")
insert_after_title("docs/ARCHITECTURE.md", "# Architecture")
insert_after_title("docs/SYSCALLS.md", "# Syscall Reference")

replace_once(
    "docs/CURRENT_STATE.md",
    "| v0.3 storage/VFS platform | NEXT AFTER v0.2 | No common path resolver, rich block descriptor, or structured filesystem ABI. |",
    "| v0.3 storage/VFS platform | IN PROGRESS | Block devices, bounded views, canonical paths, mount resolution, and nested FAT32 traversal have landed; structured public metadata remains in PR #95. |",
)
replace_once(
    "docs/CURRENT_STATE.md",
    "- generic block descriptor and flush/read-only contract;\n- normalized path and mount resolver;\n- structured directory/metadata ABI;\n- mkdir, truncate, structured stat/readdir, filesystem info;",
    "- native structured directory/metadata plumbing and promotion of PR #95;\n- mkdir, rmdir, truncate, filesystem information, and specific VFS errors;",
)
replace_once(
    "docs/ROADMAP.md",
    "**State: NEXT AFTER v0.2**",
    "**State: IN PROGRESS**\n\nBlock-device descriptors/views, direct FAT32 device mounting, canonical paths,\nlongest-prefix mount resolution, and nested FAT32 traversal are implemented.\nStructured metadata is the active ABI cut; see `V03_STORAGE_VFS_STATUS.md`.",
)
replace_once(
    "docs/ARCHITECTURE.md",
    "Missing v0.3 foundations:\n\n- common path normalization and traversal policy;\n- structured directory entries and metadata;\n- rich block-device identity/capacity/read-only/flush contract;\n- generic mkdir and truncate;\n- structured stat, readdir, and filesystem-info ABI;\n- complete filesystem driver lifecycle.",
    "Landed v0.3 foundations include the generic block-device contract, bounded\npartition views, direct FAT32 device mounting, canonical absolute paths,\nlongest-prefix mount resolution, and nested FAT32 8.3 traversal. Remaining work\nis native structured metadata, filesystem information, specific errors, mkdir,\nrmdir, truncate, durable flush semantics, and the complete driver lifecycle.",
)
replace_once(
    "docs/SYSCALLS.md",
    "FAT32 syscall scope:\n\n- root directory only;\n- short 8.3 names only;\n- create, read, write, seek, rename, delete, stat, and list;\n- no subdirectories, long-file-name ABI, partition discovery, ext2 mount, or structured directory entries.",
    "FAT32 syscall scope:\n\n- existing nested 8.3 directories can be traversed, listed, statted, and opened;\n- creation, unlink, and rename remain limited to the volume root;\n- short 8.3 names only; long-file-name entries are not exposed;\n- primary-MBR discovery and bounded partition views are implemented;\n- no ext2 mount, mkdir/rmdir, nested mutation, or promoted structured directory ABI yet.",
)

readme = Path("README.md")
text = readme.read_text()
if "V03_STORAGE_VFS_STATUS.md" not in text:
    marker = "5. [Architecture](docs/ARCHITECTURE.md) — implemented design."
    replacement = marker + "\n6. [v0.3 Storage/VFS Status](docs/V03_STORAGE_VFS_STATUS.md) — post-audit implementation status and next cuts."
    if text.count(marker) != 1:
        raise SystemExit("README read-first marker mismatch")
    text = text.replace(marker, replacement, 1)
text = text.replace(
    "| FAT32 | Root directory, 8.3 names, no subdirectories or long names. |",
    "| FAT32 | Nested existing 8.3 traversal; mutation remains root-only; no long names. |",
    1,
)
readme.write_text(text)
