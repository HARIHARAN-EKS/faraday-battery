"""Generate Faraday brand assets from logo/Logo.png.

The source is a 1254x1254 RGB artwork on a solid dark field with NO alpha
channel. Its subject (the battery body) is itself near-black, so the
background cannot be keyed out without hollowing the subject. Instead the
artwork is presented as a rounded dark "app tile": transparent outside the
tile, artwork preserved inside. That reads correctly on both light and dark
backgrounds, which the sidebar (both themes) and the Windows taskbar need.

Outputs (all with alpha):
  resources/faraday.ico       launcher + installer + window icon (16..256)
  resources/faraday_core.ico  muted variant for the internal core exe
  resources/faraday_uninst.ico  uninstaller icon (muted, same as core)
  resources/faraday.png       256px in-app logo (sidebar header)
"""

import os

from PIL import Image, ImageDraw, ImageEnhance

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "logo", "Logo.png")
RES = os.path.join(ROOT, "resources")
SIZES = [16, 24, 32, 48, 64, 128, 256]


def tile(src: Image.Image, size: int, muted: bool = False) -> Image.Image:
    """Square rounded tile at `size`, alpha outside the rounded rect."""
    # Supersample 4x for clean edges at small sizes.
    ss = size * 4
    art = src.convert("RGB").resize((ss, ss), Image.LANCZOS)

    if muted:
        # The core exe is an internal component: desaturate and dim so it is
        # visually subordinate to the launcher in Explorer.
        art = ImageEnhance.Color(art).enhance(0.25)
        art = ImageEnhance.Brightness(art).enhance(0.65)

    art = art.convert("RGBA")
    mask = Image.new("L", (ss, ss), 0)
    radius = int(ss * 0.18)  # standard Windows-ish app-tile rounding
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, ss - 1, ss - 1], radius=radius, fill=255)
    art.putalpha(mask)
    return art.resize((size, size), Image.LANCZOS)


def build_ico(path: str, muted: bool = False) -> None:
    src = Image.open(SRC)
    frames = [tile(src, s, muted) for s in SIZES]
    # Pillow writes a real multi-resolution ICO from the largest frame.
    frames[-1].save(path, format="ICO", sizes=[(s, s) for s in SIZES])
    print(f"wrote {os.path.relpath(path, ROOT)} ({'muted' if muted else 'brand'}) "
          f"sizes={SIZES}")


def main() -> None:
    src = Image.open(SRC)
    if src.size[0] < 256:
        raise SystemExit(f"source too small for a 256px icon: {src.size}")

    build_ico(os.path.join(RES, "faraday.ico"), muted=False)
    build_ico(os.path.join(RES, "faraday_core.ico"), muted=True)
    build_ico(os.path.join(RES, "faraday_uninst.ico"), muted=True)

    png = os.path.join(RES, "faraday.png")
    tile(src, 256).save(png, format="PNG")
    print(f"wrote {os.path.relpath(png, ROOT)} (256px, alpha)")


if __name__ == "__main__":
    main()
