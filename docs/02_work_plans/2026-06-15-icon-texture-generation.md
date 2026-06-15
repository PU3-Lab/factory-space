# Icon & Texture Generation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the simulated `visual_pipeline.py` (fake sleep + hard-coded paths) with a real image-generation + storage pipeline, following the same provider-adapter pattern used by `src/llm/`.

**Architecture:** The new `visual/` module gains three new files alongside the existing `profile.py`: `settings.py` (env-driven config), `adapter.py` (image-generation Protocol + Placeholder/OpenAI impls + `resize_to_profile`), and `storage.py` (image-storage Protocol + LocalFile impl). The adapter generates ONE master image per material; the pipeline downscales it to icon/texture/thumbnail with Pillow — one API call per material instead of three. `VisualAssetPipeline` gains two injectable class-level adapter slots (`_image_adapter`, `_storage_adapter`) so tests can inject stubs without touching the filesystem or calling APIs.

**Tech Stack:** Python 3.12, Pillow ≥10.0.0 (image creation/resize), OpenAI SDK (DALL-E 3), SQLAlchemy, pytest, ruff

---

## File Map

| Action | Path | Purpose |
|--------|------|---------|
| Verify | `backend/pyproject.toml` | `Pillow>=10.0.0` already present (commit `fc2dbe9`) — verify only |
| Modify | `backend/src/visual/profile.py` | `ImageProfile` dataclass + `ICON`/`TEXTURE`/`THUMBNAIL` constants |
| Create | `backend/src/visual/settings.py` | `ImageGenSettings` from env vars |
| Create | `backend/src/visual/storage.py` | `ImageStorageAdapter` Protocol + `LocalFileStorageAdapter` |
| Create | `backend/src/visual/adapter.py` | `ImageGenerationAdapter` Protocol + `PlaceholderImageAdapter` + `OpenAIImageAdapter` + `resize_to_profile()` |
| Modify | `backend/src/agents/material_generation/visual_pipeline.py` | Replace simulation with real generate→resize→store pipeline |
| Modify | `backend/tests/agents/material_generation/conftest.py` | Inject `PlaceholderImageAdapter` + `NoopStorageAdapter` |
| Create | `backend/tests/visual/__init__.py` | Empty init for test package |
| Create | `backend/tests/visual/test_profile.py` | Unit tests for ImageProfile constants |
| Create | `backend/tests/visual/test_settings.py` | Unit tests for env-based settings |
| Create | `backend/tests/visual/test_storage.py` | Unit tests for LocalFileStorageAdapter |
| Create | `backend/tests/visual/test_adapter.py` | Unit tests for PlaceholderImageAdapter + OpenAIImageAdapter |
| Create | `backend/tests/visual/test_pipeline.py` | Unit tests for updated VisualAssetPipeline |

---

## Task 1: Verify Pillow dependency

> **Note:** `Pillow>=10.0.0` is ALREADY in `backend/pyproject.toml:19` (committed in `fc2dbe9`). Do NOT re-add it or create a new commit — this task is verification only.

**Files:**
- Verify: `backend/pyproject.toml`

- [ ] **Step 1: Verify baseline tests pass**

```bash
cd backend && python -m pytest tests/agents/material_generation/ -v --tb=short -q
```
Expected: All existing tests PASS.

- [ ] **Step 2: Confirm Pillow is installed and importable**

```bash
cd backend && python -c "from PIL import Image; print('Pillow OK')"
```
Expected: `Pillow OK`. If it fails, run `pip install -e ".[dev]"` first (Pillow is already declared, so this only installs it locally — no `pyproject.toml` edit or commit needed).

---

## Task 2: ImageProfile constants

**Files:**
- Modify: `backend/src/visual/profile.py`
- Create: `backend/tests/visual/__init__.py`
- Create: `backend/tests/visual/test_profile.py`

- [ ] **Step 1: Write the failing tests**

Create `backend/tests/visual/__init__.py` (empty).

Create `backend/tests/visual/test_profile.py`:

```python
"""Unit tests for ImageProfile constants."""

from __future__ import annotations

import pytest

from visual.profile import ICON, MASTER, THUMBNAIL, TEXTURE, ImageProfile


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
```

- [ ] **Step 2: Run — expect FAIL**

```bash
cd backend && python -m pytest tests/visual/test_profile.py -v
```
Expected: FAIL with `ImportError` (profile.py is empty).

- [ ] **Step 3: Implement `visual/profile.py`**

Replace the entire content of `backend/src/visual/profile.py`:

```python
"""Material visual asset size and format profiles."""

from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class ImageProfile:
    """Defines output dimensions and format for one image asset type."""

    name: str
    width: int
    height: int
    format: str  # PIL format string, e.g. "PNG"


ICON = ImageProfile(name="icon", width=512, height=512, format="PNG")
TEXTURE = ImageProfile(name="texture", width=1024, height=1024, format="PNG")
THUMBNAIL = ImageProfile(name="thumbnail", width=128, height=128, format="PNG")

# MASTER is the single high-resolution image the adapter generates once;
# ICON/TEXTURE/THUMBNAIL are downscaled from it (one API call per material).
MASTER = ImageProfile(name="master", width=1024, height=1024, format="PNG")
```

- [ ] **Step 4: Run — expect PASS**

```bash
cd backend && python -m pytest tests/visual/test_profile.py -v
```
Expected: 5 tests PASS.

- [ ] **Step 5: Format and lint**

```bash
cd backend && ruff check --fix . && ruff format .
```

- [ ] **Step 6: Commit**

```bash
git add backend/src/visual/profile.py backend/tests/visual/__init__.py backend/tests/visual/test_profile.py
git commit -m "feat(visual): add ImageProfile dataclass and ICON/TEXTURE/THUMBNAIL constants"
```

---

## Task 3: ImageGenSettings from environment

**Files:**
- Create: `backend/src/visual/settings.py`
- Create: `backend/tests/visual/test_settings.py`

- [ ] **Step 1: Write the failing tests**

Create `backend/tests/visual/test_settings.py`:

```python
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


def test_unsupported_provider_raises() -> None:
    env = {"FACTORY_IMAGE_GEN_PROVIDER": "unknown"}
    with pytest.raises(ValueError, match="Unsupported image generation provider"):
        ImageGenSettings.from_env(env)


def test_openai_without_model_raises() -> None:
    env = {"FACTORY_IMAGE_GEN_PROVIDER": "openai", "OPENAI_API_KEY": "sk-test"}
    with pytest.raises(ValueError, match="FACTORY_IMAGE_GEN_MODEL"):
        ImageGenSettings.from_env(env)
```

- [ ] **Step 2: Run — expect FAIL**

```bash
cd backend && python -m pytest tests/visual/test_settings.py -v
```
Expected: FAIL with `ImportError`.

- [ ] **Step 3: Implement `visual/settings.py`**

Create `backend/src/visual/settings.py`:

```python
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
```

- [ ] **Step 4: Run — expect PASS**

```bash
cd backend && python -m pytest tests/visual/test_settings.py -v
```
Expected: 5 tests PASS.

- [ ] **Step 5: Format and lint**

```bash
cd backend && ruff check --fix . && ruff format .
```

- [ ] **Step 6: Commit**

```bash
git add backend/src/visual/settings.py backend/tests/visual/test_settings.py
git commit -m "feat(visual): add ImageGenSettings with env-based provider config"
```

---

## Task 4: ImageStorageAdapter and LocalFileStorageAdapter

**Files:**
- Create: `backend/src/visual/storage.py`
- Create: `backend/tests/visual/test_storage.py`

- [ ] **Step 1: Write the failing tests**

Create `backend/tests/visual/test_storage.py`:

```python
"""Unit tests for ImageStorageAdapter implementations."""

from __future__ import annotations

from pathlib import Path

from visual.storage import LocalFileStorageAdapter


def test_local_file_storage_saves_bytes(tmp_path: Path) -> None:
    adapter = LocalFileStorageAdapter(base_path=str(tmp_path))
    adapter.save("materials/abc123/icon.png", b"fake-image-bytes")
    saved = (tmp_path / "materials" / "abc123" / "icon.png").read_bytes()
    assert saved == b"fake-image-bytes"


def test_local_file_storage_creates_parent_dirs(tmp_path: Path) -> None:
    adapter = LocalFileStorageAdapter(base_path=str(tmp_path))
    adapter.save("deep/nested/path/file.png", b"data")
    assert (tmp_path / "deep" / "nested" / "path" / "file.png").exists()


def test_local_file_storage_overwrites_existing(tmp_path: Path) -> None:
    adapter = LocalFileStorageAdapter(base_path=str(tmp_path))
    adapter.save("materials/x/icon.png", b"first")
    adapter.save("materials/x/icon.png", b"second")
    assert (tmp_path / "materials" / "x" / "icon.png").read_bytes() == b"second"


def test_noop_storage_discards_without_error() -> None:
    from visual.storage import NoopStorageAdapter

    adapter = NoopStorageAdapter()
    # Should not raise and should not write anywhere
    adapter.save("materials/x/icon.png", b"ignored")
```

- [ ] **Step 2: Run — expect FAIL**

```bash
cd backend && python -m pytest tests/visual/test_storage.py -v
```
Expected: FAIL with `ImportError`.

- [ ] **Step 3: Implement `visual/storage.py`**

Create `backend/src/visual/storage.py`:

```python
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
        logger.debug("NoopStorageAdapter: discarded %d bytes for key=%s", len(data), key)
```

- [ ] **Step 4: Run — expect PASS**

```bash
cd backend && python -m pytest tests/visual/test_storage.py -v
```
Expected: 4 tests PASS.

- [ ] **Step 5: Format and lint**

```bash
cd backend && ruff check --fix . && ruff format .
```

- [ ] **Step 6: Commit**

```bash
git add backend/src/visual/storage.py backend/tests/visual/test_storage.py
git commit -m "feat(visual): add ImageStorageAdapter protocol and LocalFileStorageAdapter"
```

---

## Task 5: ImageGenerationAdapter with Placeholder and OpenAI implementations

**Files:**
- Create: `backend/src/visual/adapter.py`
- Create: `backend/tests/visual/test_adapter.py`

- [ ] **Step 1: Write the failing tests**

Create `backend/tests/visual/test_adapter.py`:

```python
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


def _settings_openai(model: str = "dall-e-3", api_key: str = "sk-test") -> ImageGenSettings:
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
```

- [ ] **Step 2: Run — expect FAIL**

```bash
cd backend && python -m pytest tests/visual/test_adapter.py -v
```
Expected: FAIL with `ImportError`.

- [ ] **Step 3: Implement `visual/adapter.py`**

Create `backend/src/visual/adapter.py`:

```python
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
                response_format="b64_json",
            )
            b64_data = response.data[0].b64_json
            return base64.b64decode(b64_data)
        except Exception as exc:
            logger.warning("OpenAIImageAdapter: generation failed: %s", exc)
            return None


def create_image_adapter(settings: ImageGenSettings) -> ImageGenerationAdapter:
    """Create an image generation adapter for the configured provider."""
    if settings.provider == "none":
        return PlaceholderImageAdapter()
    if settings.provider == "openai":
        return OpenAIImageAdapter(settings=settings)
    return PlaceholderImageAdapter()


def resize_to_profile(master_bytes: bytes, profile: ImageProfile) -> bytes:
    """Downscale a master image to the given profile's dimensions and format."""
    img = Image.open(io.BytesIO(master_bytes)).resize(
        (profile.width, profile.height),
        Image.Resampling.LANCZOS,
    )
    buf = io.BytesIO()
    img.save(buf, format=profile.format)
    return buf.getvalue()


def _create_openai_client(settings: ImageGenSettings) -> object | None:
    if not settings.api_key:
        return None
    from openai import OpenAI

    return OpenAI(api_key=settings.api_key)
```

- [ ] **Step 4: Run — expect PASS**

```bash
cd backend && python -m pytest tests/visual/test_adapter.py -v
```
Expected: All 8 tests PASS.

- [ ] **Step 5: Format and lint**

```bash
cd backend && ruff check --fix . && ruff format .
```

- [ ] **Step 6: Commit**

```bash
git add backend/src/visual/adapter.py backend/tests/visual/test_adapter.py
git commit -m "feat(visual): add ImageGenerationAdapter protocol, PlaceholderImageAdapter, and OpenAIImageAdapter"
```

---

## Task 6: Replace visual_pipeline.py simulation with real pipeline

**Files:**
- Modify: `backend/src/agents/material_generation/visual_pipeline.py`
- Modify: `backend/tests/agents/material_generation/conftest.py`
- Create: `backend/tests/visual/test_pipeline.py`

- [ ] **Step 1: Write the failing pipeline unit tests**

Create `backend/tests/visual/test_pipeline.py`:

```python
"""Unit tests for VisualAssetPipeline with injected adapter stubs."""

from __future__ import annotations

import io
from collections.abc import Iterator
from dataclasses import dataclass

import pytest
from PIL import Image
from sqlalchemy import create_engine, select
from sqlalchemy.orm import Session, sessionmaker
from sqlalchemy.pool import StaticPool

from agents.material_generation.visual_pipeline import VisualAssetPipeline
from db.models import GeneratedMaterialModel
from visual.profile import MASTER
from visual.storage import NoopStorageAdapter


@dataclass(frozen=True)
class _AlwaysSucceedsAdapter:
    """Returns a real master PNG so the pipeline can resize it."""

    def generate(self, prompt: str) -> bytes | None:
        img = Image.new("RGB", (MASTER.width, MASTER.height), (10, 20, 30))
        buf = io.BytesIO()
        img.save(buf, format=MASTER.format)
        return buf.getvalue()


@dataclass(frozen=True)
class _AlwaysFailsAdapter:
    def generate(self, prompt: str) -> bytes | None:
        return None


@pytest.fixture
def pipeline_session() -> Iterator[tuple[Session, sessionmaker]]:
    engine = create_engine(
        "sqlite:///:memory:",
        connect_args={"check_same_thread": False},
        poolclass=StaticPool,
    )
    GeneratedMaterialModel.__table__.create(engine)
    factory = sessionmaker(bind=engine, expire_on_commit=False, class_=Session)
    session = factory()
    yield session, factory
    session.close()
    engine.dispose()


def _make_material(session: Session, material_id: str) -> None:
    session.add(
        GeneratedMaterialModel(
            id=material_id,
            material_hash=f"hash_{material_id}",
            name="Test Material",
            category="alloy",
            rarity="common",
            properties_json={},
            visual_status="pending",
            visual_prompt="glowing iron ingot",
            fallback_icon="materials/default/alloy.png",
        )
    )
    session.commit()


def test_pipeline_sets_visual_ready_on_success(
    pipeline_session: tuple[Session, sessionmaker],
) -> None:
    session, factory = pipeline_session
    _make_material(session, "mat_001")

    VisualAssetPipeline.session_factory = factory
    VisualAssetPipeline._image_adapter = _AlwaysSucceedsAdapter()
    VisualAssetPipeline._storage_adapter = NoopStorageAdapter()

    try:
        VisualAssetPipeline.process_visual_asset("mat_001", "glowing iron ingot", "alloy")
    finally:
        VisualAssetPipeline.session_factory = None
        VisualAssetPipeline._image_adapter = None
        VisualAssetPipeline._storage_adapter = None

    session.expire_all()
    result = session.execute(
        select(GeneratedMaterialModel).where(GeneratedMaterialModel.id == "mat_001")
    ).scalar_one()
    assert result.visual_status == "visual_ready"
    assert result.visual_asset_key == "materials/mat_001/icon.png"
    assert result.texture_asset_key == "materials/mat_001/texture.png"
    assert result.thumbnail_asset_key == "materials/mat_001/thumbnail.png"


def test_pipeline_sets_failed_when_adapter_returns_none(
    pipeline_session: tuple[Session, sessionmaker],
) -> None:
    session, factory = pipeline_session
    _make_material(session, "mat_002")

    VisualAssetPipeline.session_factory = factory
    VisualAssetPipeline._image_adapter = _AlwaysFailsAdapter()
    VisualAssetPipeline._storage_adapter = NoopStorageAdapter()

    try:
        VisualAssetPipeline.process_visual_asset("mat_002", "glowing iron ingot", "alloy")
    finally:
        VisualAssetPipeline.session_factory = None
        VisualAssetPipeline._image_adapter = None
        VisualAssetPipeline._storage_adapter = None

    session.expire_all()
    result = session.execute(
        select(GeneratedMaterialModel).where(GeneratedMaterialModel.id == "mat_002")
    ).scalar_one()
    assert result.visual_status == "failed"
    assert result.fallback_icon == "materials/default/alloy.png"
    assert result.visual_error is not None


def test_pipeline_handles_missing_material_gracefully(
    pipeline_session: tuple[Session, sessionmaker],
) -> None:
    _, factory = pipeline_session
    VisualAssetPipeline.session_factory = factory
    VisualAssetPipeline._image_adapter = _AlwaysSucceedsAdapter()
    VisualAssetPipeline._storage_adapter = NoopStorageAdapter()

    try:
        # Should not raise
        VisualAssetPipeline.process_visual_asset("nonexistent_id", "prompt", "alloy")
    finally:
        VisualAssetPipeline.session_factory = None
        VisualAssetPipeline._image_adapter = None
        VisualAssetPipeline._storage_adapter = None
```

- [ ] **Step 2: Run — expect FAIL**

```bash
cd backend && python -m pytest tests/visual/test_pipeline.py -v
```
Expected: FAIL (no `_image_adapter`/`_storage_adapter` class vars yet).

- [ ] **Step 3: Replace `visual_pipeline.py`**

Replace the entire content of `backend/src/agents/material_generation/visual_pipeline.py`:

```python
"""Real image generation and storage pipeline for material visual assets."""

from __future__ import annotations

import logging
import os
from collections.abc import Callable

from sqlalchemy import select
from sqlalchemy.orm import Session

from db.engine import get_db_session
from db.models import GeneratedMaterialModel
from visual.adapter import (
    ImageGenerationAdapter,
    create_image_adapter,
    resize_to_profile,
)
from visual.profile import ICON, TEXTURE, THUMBNAIL
from visual.settings import ImageGenSettings
from visual.storage import (
    ImageStorageAdapter,
    LocalFileStorageAdapter,
    NoopStorageAdapter,
)

logger = logging.getLogger(__name__)

_DEFAULT_STORAGE_PATH = os.environ.get(
    "FACTORY_IMAGE_STORAGE_PATH",
    "/tmp/factory-space/assets",
)


class VisualAssetPipeline:
    """Generates and stores visual assets for materials using injected adapters."""

    session_factory: Callable[[], Session] | None = None
    _image_adapter: ImageGenerationAdapter | None = None
    _storage_adapter: ImageStorageAdapter | None = None

    @classmethod
    def process_visual_asset(
        cls,
        material_id: str,
        visual_prompt: str,
        category: str,
    ) -> None:
        """Generate icon, texture, and thumbnail for a material and update its DB record."""
        logger.info("VisualAssetPipeline: starting for material %s", material_id)

        image_adapter = cls._image_adapter or _default_image_adapter()
        storage_adapter = cls._storage_adapter or _default_storage_adapter()

        factory = cls.session_factory or get_db_session
        with factory() as session:
            try:
                material = session.execute(
                    select(GeneratedMaterialModel).where(
                        GeneratedMaterialModel.id == material_id
                    )
                ).scalar_one_or_none()

                if not material:
                    logger.warning("VisualAssetPipeline: material %s not found", material_id)
                    return

                # Generate ONE master image, then downscale to every profile.
                master_data = image_adapter.generate(visual_prompt)
                if master_data is None:
                    logger.warning(
                        "VisualAssetPipeline: image generation returned None for %s",
                        material_id,
                    )
                    material.visual_status = "failed"
                    material.visual_error = "Image generation returned no data."
                    material.fallback_icon = f"materials/default/{category}.png"
                else:
                    icon_key = f"materials/{material_id}/icon.png"
                    texture_key = f"materials/{material_id}/texture.png"
                    thumbnail_key = f"materials/{material_id}/thumbnail.png"

                    storage_adapter.save(icon_key, resize_to_profile(master_data, ICON))
                    storage_adapter.save(
                        texture_key, resize_to_profile(master_data, TEXTURE)
                    )
                    storage_adapter.save(
                        thumbnail_key, resize_to_profile(master_data, THUMBNAIL)
                    )

                    material.visual_status = "visual_ready"
                    material.visual_asset_key = icon_key
                    material.texture_asset_key = texture_key
                    material.thumbnail_asset_key = thumbnail_key
                    logger.info("VisualAssetPipeline: complete for %s", material_id)

                session.commit()
            except Exception as exc:
                logger.error(
                    "VisualAssetPipeline: unexpected failure for %s: %s",
                    material_id,
                    exc,
                )
                session.rollback()


def _default_image_adapter() -> ImageGenerationAdapter:
    try:
        settings = ImageGenSettings.from_env()
    except ValueError:
        settings = ImageGenSettings(provider="none")
    return create_image_adapter(settings)


def _default_storage_adapter() -> ImageStorageAdapter:
    backend = os.environ.get("FACTORY_IMAGE_STORAGE_BACKEND", "local").strip().lower()
    if backend == "local":
        return LocalFileStorageAdapter(base_path=_DEFAULT_STORAGE_PATH)
    return NoopStorageAdapter()
```

- [ ] **Step 4: Run pipeline unit tests — expect PASS**

```bash
cd backend && python -m pytest tests/visual/test_pipeline.py -v
```
Expected: 3 tests PASS.

- [ ] **Step 5: Update `conftest.py` to inject Placeholder + Noop adapters**

In `backend/tests/agents/material_generation/conftest.py`, update the fixture to inject adapters so background processing uses `PlaceholderImageAdapter` (no API calls) and `NoopStorageAdapter` (no disk I/O).

Add the following imports after the existing ones:

```python
from visual.adapter import PlaceholderImageAdapter
from visual.storage import NoopStorageAdapter
```

Inside the `db_session` fixture, add these two lines immediately after `VisualAssetPipeline.session_factory = session_factory`:

```python
    VisualAssetPipeline._image_adapter = PlaceholderImageAdapter()
    VisualAssetPipeline._storage_adapter = NoopStorageAdapter()
```

Add the following two lines in the teardown block (after `VisualAssetPipeline.session_factory = None`):

```python
    VisualAssetPipeline._image_adapter = None
    VisualAssetPipeline._storage_adapter = None
```

The full updated `conftest.py`:

```python
"""재료 생성 에이전트 테스트를 위한 Pytest conftest 설정입니다."""

from __future__ import annotations

from collections.abc import Iterator

import pytest
from sqlalchemy import create_engine
from sqlalchemy.orm import Session, sessionmaker
from sqlalchemy.pool import StaticPool

from agents.material_generation.events import MaterialEventPublisher
from agents.material_generation.recipe_repository import RecipeRepository
from agents.material_generation.visual_pipeline import VisualAssetPipeline
from db.models import (
    GeneratedExperimentModel,
    GeneratedMaterialDiscoveryModel,
    GeneratedMaterialModel,
    RecipeModel,
)
from visual.adapter import PlaceholderImageAdapter
from visual.storage import NoopStorageAdapter


@pytest.fixture
def db_session() -> Iterator[Session]:
    """기본 테스트 레시피가 미리 채워진 인메모리 SQLite 세션을 제공합니다."""
    MaterialEventPublisher.reset_executor(wait=False)
    engine = create_engine(
        "sqlite:///:memory:",
        echo=False,
        connect_args={"check_same_thread": False},
        poolclass=StaticPool,
    )

    RecipeModel.__table__.create(engine)
    GeneratedExperimentModel.__table__.create(engine)
    GeneratedMaterialModel.__table__.create(engine)
    GeneratedMaterialDiscoveryModel.__table__.create(engine)

    session_factory = sessionmaker(bind=engine, expire_on_commit=False, class_=Session)

    VisualAssetPipeline.session_factory = session_factory
    VisualAssetPipeline._image_adapter = PlaceholderImageAdapter()
    VisualAssetPipeline._storage_adapter = NoopStorageAdapter()

    session = session_factory()
    session.add(
        RecipeModel(
            recipe_name="Smelt_Iron",
            machine_type="Smelter",
            input_item_1="iron_ore",
            input_qty_1=2,
            output_item_1="iron_ingot",
            output_qty_1=1,
            crafting_time=3.0,
        )
    )
    session.add(
        RecipeModel(
            recipe_name="Crush_Iron",
            machine_type="Grinder",
            input_item_1="iron_ingot",
            input_qty_1=1,
            output_item_1="iron_powder",
            output_qty_1=2,
            crafting_time=1.5,
        )
    )
    session.commit()

    RecipeRepository.reload_cache(session)
    yield session

    session.close()

    MaterialEventPublisher.wait_for_jobs()
    VisualAssetPipeline.session_factory = None
    VisualAssetPipeline._image_adapter = None
    VisualAssetPipeline._storage_adapter = None

    engine.dispose()
```

- [ ] **Step 6: Run all material_generation tests — expect PASS**

```bash
cd backend && python -m pytest tests/agents/material_generation/ -v --tb=short
```
Expected: All tests PASS (including `test_agent_synthesize_new_material_visual_asset_true_background_processing` — it now uses PlaceholderImageAdapter + NoopStorageAdapter instead of the sleep simulation).

- [ ] **Step 7: Run full test suite**

```bash
cd backend && python -m pytest tests/ -v --tb=short -q
```
Expected: All tests PASS.

- [ ] **Step 8: Format and lint**

```bash
cd backend && ruff check --fix . && ruff format .
```

- [ ] **Step 9: Commit**

```bash
git add backend/src/agents/material_generation/visual_pipeline.py \
        backend/tests/agents/material_generation/conftest.py \
        backend/tests/visual/test_pipeline.py
git commit -m "feat(visual): replace simulated pipeline with real image generation and storage"
```

---

## Self-Review

### Spec coverage

| Requirement | Covered by |
|-------------|-----------|
| Real image generation (not sleep simulation) | Task 6 — `visual_pipeline.py` calls `image_adapter.generate()` |
| One API call per material (cost control) | Task 5/6 — adapter returns one MASTER image; pipeline downscales it 3× |
| Provider adapter pattern (mirrors `llm/`) | Tasks 3, 5 — `ImageGenSettings` + Protocol + Placeholder/OpenAI impls |
| Pillow for image resizing | Pillow already in `pyproject.toml:19`; Task 5 — `resize_to_profile()` |
| Local filesystem storage | Task 4 — `LocalFileStorageAdapter` |
| Key format `materials/{id}/icon.png` preserved | Task 6 — hardcoded key templates in `visual_pipeline.py` |
| Test isolation (no API calls, no disk I/O) | Task 6 — `PlaceholderImageAdapter` + `NoopStorageAdapter` in conftest |
| `visual_status` lifecycle respected | Task 6 — `"visual_ready"` on success, `"failed"` on None return |
| Existing test `visual_asset_key` assertion preserved | Task 6 — key format identical to simulation |
| `fallback_icon` set on failure | Task 6 — `f"materials/default/{category}.png"` |
| 80%+ test coverage | Tasks 2–6 — unit tests for every new file |

### No placeholder / TODO items
All code blocks are complete. No "implement later" or "fill in details."

### Type consistency
- `ImageProfile` defined in Task 2 (`ICON`/`TEXTURE`/`THUMBNAIL`/`MASTER`); used with `.name`, `.width`, `.height`, `.format` — consistent across adapter, storage, and pipeline tasks.
- `ImageGenerationAdapter.generate(prompt: str) -> bytes | None` (master image, no profile arg) — consistent across Protocol, PlaceholderImageAdapter, OpenAIImageAdapter, the test stubs in Task 6, and the pipeline call site.
- `resize_to_profile(master_bytes: bytes, profile: ImageProfile) -> bytes` — defined in Task 5 `visual/adapter.py`, called by the pipeline in Task 6 for ICON/TEXTURE/THUMBNAIL.
- `ImageStorageAdapter.save(key: str, data: bytes) -> None` — consistent across Protocol, LocalFileStorageAdapter, NoopStorageAdapter, and pipeline call sites.
- `VisualAssetPipeline._image_adapter` / `_storage_adapter` — injected in conftest, reset in teardown, used in pipeline.

---

**Plan complete and saved to `docs/02_work_plans/2026-06-15-icon-texture-generation.md`.**

**Two execution options:**

**1. Subagent-Driven (recommended)** — Fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** — Execute tasks in this session using executing-plans, batch execution with checkpoints

**Which approach?**
