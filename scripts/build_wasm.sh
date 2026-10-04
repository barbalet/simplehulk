#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
WASM_CC=${WASM_CC:-clang}
if [ -z "${WASM_LD:-}" ]; then
    if command -v wasm-ld >/dev/null 2>&1; then WASM_LD=$(command -v wasm-ld)
    elif command -v rustc >/dev/null 2>&1; then
        WASM_LD="$(rustc --print sysroot)/lib/rustlib/$(rustc -vV | sed -n 's/^host: //p')/bin/gcc-ld/wasm-ld"
    else
        echo 'Set WASM_LD to wasm-ld (LLVM or a Rust toolchain).' >&2; exit 1
    fi
    if [ ! -x "$WASM_LD" ]; then
        for candidate in "$HOME"/.rustup/toolchains/*/lib/rustlib/*/bin/gcc-ld/wasm-ld; do
            if [ -x "$candidate" ]; then WASM_LD=$candidate; break; fi
        done
    fi
fi
mkdir -p build/wasm
for unit in rules scenarios; do
    "$WASM_CC" --target=wasm32 -std=c99 -Oz -ffreestanding -fno-builtin -Wall -Wextra -Werror -Isrc/wasm/include -Isrc/c-core -c "src/c-core/$unit.c" -o "build/wasm/$unit.o"
done
for unit in runtime bridge; do
    "$WASM_CC" --target=wasm32 -std=c99 -Oz -ffreestanding -fno-builtin -Wall -Wextra -Werror -Isrc/wasm/include -Isrc/c-core -c "src/wasm/$unit.c" -o "build/wasm/$unit.o"
done
"$WASM_LD" --no-entry --export-memory --initial-memory=262144 --max-memory=262144 -z stack-size=65536 build/wasm/rules.o build/wasm/scenarios.o build/wasm/runtime.o build/wasm/bridge.o -o web/simple-hulk.wasm
chmod 644 web/simple-hulk.wasm
echo 'Built web/simple-hulk.wasm'
