"""Image generation provider settings."""

from __future__ import annotations

import os
from collections.abc import Mapping
from dataclasses import dataclass
from typing import Literal

ImageGenProvider = Literal["none", "openai"]

_PROVIDERS: frozenset[str] = frozenset({"none", "openai"})


@dataclass(frozen=True)
class ImageGenSettings:
    """Settings for one image generation provider slot."""

    provider: ImageGenProvider
    model: str | None = None
    api_key: str | None = None

    @classmethod
    def from_env(cls, env: Mapping[str, str] | None = None) -> ImageGenSettings:
        source = env if env is not None else os.environ
        provider_raw = _string_from_env(source, "FACTORY_IMAGE_GEN_PROVIDER") or "none"
        if provider_raw not in _PROVIDERS:
            raise ValueError(f"Unsupported image generation provider: {provider_raw!r}")
        provider: ImageGenProvider = provider_raw  # type: ignore[assignment]

        if provider == "none":
            return cls(provider=provider)

        model = _string_from_env(source, "FACTORY_IMAGE_GEN_MODEL")
        if model is None:
            raise ValueError("Provider requires FACTORY_IMAGE_GEN_MODEL")

        slot_key = _string_from_env(source, "FACTORY_IMAGE_GEN_API_KEY")
        api_key = slot_key or _string_from_env(source, "OPENAI_API_KEY")

        return cls(provider=provider, model=model, api_key=api_key)


def _string_from_env(env: Mapping[str, str], key: str) -> str | None:
    value = env.get(key)
    if value is None:
        return None
    stripped = value.strip()
    return stripped or None
