# WebAssembly boarding console

The browser runs the existing C rules and all nine scenarios through WebAssembly.
It provides local two-player hotseat play, manual alien activations, deployment,
and explicit overwatch responses. No automatic opponent or network play is included.

From the repository root:

```sh
make wasm
make wasm-test
make serve
```

Open `http://localhost:8000/web/`. The compiled `simple-hulk.wasm` is included, so
serving the checkout works without installing a compiler. Serve the repository
root: the game loads artwork from `../assets/`. Local-file HTML cannot reliably
perform the required fetches.

Rebuilding requires Clang with a wasm32 backend and LLVM's wasm-ld. The script
also discovers a Rust toolchain's bundled wasm-ld. Override paths with `WASM_CC`
and `WASM_LD`. Emscripten and external JS packages are unnecessary. Node runs
the tests; Python serves the files.

## Play

Choose mission, seed, optional bugs and a house rule, then Start mission.
Select a roster model to activate it. Click a square or model to choose the
destination/target, then execute an action. Coordinate controls provide the
same choices without precise map clicks. Turn with facing buttons. End an
activation before choosing another model. Interact collects mission equipment
and operates stations; Pick up handles dropped items.

End phase switches to deployment when arrivals are due. Deploy at A–F, then
finish deployment and play aliens. The overwatch panel pauses an alien action:
each eligible Marine may fire or decline once; Finish reactions resumes it.
Invalid actions report the engine's reason without changing the board.

Marine view hides contact strength/profile. Switch to Alien view after passing
the device. This is a local convenience, not an access-control boundary.
Reloading or restarting creates a new game; Download game log saves feedback.

Fit board shows the whole 32 × 32 display grid, including vacuum padding around
the 31 × 25 book map. Larger zoom levels scroll. Animation interpolates legal
cell movements and Marine facings; it never spends AP or resolves dice.

## Implementation and checks

- `src/wasm/bridge.c` exports start, command, snapshot, error and mission text.
- `src/wasm/runtime.c` supplies only the engine's required memory operations,
  one bounded game allocation, and %s/%d/%c formatting. Fixed memory is 256 KiB.
- The module's only import is synchronous typed feedback; no files, clock or
  network are imported. One game runs at a time; restart releases its arena.
- `engine.js` reads snapshots/feedback; `game.js` handles controls and drawing.
- `assets/manifest.json` owns all image paths, cells and feature mappings.
- `make wasm-test` compares every native/WASM snapshot and action result across
  all nine missions, deterministic event replay, legal endings and art coverage.
  These games exercise deadline endings. Native smoke fixtures verify objective
  victories. Browser checks cover movement, overwatch, views and the inventory.

Build references: [Clang freestanding builds](https://clang.llvm.org/docs/UsersManual.html#freestanding-builds)
and [LLVM WebAssembly linker](https://lld.llvm.org/WebAssembly.html).
