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
            )
            image_data = response.data[0]
            if image_data.b64_json:
                return base64.b64decode(image_data.b64_json)
            elif image_data.url:
                import httpx

                resp = httpx.get(image_data.url)
                resp.raise_for_status()
                return resp.content
            return None
        except Exception as exc:
            logger.warning("OpenAIImageAdapter: generation failed: %s", exc)
            return None


@dataclass(frozen=True)
class LocalDiffusersImageAdapter:
    """Generates a single master image locally via a diffusers pipeline (e.g. SDXL-Turbo).

    The heavy ``torch``/``diffusers`` stack is imported lazily and only when a
    pipeline must be built, so importing this module never requires the
    optional ``[image-local]`` dependencies. Inject ``pipeline`` in tests to
    avoid loading a real model.
    """

    settings: ImageGenSettings
    pipeline: object = field(default=None, compare=False)
    num_inference_steps: int = 2
    guidance_scale: float = 0.0

    def generate(self, prompt: str) -> bytes | None:
        if not self.settings.model:
            return None
        logger.info(
            "LocalDiffusersImageAdapter: generating master image (model=%s)",
            self.settings.model,
        )
        try:
            pipeline = self.pipeline or _create_diffusers_pipeline(self.settings)
            if pipeline is None:
                return None
            result = pipeline(
                prompt=prompt,
                num_inference_steps=self.num_inference_steps,
                guidance_scale=self.guidance_scale,
                width=MASTER.width,
                height=MASTER.height,
            )
            image = result.images[0]
            buf = io.BytesIO()
            image.save(buf, format=MASTER.format)
            return buf.getvalue()
        except Exception as exc:
            logger.warning("LocalDiffusersImageAdapter: generation failed: %s", exc)
            return None


def create_image_adapter(settings: ImageGenSettings) -> ImageGenerationAdapter:
    """Create an image generation adapter for the configured provider."""
    if settings.provider == "none":
        return PlaceholderImageAdapter()
    if settings.provider == "openai":
        return OpenAIImageAdapter(settings=settings)
    if settings.provider == "local":
        return LocalDiffusersImageAdapter(settings=settings)
    return PlaceholderImageAdapter()


def _autodetect_device() -> str:
    """Pick the best available torch device, preferring GPU/MPS over CPU."""
    try:
        import torch

        if torch.cuda.is_available():
            return "cuda"
        if torch.backends.mps.is_available():
            return "mps"
    except Exception:  # pragma: no cover - torch import/feature probing
        pass
    return "cpu"


def _create_diffusers_pipeline(settings: ImageGenSettings) -> object | None:
    if not settings.model:
        return None
    import torch
    from diffusers import AutoPipelineForText2Image

    device = settings.device or _autodetect_device()
    dtype = torch.float16 if device in {"cuda", "mps"} else torch.float32
    pipeline = AutoPipelineForText2Image.from_pretrained(
        settings.model,
        torch_dtype=dtype,
    )
    return pipeline.to(device)


def resize_to_profile(master_bytes: bytes, profile: ImageProfile) -> bytes:
    """Downscale a master image to the given profile's dimensions and format."""
    img = Image.open(io.BytesIO(master_bytes))
    if img.size == (profile.width, profile.height) and img.format == profile.format:
        return master_bytes
    resized = img.resize(
        (profile.width, profile.height),
        Image.Resampling.LANCZOS,
    )
    buf = io.BytesIO()
    resized.save(buf, format=profile.format)
    return buf.getvalue()


def _create_openai_client(settings: ImageGenSettings) -> object | None:
    if not settings.api_key:
        return None
    from openai import OpenAI

    return OpenAI(api_key=settings.api_key)
