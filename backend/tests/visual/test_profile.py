"""Unit tests for ImageProfile constants."""

from __future__ import annotations

import pytest

from visual.profile import ICON, MASTER, TEXTURE, THUMBNAIL, ImageProfile


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


def test_master_profile_is_largest() -> None:
    # MASTER is generated once, then downscaled to every other profile,
    # so it must be at least as large as the biggest derived profile.
    assert MASTER.name == "master"
    assert MASTER.width >= TEXTURE.width
    assert MASTER.height >= TEXTURE.height
    assert MASTER.format == "PNG"
