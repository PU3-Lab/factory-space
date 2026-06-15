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

# 영속 자산 저장 경로. 프로덕션에서는 FACTORY_IMAGE_STORAGE_PATH로 영속 절대 경로를
# 반드시 지정한다. 기본값은 db/engine.py의 SQLite(./factory_space.db)와 동일하게
# 실행 디렉터리(backend/) 기준 상대 경로 var/assets 를 사용한다(.gitignore 처리됨).
# /tmp 대신 상대 영속 경로를 기본값으로 둬서 재부팅 시 자산이 소실되지 않게 한다.
_DEFAULT_STORAGE_PATH = os.environ.get(
    "FACTORY_IMAGE_STORAGE_PATH",
    "var/assets",
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
                    logger.warning(
                        "VisualAssetPipeline: material %s not found", material_id
                    )
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
    if settings.provider == "none":
        # 프로바이더 미설정 시 단색 placeholder가 생성되고 visual_ready로 마킹된다.
        # lifecycle 자체는 유지하되(placeholder도 유효한 PNG 자산), 프로덕션 오설정을
        # 조기에 인지할 수 있도록 경고를 남긴다.
        logger.warning(
            "Image generation provider is not configured "
            "(FACTORY_IMAGE_GEN_PROVIDER unset); using a solid-color placeholder. "
            "Materials will be marked visual_ready with placeholder assets."
        )
    return create_image_adapter(settings)


def _default_storage_adapter() -> ImageStorageAdapter:
    backend_raw = os.environ.get("FACTORY_IMAGE_STORAGE_BACKEND", "local")
    backend = backend_raw.strip().lower()
    if backend == "local":
        return LocalFileStorageAdapter(base_path=_DEFAULT_STORAGE_PATH)
    if backend == "noop":
        return NoopStorageAdapter()
    raise ValueError(
        f"Unsupported image storage backend: {backend_raw!r}. "
        f"Supported backends are 'local' or 'noop'."
    )
