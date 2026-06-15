"""Image asset storage adapters."""

from __future__ import annotations

import logging
from dataclasses import dataclass
from pathlib import Path
from typing import Protocol

logger = logging.getLogger(__name__)


class ImageStorageAdapter(Protocol):
    """Common contract for persisting image bytes to a storage backend."""

    def save(self, key: str, data: bytes) -> None:
        """Write image bytes under the given key."""


@dataclass(frozen=True)
class LocalFileStorageAdapter:
    """Stores image files on the local filesystem under base_path/key."""

    base_path: str

    def save(self, key: str, data: bytes) -> None:
        path = Path(self.base_path) / key
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        logger.debug("Saved image asset: %s", path)


@dataclass(frozen=True)
class NoopStorageAdapter:
    """Discards all image data — used in tests and when storage is disabled."""

    def save(self, key: str, data: bytes) -> None:
        logger.debug(
            "NoopStorageAdapter: discarded %d bytes for key=%s", len(data), key
        )
