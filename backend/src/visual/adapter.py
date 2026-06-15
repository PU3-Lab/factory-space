"""Image generation provider adapters."""

from __future__ import annotations

import base64
import io
import logging
from dataclasses import dataclass, field
from typing import Protocol

from PIL import Image

from visual.profile import MASTER, ImageProfile
from visual.settings import ImageGenSettings

logger = logging.getLogger(__name__)


class ImageGenerationAdapter(Protocol):
    """Common contract for generating a single master image from a text prompt."""

    def generate(self, prompt: str) -> bytes | None:
        """Return master PNG bytes (large, ready to downscale), or None on failure."""


@dataclass(frozen=True)
class PlaceholderImageAdapter:
    """Creates a solid-color master PNG using Pillow — no API calls required."""

    color: tuple[int, int, int] = (128, 128, 128)

    def generate(self, prompt: str) -> bytes | None:
        img = Image.new("RGB", (MASTER.width, MASTER.height), self.color)
        buf = io.BytesIO()
        img.save(buf, format=MASTER.format)
        return buf.getvalue()


@dataclass(frozen=True)
class OpenAIImageAdapter:
    """Generates a single master image via OpenAI DALL-E 3 (no resizing here)."""

    settings: ImageGenSettings
    client: object = field(default=None, compare=False)

    def generate(self, prompt: str) -> bytes | None:
        if not self.settings.api_key:
            return None
        if not self.settings.model:
            return None
        logger.info(
            "OpenAIImageAdapter: generating master image (model=%s)",
            self.settings.model,
        )
        try:
            client = self.client or _create_openai_client(self.settings)
            if client is None:
                return None
            response = client.images.generate(
                model=self.settings.model,
                prompt=prompt,
                n=1,
                size="1024x1024",
                response_format="b64_json",
            )
            b64_data = response.data[0].b64_json
            return base64.b64decode(b64_data)
        except Exception as exc:
            logger.warning("OpenAIImageAdapter: generation failed: %s", exc)
            return None


def create_image_adapter(settings: ImageGenSettings) -> ImageGenerationAdapter:
    """Create an image generation adapter for the configured provider."""
    if settings.provider == "none":
        return PlaceholderImageAdapter()
    if settings.provider == "openai":
        return OpenAIImageAdapter(settings=settings)
    return PlaceholderImageAdapter()


def resize_to_profile(master_bytes: bytes, profile: ImageProfile) -> bytes:
    """Downscale a master image to the given profile's dimensions and format."""
    img = Image.open(io.BytesIO(master_bytes)).resize(
        (profile.width, profile.height),
        Image.Resampling.LANCZOS,
    )
    buf = io.BytesIO()
    img.save(buf, format=profile.format)
    return buf.getvalue()


def _create_openai_client(settings: ImageGenSettings) -> object | None:
    if not settings.api_key:
        return None
    from openai import OpenAI

    return OpenAI(api_key=settings.api_key)
