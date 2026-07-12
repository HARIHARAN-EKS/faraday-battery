"""Generate Faraday brand assets from logo/Logo.png.

Two problems this script solves, both standard icon practice:

1. The source (1254x1254 RGB, no alpha) is a dark-field *illustration*: a
   glossy battery with a thin glowing orbit. Downscaled to 16/24/32 px it
   loses its silhouette and turns to mush -- the battery becomes a sliver
   and the orbit becomes noise. So the small sizes are NOT downscales of
   the artwork: they are a purpose-drawn SIMPLIFIED MARK with the same
   identity (dark rounded tile, bold battery, green charge, cyan arc) but
   fewer details, a stronger silhouette and higher contrast. The full
   artwork is kept at 48/64/128/256, where it reads properly.

2. The artwork has no alpha and its subject is near-black, so the black
   background cannot be keyed out without hollowing the subject. Both the
   artwork and the simplified mark are therefore presented as a rounded
   dark "app tile" (transparent outside the tile), which reads correctly
   on light and dark shells alike.

Outputs (all with alpha):
  resources/faraday.ico        launcher + installer + window icon (16..256)
  resources/faraday_core.ico   muted variant for the internal core exe
  resources/faraday_uninst.ico uninstaller icon (muted)
  resources/faraday_mark.png   256px SIMPLIFIED mark for the in-app sidebar
                               (renders crisply when scaled to ~32px)
  resources/faraday.png        256px full artwork tile (large in-app use)
"""

import os

from PIL import Image, ImageDraw, ImageEnhance

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "logo", "Logo.png")
RES = os.path.join(ROOT, "resources")

ICO_SIZES = [16, 24, 32, 48, 64, 128, 256]
SIMPLIFY_AT_OR_BELOW = 32  # sizes that get the simplified mark

TILE_BG = (11, 14, 18, 255)      # near-black, matches the artwork's field
BODY = (228, 236, 244, 255)      # battery shell
CHARGE_TOP = (126, 231, 135, 255)
CHARGE_BOTTOM = (63, 185, 80, 255)
ORBIT = (86, 224, 231, 255)      # cyan


def rounded_tile_mask(size: int) -> Image.Image:
    mask = Image.new("L", (size, size), 0)
    radius = int(size * 0.18)  # standard Windows-ish app-tile rounding
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, size - 1, size - 1],
                                           radius=radius, fill=255)
    return mask


def simplified_mark(size: int, with_orbit: bool = True) -> Image.Image:
    """Purpose-drawn mark: bold silhouette that survives 16px.

    Drawn at 8x and downsampled, so the edges stay clean at any size.
    """
    ss = size * 8
    img = Image.new("RGBA", (ss, ss), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    # Tile
    d.rounded_rectangle([0, 0, ss - 1, ss - 1], radius=int(ss * 0.18), fill=TILE_BG)

    bw = ss * 0.42            # body width
    bh = ss * 0.62            # body height
    bx = (ss - bw) / 2
    by = (ss - bh) / 2 + ss * 0.035
    stroke = max(2, int(ss * 0.045))

    # Orbit FIRST, so the battery occludes its middle and the visible lobes
    # read as an ellipse passing behind the cell -- the artwork's motif,
    # rather than a handle sitting on top of it.
    if with_orbit:
        ow = max(3, int(ss * 0.05))
        d.ellipse([ss * 0.05, ss * 0.40, ss * 0.95, ss * 0.76],
                  outline=ORBIT, width=ow)

    # Battery body: deliberately chunky so it reads at 16px. Its interior is
    # filled with the tile color so the orbit does not show through it.
    d.rounded_rectangle([bx, by, bx + bw, by + bh],
                        radius=int(ss * 0.06), fill=TILE_BG,
                        outline=BODY, width=stroke)

    # Terminal nub on top
    nw = bw * 0.36
    nh = ss * 0.055
    d.rounded_rectangle([(ss - nw) / 2, by - nh, (ss + nw) / 2, by + stroke * 0.5],
                        radius=int(nh * 0.4), fill=BODY)

    # Charge fill: a solid block (no gradient banding at small sizes),
    # filling ~70% from the bottom.
    pad = stroke * 1.6
    fill_h = (bh - pad * 2) * 0.70
    fx0, fx1 = bx + pad, bx + bw - pad
    fy1 = by + bh - pad
    fy0 = fy1 - fill_h
    for i in range(int(fy0), int(fy1)):
        t = (i - fy0) / max(1.0, fill_h)
        c = tuple(int(CHARGE_TOP[k] + (CHARGE_BOTTOM[k] - CHARGE_TOP[k]) * t)
                  for k in range(3)) + (255,)
        d.line([(fx0, i), (fx1, i)], fill=c, width=1)

    # The short front-crossing segment of the orbit, drawn over the cell so
    # the ellipse reads as a ring around it rather than a ring behind it.
    if with_orbit:
        ow = max(3, int(ss * 0.05))
        d.arc([ss * 0.05, ss * 0.40, ss * 0.95, ss * 0.76],
              start=35, end=145, fill=ORBIT, width=ow)

    img.putalpha(rounded_tile_mask(ss))
    return img.resize((size, size), Image.LANCZOS)


def artwork_tile(size: int) -> Image.Image:
    """The full artwork, as a rounded tile (used at 48px and above)."""
    src = Image.open(SRC).convert("RGB")
    ss = size * 4
    art = src.resize((ss, ss), Image.LANCZOS).convert("RGBA")
    art.putalpha(rounded_tile_mask(ss))
    return art.resize((size, size), Image.LANCZOS)


def mute(img: Image.Image) -> Image.Image:
    """Desaturate + dim: the internal core exe must read as subordinate."""
    rgb = img.convert("RGB")
    rgb = ImageEnhance.Color(rgb).enhance(0.25)
    rgb = ImageEnhance.Brightness(rgb).enhance(0.65)
    out = rgb.convert("RGBA")
    out.putalpha(img.getchannel("A"))
    return out


def frame(size: int, muted: bool) -> Image.Image:
    if size <= SIMPLIFY_AT_OR_BELOW:
        # 16px has no room for the orbit; 24/32 keep one bold arc.
        img = simplified_mark(size, with_orbit=(size >= 24))
    else:
        img = artwork_tile(size)
    return mute(img) if muted else img


def build_ico(path: str, muted: bool = False) -> None:
    frames = [frame(s, muted) for s in ICO_SIZES]
    # Pillow writes a true multi-resolution ICO; passing the explicit frame
    # list means each size is OUR render, not a rescale of the largest.
    frames[-1].save(path, format="ICO",
                    sizes=[(s, s) for s in ICO_SIZES],
                    append_images=frames[:-1])
    kind = "muted" if muted else "brand"
    print(f"wrote {os.path.relpath(path, ROOT)} ({kind}) sizes={ICO_SIZES} "
          f"(<= {SIMPLIFY_AT_OR_BELOW}px simplified)")


def main() -> None:
    src = Image.open(SRC)
    if src.size[0] < 256:
        raise SystemExit(f"source too small for a 256px icon: {src.size}")

    build_ico(os.path.join(RES, "faraday.ico"), muted=False)
    build_ico(os.path.join(RES, "faraday_core.ico"), muted=True)
    build_ico(os.path.join(RES, "faraday_uninst.ico"), muted=True)

    # In-app sidebar mark: the SIMPLIFIED design at high resolution, so Qt
    # can downsample it cleanly to ~32px on any display scale.
    mark = simplified_mark(256, with_orbit=True)
    mark.save(os.path.join(RES, "faraday_mark.png"), format="PNG")
    print("wrote resources/faraday_mark.png (256px simplified mark, alpha)")

    # Full artwork tile, kept for large in-app use.
    artwork_tile(256).save(os.path.join(RES, "faraday.png"), format="PNG")
    print("wrote resources/faraday.png (256px artwork tile, alpha)")


if __name__ == "__main__":
    main()
