"""Unit tests for ImageGenSettings."""

from __future__ import annotations

import pytest

from visual.settings import ImageGenSettings


def test_defaults_to_none_provider() -> None:
    settings = ImageGenSettings.from_env({})
    assert settings.provider == "none"
    assert settings.model is None
    assert settings.api_key is None


def test_openai_provider_from_env() -> None:
    env = {
        "FACTORY_IMAGE_GEN_PROVIDER": "openai",
        "FACTORY_IMAGE_GEN_MODEL": "dall-e-3",
        "OPENAI_API_KEY": "sk-test",
    }
    settings = ImageGenSettings.from_env(env)
    assert settings.provider == "openai"
    assert settings.model == "dall-e-3"
    assert settings.api_key == "sk-test"


def test_openai_slot_api_key_overrides_global() -> None:
    env = {
        "FACTORY_IMAGE_GEN_PROVIDER": "openai",
        "FACTORY_IMAGE_GEN_MODEL": "dall-e-3",
        "FACTORY_IMAGE_GEN_API_KEY": "sk-slot",
        "OPENAI_API_KEY": "sk-global",
    }
    settings = ImageGenSettings.from_env(env)
    assert settings.api_key == "sk-slot"


def test_local_provider_from_env() -> None:
    env = {
        "FACTORY_IMAGE_GEN_PROVIDER": "local",
        "FACTORY_IMAGE_GEN_MODEL": "stabilityai/sdxl-turbo",
    }
    settings = ImageGenSettings.from_env(env)
    assert settings.provider == "local"
    assert settings.model == "stabilityai/sdxl-turbo"
    assert settings.api_key is None
    assert settings.device is None


def test_local_provider_reads_optional_device() -> None:
    env = {
        "FACTORY_IMAGE_GEN_PROVIDER": "local",
        "FACTORY_IMAGE_GEN_MODEL": "stabilityai/sdxl-turbo",
        "FACTORY_IMAGE_GEN_DEVICE": "mps",
    }
    settings = ImageGenSettings.from_env(env)
    assert settings.device == "mps"


def test_local_without_model_raises() -> None:
    env = {"FACTORY_IMAGE_GEN_PROVIDER": "local"}
    with pytest.raises(ValueError, match="FACTORY_IMAGE_GEN_MODEL"):
        ImageGenSettings.from_env(env)


def test_unsupported_provider_raises() -> None:
    env = {"FACTORY_IMAGE_GEN_PROVIDER": "unknown"}
    with pytest.raises(ValueError, match="Unsupported image generation provider"):
        ImageGenSettings.from_env(env)


def test_openai_without_model_raises() -> None:
    env = {"FACTORY_IMAGE_GEN_PROVIDER": "openai", "OPENAI_API_KEY": "sk-test"}
    with pytest.raises(ValueError, match="FACTORY_IMAGE_GEN_MODEL"):
        ImageGenSettings.from_env(env)
