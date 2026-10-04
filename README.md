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

Cover assets and their generation prompts are in `output/pdf/`.

![Marines holding a corridor against the bugs](https://github.com/barbalet/simplehulk/raw/refs/heads/main/output/pdf/rules-cover.png)
