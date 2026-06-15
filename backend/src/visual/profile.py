"""Material visual asset size and format profiles."""

from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class ImageProfile:
    """Defines output dimensions and format for one image asset type."""

    name: str
    width: int
    height: int
    format: str  # PIL format string, e.g. "PNG"


ICON = ImageProfile(name="icon", width=512, height=512, format="PNG")
TEXTURE = ImageProfile(name="texture", width=1024, height=1024, format="PNG")
THUMBNAIL = ImageProfile(name="thumbnail", width=128, height=128, format="PNG")
