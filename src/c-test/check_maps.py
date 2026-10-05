"""Fail if the C mission maps differ from the published ASCII maps."""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
book_rows = []
for name in ("RULES", "FLUFF"):
    text = (ROOT / "rules" / (name + ".md")).read_text()
    for block in re.findall(r"```text\n(.*?)\n```", text, re.S):
        lines = block.splitlines()
        if len(lines) != 27 or not lines[0].startswith("    000"):
            continue
        for number, line in enumerate(lines[2:], 1):
            match = re.fullmatch(r"\s*(\d+)(?:  (.*))?", line)
            assert match and int(match[1]) == number, (name, line)
            row = (match[2] or "").ljust(31)
            assert len(row) == 31, (name, number, row)
            book_rows.append(row)
core_rows = re.findall(r'"([ .A-Z0-9=|+/#-]{31})"',
                       (ROOT / "src/c-core/scenarios.c").read_text())
assert len(book_rows) == len(core_rows) == 225, "Expected nine 31 by 25 maps"
for index, (book, core) in enumerate(zip(book_rows, core_rows)):
    assert book == core, f"Mission {index // 25}, row {index % 25 + 1} differs"
print("Map sync: all 225 C rows match RULES and FLUFF, including exterior blanks.")
