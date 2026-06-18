"""Unit tests for ImageGenerationAdapter implementations."""

from __future__ import annotations

import base64
import io
from unittest.mock import MagicMock, patch

from PIL import Image

from visual.adapter import (
    LocalDiffusersImageAdapter,
    OpenAIImageAdapter,
    PlaceholderImageAdapter,
    create_image_adapter,
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

    def test_resize_to_identical_profile_returns_original_bytes(self) -> None:
        master = _png_bytes(MASTER.width, MASTER.height)
        result = resize_to_profile(master, MASTER)
        assert result is master


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
        mock_image_data.url = None
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

    def test_returns_master_png_on_success_via_url(self) -> None:
        raw_png = _png_bytes(1024, 1024)
        mock_image_data = MagicMock()
        mock_image_data.b64_json = None
        mock_image_data.url = "https://fake-url.com/image.png"
        mock_response = MagicMock()
        mock_response.data = [mock_image_data]

        mock_client = MagicMock()
        mock_client.images.generate.return_value = mock_response

        adapter = OpenAIImageAdapter(settings=_settings_openai(), client=mock_client)

        with patch("httpx.get") as mock_get:
            mock_resp = MagicMock()
            mock_resp.content = raw_png
            mock_get.return_value = mock_resp

            result = adapter.generate("a glowing crystal")

            assert result == raw_png
            mock_get.assert_called_once_with("https://fake-url.com/image.png")

    def test_returns_none_on_api_exception(self) -> None:
        mock_client = MagicMock()
        mock_client.images.generate.side_effect = RuntimeError("API down")

        adapter = OpenAIImageAdapter(settings=_settings_openai(), client=mock_client)
        assert adapter.generate("prompt") is None


def _settings_local(model: str = "stabilityai/sdxl-turbo") -> ImageGenSettings:
    return ImageGenSettings(provider="local", model=model)


def _fake_pipeline_returning(image: Image.Image) -> MagicMock:
    """Mimic a diffusers pipeline: callable returning an object with .images."""
    pipe = MagicMock()
    result = MagicMock()
    result.images = [image]
    pipe.return_value = result
    return pipe


class TestLocalDiffusersImageAdapter:
    def test_returns_none_when_model_missing(self) -> None:
        settings = ImageGenSettings(provider="local", model=None)
        adapter = LocalDiffusersImageAdapter(settings=settings)
        assert adapter.generate("prompt") is None

    def test_returns_master_png_on_success(self) -> None:
        master_image = Image.new("RGB", (MASTER.width, MASTER.height), (10, 20, 30))
        pipe = _fake_pipeline_returning(master_image)

        adapter = LocalDiffusersImageAdapter(settings=_settings_local(), pipeline=pipe)
        result = adapter.generate("a seamless metal texture")

        assert result is not None
        img = Image.open(io.BytesIO(result))
        assert img.format == "PNG"
        assert img.size == (MASTER.width, MASTER.height)
        pipe.assert_called_once()

    def test_returns_none_on_pipeline_exception(self) -> None:
        pipe = MagicMock(side_effect=RuntimeError("CUDA OOM"))
        adapter = LocalDiffusersImageAdapter(settings=_settings_local(), pipeline=pipe)
        assert adapter.generate("prompt") is None


class TestCreateImageAdapter:
    def test_none_provider_returns_placeholder(self) -> None:
        adapter = create_image_adapter(ImageGenSettings(provider="none"))
        assert isinstance(adapter, PlaceholderImageAdapter)

    def test_openai_provider_returns_openai_adapter(self) -> None:
        adapter = create_image_adapter(_settings_openai())
        assert isinstance(adapter, OpenAIImageAdapter)

    def test_local_provider_returns_local_adapter(self) -> None:
        adapter = create_image_adapter(_settings_local())
        assert isinstance(adapter, LocalDiffusersImageAdapter)
