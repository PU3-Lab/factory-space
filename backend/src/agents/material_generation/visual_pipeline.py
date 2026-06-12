"""Simulated visual asset pipeline for generating material textures and icons."""

from __future__ import annotations

import logging
import time

from sqlalchemy import select

from db.engine import get_db_session
from db.models import GeneratedMaterialModel

logger = logging.getLogger(__name__)


class VisualAssetPipeline:
    """Synchronous background processing for texturing and rendering material assets."""

    @classmethod
    def process_visual_asset(
        cls,
        material_id: str,
        visual_prompt: str,
        category: str,
    ) -> None:
        """Background task simulating image texturing with isolated error fallback."""
        logger.info(
            "VisualAssetPipeline: Starting rendering for material: %s", material_id
        )

        # Simulate processing delay
        time.sleep(2.0)

        with get_db_session() as session:
            try:
                # Query the material model to update its asset keys
                stmt = select(GeneratedMaterialModel).where(
                    GeneratedMaterialModel.id == material_id
                )
                material = session.execute(stmt).scalar_one_or_none()

                if not material:
                    logger.warning(
                        "VisualAssetPipeline: Material %s was not found in database.",
                        material_id,
                    )
                    return

                # Failure Simulation Path
                if "fail" in visual_prompt.lower() or "error" in visual_prompt.lower():
                    logger.warning(
                        "VisualAssetPipeline: Simulating pipeline failure for %s",
                        material_id,
                    )
                    material.visual_status = "failed"
                    material.visual_error = (
                        "Asset pipeline failed during neural diffusion rendering."
                    )
                    material.fallback_icon = f"materials/default/{category}.png"
                else:
                    logger.info(
                        "VisualAssetPipeline: Asset rendering complete for %s",
                        material_id,
                    )
                    material.visual_status = "visual_ready"
                    material.visual_asset_key = f"materials/{material_id}/icon.png"
                    material.texture_asset_key = f"materials/{material_id}/texture.png"
                    material.thumbnail_asset_key = (
                        f"materials/{material_id}/thumbnail.png"
                    )

                session.commit()
            except Exception as exc:
                logger.error(
                    "VisualAssetPipeline: Unexpected failure while updating assets: %s",
                    exc,
                )
                session.rollback()
