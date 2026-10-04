# C build smoke tests and scenario runner

From the repository root:

```sh
make test
# Equivalent from any working directory:
/path/to/simplehulk/src/c-test/run.sh
```

`make test` first checks the current build with the smoke suite and launches the
interactive API front end with a status/quit script. It then forcibly rebuilds
the library, front end, and test executable and plays every current mission until
one team wins. Stalls, impossible internal states, assertion failures, build
failures, and report-write errors fail the run.

Other useful commands:

```sh
make smoke                  # Unit/fixture checks and CLI startup
make scenarios              # Run all nine missions on the current build
make rebuild-test           # Clean, build, smoke, rebuild, play
make sanitize               # Same tests under address/undefined-behavior checks
./build/c-test --scenario 7 --seed 123 --verbose
./build/c-test --scenarios --seed 100 --runs 20
```

`main.c` parses options and selects missions. `smoke.c` constructs focused legal
positions and checks movement costs, activation order, sight/corners, weapon
ammunition, jam cancellation, pre-attack overwatch, stun, reveals, item transfers,
reinforcement queues, all nine Marine victory paths, and deadline defeats.
White-box state edits are restricted to fixture construction in this file.
`runner.c` uses only the public API for full games. It never changes wounds, dice,
AP, maps, mission flags, or outcomes directly.

The Marine bot follows objective assignments, chooses combat targets, clears jams,
and holds corridors when it cannot advance. The alien bot approaches Marines,
opens doors, and attacks. These are small, deterministic policies with seeded
random game dice, not expert opponents or a proof of mission balance. A completed
game is **either** team's legal victory; passing does not mean the Marines won.
The fixture suite separately exercises every mission's successful objective path.

Every game writes `build/scenario-NN-seed-SEED.log`, with feedback for movement,
weapon use, overwatch windows and shots, jams, reveals, arrivals, mission actions,
and the ending. The console prints a summary of the result and event counts.
`--verbose` also streams those events to the console. Repeat seeds produce the
same reports. Run the executable from the repository root so `build/` exists.

Generated objects, executables, and game reports stay in ignored `build/`.
Sanitizer binaries use `build/sanitize/`, keeping the regular build separate.
