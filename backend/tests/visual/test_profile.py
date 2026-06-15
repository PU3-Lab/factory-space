"""Unit tests for ImageProfile constants."""

from __future__ import annotations

import pytest

from visual.profile import ICON, TEXTURE, THUMBNAIL, ImageProfile


def test_image_profile_is_frozen_dataclass() -> None:
    profile = ImageProfile(name="test", width=64, height=64, format="PNG")
    with pytest.raises(AttributeError):
        profile.width = 128  # type: ignore[misc]


def test_icon_profile_dimensions() -> None:
    assert ICON.name == "icon"
    assert ICON.width == 512
    assert ICON.height == 512
    assert ICON.format == "PNG"


def test_texture_profile_dimensions() -> None:
    assert TEXTURE.name == "texture"
    assert TEXTURE.width == 1024
    assert TEXTURE.height == 1024
    assert TEXTURE.format == "PNG"


def test_thumbnail_profile_dimensions() -> None:
    assert THUMBNAIL.name == "thumbnail"
    assert THUMBNAIL.width == 128
    assert THUMBNAIL.height == 128
    assert THUMBNAIL.format == "PNG"
