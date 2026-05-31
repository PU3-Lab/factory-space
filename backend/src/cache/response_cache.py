"""In-memory agent response cache."""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any


@dataclass
class ResponseCache:
    """Small in-memory response cache used by the local graph setup."""

    _items: dict[str, dict[str, Any]] = field(default_factory=dict)

    def get(self, key: str) -> dict[str, Any] | None:
        """Return a cached payload if one exists."""

        return self._items.get(key)

    def set(self, key: str, payload: dict[str, Any]) -> None:
        """Store a response payload."""

        self._items[key] = payload
