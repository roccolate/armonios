# Canonical VFS paths

ArmoniOS resolves every VFS path through one bounded canonicalization contract
before node lookup, mount selection, or filesystem callbacks.

## Canonical form

A valid path:

- is absolute;
- fits in `VFS_MAX_PATH`, including the terminating null byte;
- collapses repeated `/` separators;
- removes `.` components;
- resolves `..` by removing the preceding component;
- rejects any `..` sequence that would escape above `/`;
- removes a trailing slash unless the result is `/`.

Examples:

```text
/fat//a/./b/../c.txt/  ->  /fat/a/c.txt
////./                  ->  /
/a/b/../../             ->  /
/../escape              ->  rejected
relative/path           ->  rejected
```

The raw input is also bounded. An overlong path is rejected even when removing
components could theoretically shorten it. This keeps every scan inside the
fixed kernel path buffer contract.

## Stored identity

Static nodes and mount roots are stored only in canonical form. Therefore these
names identify the same object and cannot be registered twice:

```text
/disk/sub
/disk//sub/
/disk/x/../sub
```

Lookup, stat, read, write, list, open, unlink, unmount, and rename all normalize
before touching VFS tables.

## Mount resolution

Mount selection runs on the canonical path and chooses the longest component
prefix. Given mounts at `/disk` and `/disk/sub`, this path belongs to the nested
mount:

```text
/disk//sub/tmp/../item  ->  /disk/sub/item
```

A textual prefix is not enough: `/diskette/item` does not belong to `/disk`.
Normalization also happens before mount selection, so traversal cannot be used
to make one filesystem receive a path owned by another:

```text
/disk/sub/../../other/file  ->  /other/file
```

Filesystem callbacks receive the canonical absolute path. Adapters do not need
to collapse separators or interpret `.` and `..` themselves.

## Rename boundary

Rename remains limited to one mounted filesystem. Both paths are canonicalized
first and must resolve to the same mount object. Cross-mount rename is rejected.

## Scope

This policy defines lexical path identity and mount dispatch. It does not yet
add:

- current working directories or relative paths;
- symbolic links;
- per-process mount namespaces;
- directory inode traversal;
- case folding or Unicode normalization.
