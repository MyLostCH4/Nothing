from __future__ import annotations

import sys
from pathlib import Path

from PIL import Image, ImageChops, ImageFilter


def emphasize_pupils(artwork: Image.Image) -> Image.Image:
    pupil_mask = Image.new("L", artwork.size, 0)
    pupil_pixels = pupil_mask.load()
    pixels = artwork.load()
    for y in range(artwork.height):
        for x in range(artwork.width):
            red, green, blue, alpha = pixels[x, y]
            if alpha > 80 and red < 100 and green > 130 and blue > 150:
                pupil_pixels[x, y] = alpha

    if pupil_mask.getbbox() is None:
        return artwork

    halo_mask = pupil_mask.filter(ImageFilter.MaxFilter(17))
    halo_ring = ImageChops.subtract(halo_mask, pupil_mask)

    halo_layer = Image.new("RGBA", artwork.size, (247, 241, 227, 0))
    halo_layer.putalpha(halo_ring)
    artwork = Image.alpha_composite(halo_layer, artwork)

    black_layer = Image.new("RGBA", artwork.size, (8, 8, 10, 0))
    black_layer.putalpha(pupil_mask)
    return Image.alpha_composite(artwork, black_layer)


def main() -> int:
    if len(sys.argv) != 3:
        print("Usage: generate_icon.py INPUT_IMAGE OUTPUT_DIRECTORY")
        return 2

    input_path = Path(sys.argv[1])
    output_directory = Path(sys.argv[2])
    output_directory.mkdir(parents=True, exist_ok=True)

    source_rgba = Image.open(input_path).convert("RGBA")
    alpha = source_rgba.getchannel("A")
    if alpha.getextrema()[0] < 255:
        bounds = alpha.getbbox()
        if bounds is None:
            raise RuntimeError("The source image does not contain visible artwork.")
        artwork = source_rgba.crop(bounds)
    else:
        source = source_rgba.convert("RGB")
        white = Image.new("RGB", source.size, "white")
        difference = ImageChops.difference(source, white).convert("L")
        mask = difference.point(lambda value: 255 if value > 8 else 0)
        bounds = mask.getbbox()
        if bounds is None:
            raise RuntimeError("The source image does not contain visible artwork.")

        artwork_rgb = source.crop(bounds)
        artwork = artwork_rgb.convert("RGBA")
        pixels = artwork.load()
        for y in range(artwork.height):
            for x in range(artwork.width):
                red, green, blue, _ = pixels[x, y]
                distance_from_white = max(255 - red, 255 - green, 255 - blue)
                alpha_value = 0 if distance_from_white < 12 else min(255, round(distance_from_white * 1.65))
                pixels[x, y] = (red, green, blue, alpha_value)

    artwork = emphasize_pupils(artwork)
    side = max(artwork.size)
    padding = max(4, round(side * 0.015))
    canvas_side = side + padding * 2

    canvas = Image.new("RGBA", (canvas_side, canvas_side), (255, 255, 255, 0))
    offset = ((canvas_side - artwork.width) // 2, (canvas_side - artwork.height) // 2)
    canvas.alpha_composite(artwork, offset)
    icon_512 = canvas.resize((512, 512), Image.Resampling.LANCZOS)

    png_path = output_directory / "app_icon.png"
    ico_path = output_directory / "app_icon.ico"
    icon_512.save(png_path, optimize=True)
    icon_512.save(
        ico_path,
        format="ICO",
        sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)],
    )

    print(f"PNG: {png_path}")
    print(f"ICO: {ico_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
