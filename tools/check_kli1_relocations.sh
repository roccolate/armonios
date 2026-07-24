#!/usr/bin/env bash

# Reject input-object relocations that cannot survive KLI1's load model.
#
# The kernel loader copies KLI1 images linked at address zero to a page-aligned
# EL0 base and applies no runtime fixups. PC-relative references and low-12-bit
# page-relative address materialisation remain valid after that move. Absolute
# pointers, GOT/dynamic-linker references, and TLS references do not.

set -euo pipefail

READELF="${READELF:-aarch64-linux-gnu-readelf}"
OBJCOPY="${OBJCOPY:-aarch64-linux-gnu-objcopy}"

if (( $# == 0 )); then
    echo "usage: $0 object.o [...]" >&2
    exit 2
fi

is_forbidden_relocation() {
    case "$1" in
        R_AARCH64_ABS16|R_AARCH64_ABS32|R_AARCH64_ABS64|\
        R_AARCH64_MOVW_UABS_*|R_AARCH64_MOVW_SABS_*|\
        R_AARCH64_ADR_GOT_PAGE|R_AARCH64_GOT_LD_PREL19|\
        R_AARCH64_LD64_GOT_LO12_NC|R_AARCH64_LD64_GOTOFF_LO15|\
        R_AARCH64_GOTREL32|R_AARCH64_GOTREL64|\
        R_AARCH64_GLOB_DAT|R_AARCH64_JUMP_SLOT|R_AARCH64_COPY|\
        R_AARCH64_RELATIVE|R_AARCH64_IRELATIVE|\
        R_AARCH64_TLS*|R_AARCH64_TLSDESC*)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

failed=0
index=0

for object in "$@"; do
    if [[ ! -f "$object" ]]; then
        printf 'FAIL: KLI1 relocation input not found: %s\n' "$object" >&2
        failed=1
        continue
    fi

    stripped="$tmp_dir/object-$index.o"
    index=$((index + 1))
    "$OBJCOPY" --strip-debug "$object" "$stripped"

    # A KLI1 image has no dynamic loader and no GOT fixup phase.
    if "$READELF" -SW "$stripped" | \
            grep -Eq '\.(got|got\.plt|dynamic|rela\.dyn|rela\.plt)([[:space:]]|$)'; then
        printf 'FAIL: %s contains a dynamic/GOT section unsupported by KLI1\n' \
               "$object" >&2
        failed=1
    fi

    while IFS= read -r relocation; do
        [[ -n "$relocation" ]] || continue
        if is_forbidden_relocation "$relocation"; then
            printf 'FAIL: %s uses forbidden KLI1 relocation %s\n' \
                   "$object" "$relocation" >&2
            failed=1
        fi
    done < <(
        "$READELF" -rW "$stripped" | awk '
            {
                for (i = 1; i <= NF; i++) {
                    if ($i ~ /^R_AARCH64_/) {
                        print $i
                    }
                }
            }
        '
    )
done

if (( failed != 0 )); then
    exit 1
fi

printf 'PASS: KLI1 relocation contract holds for %d object(s)\n' "$#"
