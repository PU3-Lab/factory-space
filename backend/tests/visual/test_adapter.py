"""Unit tests for ImageGenerationAdapter implementations."""

from __future__ import annotations

import base64
import io
from unittest.mock import MagicMock

from PIL import Image

from visual.adapter import (
    OpenAIImageAdapter,
    PlaceholderImageAdapter,
    resize_to_profile,
)
from visual.profile import ICON, MASTER, THUMBNAIL
from visual.settings import ImageGenSettings


def _settings_openai(
    model: str = "dall-e-3", api_key: str = "sk-test"
) -> ImageGenSettings:
    return ImageGenSettings(provider="openai", model=model, api_key=api_key)


def _png_bytes(width: int, height: int) -> bytes:
    img = Image.new("RGB", (width, height), (255, 0, 0))
    buf = io.BytesIO()
    img.save(buf, format="PNG")
    return buf.getvalue()


class TestPlaceholderImageAdapter:
    def test_returns_master_sized_png(self) -> None:
        adapter = PlaceholderImageAdapter()
        result = adapter.generate("a shiny metal ingot")
        assert result is not None
        img = Image.open(io.BytesIO(result))
        assert img.format == "PNG"
        assert img.size == (MASTER.width, MASTER.height)

    def test_custom_color(self) -> None:
        adapter = PlaceholderImageAdapter(color=(200, 100, 50))
        result = adapter.generate("prompt")
        assert result is not None
        img = Image.open(io.BytesIO(result)).convert("RGB")
        assert img.getpixel((0, 0)) == (200, 100, 50)


class TestResizeToProfile:
    def test_resizes_master_to_icon_dimensions(self) -> None:
        master = _png_bytes(MASTER.width, MASTER.height)
        result = resize_to_profile(master, ICON)
        img = Image.open(io.BytesIO(result))
        assert img.size == (ICON.width, ICON.height)

    def test_resizes_master_to_thumbnail_dimensions(self) -> None:
        master = _png_bytes(MASTER.width, MASTER.height)
        result = resize_to_profile(master, THUMBNAIL)
        img = Image.open(io.BytesIO(result))
        assert img.size == (THUMBNAIL.width, THUMBNAIL.height)


class TestOpenAIImageAdapter:
    def test_returns_none_when_api_key_missing(self) -> None:
        settings = ImageGenSettings(provider="openai", model="dall-e-3", api_key=None)
        adapter = OpenAIImageAdapter(settings=settings)
        assert adapter.generate("prompt") is None

    def test_returns_none_when_model_missing(self) -> None:
        settings = ImageGenSettings(provider="openai", model=None, api_key="sk-test")
        adapter = OpenAIImageAdapter(settings=settings)
        assert adapter.generate("prompt") is None

    def test_returns_master_png_on_success(self) -> None:
        # OpenAI returns the raw 1024x1024 master image; no resizing in the adapter.
        raw_png = _png_bytes(1024, 1024)
        b64_data = base64.b64encode(raw_png).decode()

        mock_image_data = MagicMock()
        mock_image_data.b64_json = b64_data
        mock_response = MagicMock()
        mock_response.data = [mock_image_data]

        mock_client = MagicMock()
        mock_client.images.generate.return_value = mock_response

        adapter = OpenAIImageAdapter(settings=_settings_openai(), client=mock_client)
        result = adapter.generate("a glowing crystal")

        assert result is not None
        img = Image.open(io.BytesIO(result))
        assert img.size == (1024, 1024)
        mock_client.images.generate.assert_called_once()

    def test_returns_none_on_api_exception(self) -> None:
        mock_client = MagicMock()
        mock_client.images.generate.side_effect = RuntimeError("API down")

        adapter = OpenAIImageAdapter(settings=_settings_openai(), client=mock_client)
        assert adapter.generate("prompt") is None
