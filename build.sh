#!/usr/bin/env bash
# build.sh — Build Nomad2026 on NTFS filesystem (no exec bit support)
#
# Problem: juceaide lives on NTFS and can't be executed. The cmake
# configure step (run by cmake_check_build_system on every make) tests
# juceaide and fails. Solution: copy to /tmp where exec works, patch
# all build references, then invoke make via Makefile2 (no reconfigure).
#
# Usage:  bash build.sh [--configure] [--run] [extra make args...]
#   --configure   Force cmake reconfigure first (needed after CMakeLists changes)
#   --run         Launch the binary after a successful build
#   --jobs N      Override number of parallel jobs (default: nproc)

set -e
cd "$(dirname "$0")"
ROOT="$(pwd)"
BUILD="$ROOT/build"

NTFS_AIDE="$BUILD/JUCE/tools/extras/Build/juceaide/juceaide_artefacts/Custom/juceaide"
TMP_AIDE="/tmp/juceaide_nomad"

DO_CONFIGURE=0
DO_RUN=0
JOBS=$(nproc)
EXTRA_ARGS=()

for arg in "$@"; do
    case "$arg" in
        --configure) DO_CONFIGURE=1 ;;
        --run)       DO_RUN=1 ;;
        --jobs)      shift; JOBS="$1" ;;
        *)           EXTRA_ARGS+=("$arg") ;;
    esac
done

# ── Step 1: Configure (first time or --configure) ───────────────────────────
if [ ! -f "$BUILD/Makefile" ] || [ "$DO_CONFIGURE" -eq 1 ]; then
    echo "==> Configuring..."
    cmake -B build -DCMAKE_BUILD_TYPE=Debug
    # cmake_check_build_system runs juceaide test and fails. Patch it out:
    sed -i 's/message(FATAL_ERROR "Testing juceaide failed/message(STATUS "juceaide test skipped (NTFS):/' \
        "$ROOT/JUCE/extras/Build/juceaide/CMakeLists.txt" 2>/dev/null || true
fi

# ── Step 2: Copy juceaide to /tmp ────────────────────────────────────────────
if [ -f "$NTFS_AIDE" ]; then
    echo "==> Copying juceaide to $TMP_AIDE ..."
    cp "$NTFS_AIDE" "$TMP_AIDE"
    chmod +x "$TMP_AIDE"
    echo "    juceaide version: $("$TMP_AIDE" version 2>/dev/null || echo 'unknown')"
else
    if [ ! -f "$TMP_AIDE" ]; then
        echo "ERROR: juceaide not found at $NTFS_AIDE or $TMP_AIDE"
        echo "Run 'bash build.sh --configure' first."
        exit 1
    fi
    echo "==> Using cached $TMP_AIDE"
fi

# ── Step 3: Patch build files to use /tmp juceaide ───────────────────────────
ESCAPED_NTFS=$(echo "$NTFS_AIDE" | sed 's|/|\\/|g')
ESCAPED_TMP=$(echo "$TMP_AIDE" | sed 's|/|\\/|g')

echo "==> Patching build files ..."
find "$BUILD" -name "*.make" -o -name "JUCEToolsExport.cmake" | \
    xargs -I{} sed -i "s|$NTFS_AIDE|$TMP_AIDE|g" {} 2>/dev/null || true

# ── Step 4: Build via Makefile2 (skips cmake_check_build_system) ─────────────
echo "==> Building with $JOBS jobs ..."
make -C "$BUILD" -f CMakeFiles/Makefile2 all -j"$JOBS" "${EXTRA_ARGS[@]}"

echo ""
echo "Build complete."
BINARY="$BUILD/Nomad2026_artefacts/Debug/Nomad2026"
echo "Binary: $BINARY"

# ── Step 5: Run ───────────────────────────────────────────────────────────────
if [ "$DO_RUN" -eq 1 ]; then
    if [ -x "$BINARY" ]; then
        echo "==> Launching..."
        "$BINARY"
    else
        # NTFS: can't exec from here, copy to /tmp first
        TMP_BIN="/tmp/Nomad2026"
        echo "==> Copying binary to $TMP_BIN (NTFS exec workaround)..."
        cp "$BINARY" "$TMP_BIN"
        chmod +x "$TMP_BIN"
        echo "==> Launching..."
        "$TMP_BIN"
    fi
fi
