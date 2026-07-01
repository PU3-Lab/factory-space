"""Color extraction helpers for generated visual assets."""

from __future__ import annotations

import io

from PIL import Image, ImageStat


def unreal_rgba_from_average_rgb(image_bytes: bytes) -> str:
    """Return an Unreal-style RGBA string from the image's average RGB color."""
    image = Image.open(io.BytesIO(image_bytes)).convert("RGBA")
    mean_r, mean_g, mean_b, _ = ImageStat.Stat(image).mean
    return (
        f"(R={mean_r / 255:.2f},"
        f"G={mean_g / 255:.2f},"
        f"B={mean_b / 255:.2f},A=1.0)"
    )
