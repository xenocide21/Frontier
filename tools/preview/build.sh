#!/bin/bash
# Builds the preview renderer: the game's own sources + host.c. Usage: build.sh [outfile]
# Needs PSn00bSDK's headers: set PSN00BSDK_INCLUDE to its include/libpsn00b folder, or have
# the SDK's tools (elf2x) on your PATH and it is found from there.
cd "$(dirname "$0")"
SRC=../../src
SDK=${PSN00BSDK_INCLUDE:-$(dirname "$(dirname "$(command -v elf2x 2>/dev/null || echo /usr/local/bin/elf2x)")")/include/libpsn00b}
if [ ! -f "$SDK/psxgpu.h" ]; then echo "PSn00bSDK headers not found at $SDK (set PSN00BSDK_INCLUDE)"; exit 1; fi
LINKS=$(mktemp -d)                       # just the three SDK headers, so its libc headers don't shadow the host's
for h in psxgpu psxgte psxpad; do ln -s "$SDK/$h.h" "$LINKS/sdk_$h.h"; done
FILES=$(ls $SRC/*.c | grep -v "/input.c")
gcc -O1 -w -DFRONTIER_HOST -DFRONTIER_DEV -Iinc -I"$LINKS" -I$SRC -o ${1:-/tmp/frontier_host} $FILES host.c host_spu.c -lm 2>&1 | grep -E "error|undefined" | head -20
rm -rf "$LINKS"
