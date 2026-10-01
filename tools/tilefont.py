"""Draw the tiles' letters: capitals rasterized from DejaVu Serif Bold,
anti-aliased with one in-between tone (CHCrossword's tools/tilefont.py).

    python tools/tilefont.py [--size 12] [--half 80]

Writes tools/art/tilefont.txt, which tools/assets.py packs. Each glyph: a
line "= A", then its rows from the capitals' top: '#' ink, '+' half ink
(drawn in a tone between the letter's colour and the tile's), '.' clear.
The ink is the typeface's own hinted one-bit rendering, so stems stay crisp;
the half tones are where its smooth rendering covers at least --half of 255
of a pixel the one-bit one left clear. M and W come from DejaVu Serif
Condensed Bold, and W loses its first column, so that every letter is at
most 11 pixels: with its shadow it fits a 13-pixel tile face. Q's tail runs
below the baseline; J's hook is brought up to stand on it.
"""
import argparse
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
FONTS = ["C:/Windows/Fonts/", "/usr/share/fonts/truetype/dejavu/", "/Library/Fonts/"]


def find(name):
    path = next((d + name for d in FONTS if Path(d + name).exists()), None)
    if not path:
        raise SystemExit(f"{name} not found")
    return path


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--size", type=int, default=12)
    ap.add_argument("--half", type=int, default=80)
    a = ap.parse_args()
    regular = ImageFont.truetype(find("DejaVuSerif-Bold.ttf"), a.size)
    narrow = ImageFont.truetype(find("DejaVuSerifCondensed-Bold.ttf"), a.size)
    base = 20

    def ink(ch, mode):
        im = Image.new("L", (40, 40), 0)
        d = ImageDraw.Draw(im)
        d.fontmode = mode
        d.text((10, base), ch, font=narrow if ch in "MW" else regular, fill=255, anchor="ls")
        return im

    top = ink("H", "1").getbbox()[1]                 # the capitals' top row
    out = ["# The tiles' letters (tools/tilefont.py: DejaVu Serif Bold, size %d; M and W" % a.size,
           "# Condensed). '#' ink, '+' half ink. Rows from the capitals' top; the baseline is row %d." % (base - top), ""]
    for ch in "ABCDEFGHIJKLMNOPQRSTUVWXYZ":
        hard, soft = ink(ch, "1"), ink(ch, "L")
        x0, y0, x1, y1 = hard.getbbox()
        if ch == "W":
            x0 += 1
        rows = []
        for y in range(top, y1):
            rows.append("".join("#" if hard.getpixel((x, y)) else "+" if soft.getpixel((x, y)) >= a.half else "."
                                for x in range(x0, x1)))
        if ch == "J":                                # its hook brought up to the baseline: a tile has no room below
            rows = rows[:base - top - 2] + rows[-2:]
        out.append(f"= {ch}")
        out += rows
        out.append("")
    (HERE / "art" / "tilefont.txt").write_text("\n".join(out), newline="\n")
    print("tools/art/tilefont.txt written")


if __name__ == "__main__":
    main()
