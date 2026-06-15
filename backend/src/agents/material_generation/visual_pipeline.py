"""재료 텍스처 및 아이콘을 생성하기 위해 시뮬레이션된 시각 자산 파이프라인입니다."""

from __future__ import annotations

import logging
import time
from collections.abc import Callable

from sqlalchemy import select
from sqlalchemy.orm import Session

from db.engine import get_db_session
from db.models import GeneratedMaterialModel

logger = logging.getLogger(__name__)


class VisualAssetPipeline:
    """재료 자산의 텍스처링 및 렌더링을 위한 동기식 백그라운드 처리 클래스입니다."""

    session_factory: Callable[[], Session] | None = None

    @classmethod
    def process_visual_asset(
        cls,
        material_id: str,
        visual_prompt: str,
        category: str,
    ) -> None:
        """격리된 에러 폴백을 활용하여 이미지 텍스처링을 시뮬레이션하는 백그라운드 작업입니다."""
        logger.info(
            "VisualAssetPipeline: Starting rendering for material: %s", material_id
        )

        # 처리 지연 시뮬레이션
        time.sleep(2.0)

        factory = cls.session_factory or get_db_session
        with factory() as session:
            try:
                # 자산 키를 업데이트하기 위해 재료 모델을 쿼리합니다.
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

                # 실패 시뮬레이션 경로
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
