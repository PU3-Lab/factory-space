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
        VisualAssetPipeline.process_visual_asset(
            "mat_001", "glowing iron ingot", "alloy"
        )
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
        VisualAssetPipeline.process_visual_asset(
            "mat_002", "glowing iron ingot", "alloy"
        )
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


def test_pipeline_raises_on_unsupported_storage_backend(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    from agents.material_generation.visual_pipeline import _default_storage_adapter

    monkeypatch.setenv("FACTORY_IMAGE_STORAGE_BACKEND", "unknown_backend")
    with pytest.raises(ValueError, match="Unsupported image storage backend"):
        _default_storage_adapter()


def test_pipeline_returns_noop_storage_on_noop_backend(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    from agents.material_generation.visual_pipeline import _default_storage_adapter
    from visual.storage import NoopStorageAdapter

    monkeypatch.setenv("FACTORY_IMAGE_STORAGE_BACKEND", "noop")
    adapter = _default_storage_adapter()
    assert isinstance(adapter, NoopStorageAdapter)


def test_default_storage_path_is_persistent_not_tmp(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    # m4 회귀 방지: 기본 저장 경로가 휘발성 /tmp 로 되돌아가지 않아야 한다.
    from agents.material_generation.visual_pipeline import (
        _DEFAULT_STORAGE_PATH,
        _default_storage_adapter,
    )
    from visual.storage import LocalFileStorageAdapter

    monkeypatch.delenv("FACTORY_IMAGE_STORAGE_BACKEND", raising=False)
    adapter = _default_storage_adapter()
    assert isinstance(adapter, LocalFileStorageAdapter)
    assert not _DEFAULT_STORAGE_PATH.startswith("/tmp")
    assert adapter.base_path == _DEFAULT_STORAGE_PATH


def test_default_image_adapter_warns_when_provider_unset(
    monkeypatch: pytest.MonkeyPatch,
    caplog: pytest.LogCaptureFixture,
) -> None:
    # m3: lifecycle(placeholder→visual_ready)은 유지하되 미설정 시 경고가 남아야 한다.
    from agents.material_generation.visual_pipeline import _default_image_adapter
    from visual.adapter import PlaceholderImageAdapter

    monkeypatch.delenv("FACTORY_IMAGE_GEN_PROVIDER", raising=False)
    with caplog.at_level("WARNING"):
        adapter = _default_image_adapter()

    assert isinstance(adapter, PlaceholderImageAdapter)
    assert any("not configured" in record.message for record in caplog.records)
