# Game assets

[Inventory](index.html) · [Manifest](manifest.json) · [Generation prompts](ARTWORK.md)

There are **92 named sprite cells across three PNG atlases**. These are the sole
source files for the browser's game artwork.

| Atlas | Layout | Contents |
| --- | --- | --- |
| hulk-atlas.png | 8 × 8 | Floor, hull, five sliding-door stages, breach, vacuum, supplemental crew, four bug profiles and walks, blip, casualties, weapons and grenades |
| mission-atlas.png | 4 × 4 | Every mission equipment category, acid impact, stun, overwatch and jam markers |
| marines-atlas.png | 4 × 3 | Overhead Marines: rifle, scattergun, cannon and flame projector; idle and two walking poses for each |

The current rules have four ranged weapons following the requested removal;
all four are covered. Both fragmentation and stun grenades have projectiles and
blast effects. Spitters have an acid projectile and impact; melee has a claw
effect. Every mission marker and item name maps to an asset. Letters/numbers
are drawn separately to preserve the books' map symbols.

## Grid and animation

- 32 × 32 display cells, each 32 × 32 logical pixels. The 31 × 25 rules map
  begins at display column 0, row 3. Padding is vacuum, never playable floor.
- Source cells are sampled proportionally from actual PNG dimensions, so
  source-image dimensions need not be multiples of 32.
- Marines rotate to the engine's facing. A white triangle clarifies facing at
  small zoom. Walking alternates left/right poses with the same weapon.
- Bugs rotate toward movement and alternate strides. The manifest records their
  source-art heading (south) so the renderer turns their heads into the move. Their heading is cosmetic;
  the C rules do not add an alien turning cost.
- Blips retain their contact sprite until revealed. Hidden strength/profile
  never changes their Marine-view appearance. Reveals place surviving siblings
  on legal neighboring cells.
- Doors play five sliding frames forward/backward. Breached doors show rubble
  during their transition. Hull seams have subtle animated lighting; cracks,
  corners and rubble are available for later map variants.
- Muzzle flashes, projectiles, impacts, area bursts, death and status markers
  share the grid. Area effects cover only the center and eligible orthogonal
  neighbors. Effects are cosmetic; engine feedback determines actual damage.
- Disable Animation, or use the system reduced-motion preference, to suppress
  movement, turning, wall lighting and effects.

`make wasm-test` audits required asset references and atlas bounds. The inventory
checks all images load and displays every cell, including supplemental art.
