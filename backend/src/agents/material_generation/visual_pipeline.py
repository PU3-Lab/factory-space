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
    "FACTORY_IMAGE_STORAGE_PATH", "/tmp/factory-space/assets"
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

                icon_data = image_adapter.generate(visual_prompt, ICON)
                if icon_data is None:
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

                    storage_adapter.save(icon_key, icon_data)

                    texture_data = image_adapter.generate(visual_prompt, TEXTURE)
                    if texture_data:
                        storage_adapter.save(texture_key, texture_data)

                    thumbnail_data = image_adapter.generate(visual_prompt, THUMBNAIL)
                    if thumbnail_data:
                        storage_adapter.save(thumbnail_key, thumbnail_data)

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
