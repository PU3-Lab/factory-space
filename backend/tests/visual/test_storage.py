"""Unit tests for ImageStorageAdapter implementations."""

from __future__ import annotations

import pytest

from visual.storage import LocalFileStorageAdapter


def test_local_file_storage_saves_bytes(tmp_path: pytest.TempPathFactory) -> None:
    adapter = LocalFileStorageAdapter(base_path=str(tmp_path))
    adapter.save("materials/abc123/icon.png", b"fake-image-bytes")
    saved = (tmp_path / "materials" / "abc123" / "icon.png").read_bytes()
    assert saved == b"fake-image-bytes"


def test_local_file_storage_creates_parent_dirs(
    tmp_path: pytest.TempPathFactory,
) -> None:
    adapter = LocalFileStorageAdapter(base_path=str(tmp_path))
    adapter.save("deep/nested/path/file.png", b"data")
    assert (tmp_path / "deep" / "nested" / "path" / "file.png").exists()


def test_local_file_storage_overwrites_existing(
    tmp_path: pytest.TempPathFactory,
) -> None:
    adapter = LocalFileStorageAdapter(base_path=str(tmp_path))
    adapter.save("materials/x/icon.png", b"first")
    adapter.save("materials/x/icon.png", b"second")
    assert (tmp_path / "materials" / "x" / "icon.png").read_bytes() == b"second"
