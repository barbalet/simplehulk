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

Choose a scenario and rules version from the single Play dropdown. It groups
all nine missions and preserves every optional bug / house-rule combination.
Seed, animation and private-view overrides live in Preferences. Marine view is the default; switch to the private Alien view only after
passing the device. Current team view is an optional phase-following preference.

Click a friendly piece to activate it, then click it again for its contextual
action dropdown. Choose overwatch, turning, attacks, grenades, doors, mission
interactions or equipment there. Targeted actions prompt for a map square or
piece; Escape cancels. Dropped items appear by name in the pickup choices.
End activation is in the same menu; End phase is above the board. During
reinforcement deployment click entry letters on the map. During a reaction
window click an overwatching Marine to fire or decline, then Resume alien
attack. The engine rejects illegal actions without spending resources.

Adjacent empty floor squares are outlined for the active model. Click one or
drag the active piece one cell to move. Dragging previews the destination; it
never chains moves or bypasses AP, facing restrictions or overwatch. Drag other
board space to pan. Pieces start at 64 pixels per cell so the viewport shows a
useful portion of the map. Wheel / pinch and + / − zoom around the cursor or
viewport center; Overview fits the whole map and Find active unit recenters it.
Arrow keys select cells; Enter selects / moves and M opens piece actions.

Marine view uses the core's current shared field of view: each living onboard
Marine sees the forward half-plane through clear lines of sight. Doors, walls,
exterior and intervening models block sight. Charted hull outlines remain visible;
unseen interiors are dark, without fixtures, items, corpses or weapon effects.
There is no permanent explored-area memory. Visible bugs use model art; unseen
living bugs use the existing grayscale blip sprite with hidden profile and stats.
Turning away or closing a door immediately restores the blip presentation without
undoing a contact's actual reveal or changing its combat rules. The WASM snapshot
provides the visibility mask, masked map and observed entities; JS does not invent
its own visibility rules.

Marine view hides contact strength/profile. Alien view reveals private data;
this local hotseat convenience is not an access-control boundary. Restarting
creates a new game; Preferences → Download log saves engine feedback.

The C maps now use the books' hull outlines: `+`, `|`, `-` are walls, blank is
exterior, `=` is a closed door, and `/` is open. The renderer connects wall
neighbors with the new sixteen-join pencil atlas, leaves exterior gaps dark,
and animates only real doors. `tiles.js` defines the renderer's floor/wall
classification and neighbor masks; tests exercise all sixteen combinations.

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
  victories. The UI regression test covers contextual actions, board movement, deployment,
  reactions and zoom without a browser.

Build references: [Clang freestanding builds](https://clang.llvm.org/docs/UsersManual.html#freestanding-builds)
and [LLVM WebAssembly linker](https://lld.llvm.org/WebAssembly.html).

## Touch controls

Select a Marine with the named roster buttons or its map icon. Common actions use large buttons below the board; facing and equipment are expandable. Tap or drag to a floor destination for a route through currently exposed, unoccupied floor. Each step uses the engine and spends AP; doors need a separate action and pending reactions stop the route. Direction buttons provide precise single-square movement. Cancel target selection returns to the action panel. Infected crew is currently a tabletop option in FLUFF, not an engine variant.
