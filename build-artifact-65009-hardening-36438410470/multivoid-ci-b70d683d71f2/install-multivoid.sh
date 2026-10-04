#!/usr/bin/env bash

set -u

PROFILE_ROOT="$HOME/.config/r2modmanPlus-local/VotV/profiles"
MOD_REL="shimloader/mod/Pelmentor-Multivoid/dlls/main.dll"
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

echo "=== Multivoid DLL Installer ==="
echo

mapfile -t SOURCES < <(
    find "$SCRIPT_DIR" -maxdepth 4 -type f -name 'main.dll' \
        ! -path '*/r2modmanPlus-local/*' \
        -printf '%T@ %p\n' 2>/dev/null |
    sort -nr |
    cut -d' ' -f2-
)

if (( ${#SOURCES[@]} == 0 )); then
    echo "ERROR: Could not find a new main.dll."
    echo
    echo "Put this installer in the same folder as the downloaded build"
    echo "or somewhere above its main.dll, then run it again."
    echo
    read -rp "Press Enter to close..."
    exit 1
fi

NEW="${SOURCES[0]}"

echo "New DLL:"
echo "  $NEW"
echo

if [[ ! -d "$PROFILE_ROOT" ]]; then
    echo "ERROR: VotV r2modman profile folder was not found:"
    echo "  $PROFILE_ROOT"
    echo
    read -rp "Press Enter to close..."
    exit 1
fi

mapfile -t TARGETS < <(
    find "$PROFILE_ROOT" -type f -path "*/$MOD_REL" -print 2>/dev/null | sort
)

if (( ${#TARGETS[@]} == 0 )); then
    echo "ERROR: No installed Pelmentor-Multivoid main.dll was found."
    echo
    read -rp "Press Enter to close..."
    exit 1
fi

echo "Installed profile(s) found:"
for i in "${!TARGETS[@]}"; do
    profile="${TARGETS[$i]#"$PROFILE_ROOT"/}"
    profile="${profile%%/*}"
    printf '  [%d] %s\n' "$((i + 1))" "$profile"
done
echo

SELECTED=()

if (( ${#TARGETS[@]} == 1 )); then
    SELECTED=("${TARGETS[0]}")
else
    echo "Enter:"
    echo "  a        = update ALL listed profiles"
    echo "  1 2 ...  = update specific profile numbers"
    echo "  q        = quit"
    echo
    read -rp "Selection [a]: " choice
    choice="${choice:-a}"

    if [[ "$choice" =~ ^[Qq]$ ]]; then
        echo "Cancelled."
        exit 0
    fi

    if [[ "$choice" =~ ^[Aa]$ ]]; then
        SELECTED=("${TARGETS[@]}")
    else
        for n in $choice; do
            if [[ ! "$n" =~ ^[0-9]+$ ]] || (( n < 1 || n > ${#TARGETS[@]} )); then
                echo "ERROR: Invalid selection: $n"
                read -rp "Press Enter to close..."
                exit 1
            fi
            SELECTED+=("${TARGETS[$((n - 1))]}")
        done
    fi
fi

echo
echo "=== SOURCE HASH ==="
sha256sum "$NEW"
echo

SOURCE_HASH="$(sha256sum "$NEW" | awk '{print $1}')"
STAMP="$(date +%Y%m%d-%H%M%S)"
FAILED=0

for TARGET in "${SELECTED[@]}"; do
    profile="${TARGET#"$PROFILE_ROOT"/}"
    profile="${profile%%/*}"
    BACKUP="$TARGET.bak-$STAMP"

    echo "=== Installing to profile: $profile ==="
    echo "Current:"
    sha256sum "$TARGET"

    echo
    echo "Backing up:"
    echo "  $BACKUP"

    if ! cp -p -- "$TARGET" "$BACKUP"; then
        echo "ERROR: Backup failed. This profile was NOT modified."
        echo
        FAILED=1
        continue
    fi

    if ! cp -- "$NEW" "$TARGET"; then
        echo "ERROR: Install failed. Restoring backup..."
        cp -p -- "$BACKUP" "$TARGET" || true
        FAILED=1
        continue
    fi

    echo
    echo "Installed:"
    sha256sum "$TARGET"

    TARGET_HASH="$(sha256sum "$TARGET" | awk '{print $1}')"

    if [[ "$SOURCE_HASH" == "$TARGET_HASH" ]]; then
        echo "PASS: hashes match."
    else
        echo "ERROR: hash mismatch. Restoring backup..."
        cp -p -- "$BACKUP" "$TARGET" || true
        FAILED=1
    fi

    echo
done

if (( FAILED == 0 )); then
    echo "=== INSTALL COMPLETE ==="
else
    echo "=== INSTALL FINISHED WITH ERRORS ==="
fi

echo
read -rp "Press Enter to close..."
