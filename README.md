![Marines exploring the wreck](https://github.com/barbalet/simplehulk/raw/refs/heads/main/output/pdf/fluff-cover.png)

# Simple Hulk

A compact boarding game for two players: five Marines, hidden alien contacts,
and corridors held by overwatch.

- [Download page](output/pdf/index.html): illustrated landing page with both PDF downloads.
- [Rules](rules/RULES.md): setup, ASCII tiles, weapons, grenades, blips, and house rules.
- [Setting and examples](rules/FLUFF.md): crew, optional rules, a worked turn, and eight detailed scenarios.
- [Rules PDF](output/pdf/RULES.pdf): seven pages including the illustrated cover.
- [Setting PDF](output/pdf/FLUFF.pdf): 23 pages including the illustrated cover.

Start with **Wake the beacon** on rules page 6. You need two six-sided dice and
counters on a square grid. The starting map is 31 by 25 squares, with two relay objectives before the beacon.
The PDFs use US Letter pages and embedded fonts; both covers are black and white.

The PDF builder is `scripts/build_rules_pdf.py`; it uses ReportLab and the local
Codex bundled fonts. Run it with the bundled Python runtime to regenerate both
PDFs after editing the Markdown.

## C game engine

The C99 implementation is in [src/c-core](src/c-core/README.md), with an API,
interactive command-line front end, and all nine mission maps. The test runner
is in [src/c-test](src/c-test/README.md).

```sh
make test                       # Smoke test, rebuild, then run all nine games
./build/simple-hulk 0 2026       # Interactive game: mission ID and seed
./build/c-test --scenarios --verbose
```

Games emit movement, weapons, overwatch, and objective feedback. Full scenario
reports are written under `build/`. A test game completes with either team's
victory; the bot results do not establish mission balance.

Cover assets and their generation prompts are in `output/pdf/`.

## Browser game and artwork

[WebAssembly game](web/index.html) · [Game guide](web/README.md) · [Art inventory](assets/index.html)

The browser uses the C engine for all nine missions and provides local hotseat
play, a 32 × 32 display grid, walking/turning sprites, sliding doors, weapon and
grenade effects, and explicit overwatch reactions. The four grayscale pencil
atlases and their prompts are in [assets](assets/README.md).

```sh
make wasm-test                  # Build WASM, compare native behavior, audit art
make serve                      # Open http://localhost:8000/web/
```

The compiled WASM module is included. Rebuilding needs Clang and wasm-ld;
serving the existing files only needs an HTTP server.

![Marines holding a corridor against the bugs](https://github.com/barbalet/simplehulk/raw/refs/heads/main/output/pdf/rules-cover.png)
