"""재료 생성 에이전트 실험을 위한 API 라우터입니다."""

from __future__ import annotations

import logging
from typing import Any

from fastapi import APIRouter, HTTPException, status
from sqlalchemy import select

from agents.material_generation.agent import MaterialCreationAgent
from agents.material_generation.schemas import (
    MaterialCreationRequest,
    MaterialCreationResponse,
)
from db.engine import get_db_session
from db.models import GeneratedMaterialModel

logger = logging.getLogger(__name__)

router = APIRouter()
agent = MaterialCreationAgent()


@router.post(
    "/experiments/material-creation",
    response_model=MaterialCreationResponse,
    status_code=status.HTTP_201_CREATED,
)
def create_material_experiment(
    request: MaterialCreationRequest,
) -> MaterialCreationResponse:
    """규칙 및 LLM 검증을 사용하여 새로운 재료를 합성합니다."""
    try:
        with get_db_session() as db:
            response = agent.synthesize(db, request)
            return response
    except Exception as exc:
        logger.error("Error in create_material_experiment: %s", exc)
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to process synthesis: {exc}",
        )


@router.get("/materials/{material_id}/visual-status")
def get_material_visual_status(material_id: str) -> dict[str, Any]:
    """생성된 재료의 시각적 자산에 대한 상태 및 자산 키(Asset keys)를 조회합니다."""
    with get_db_session() as db:
        stmt = select(
            GeneratedMaterialModel.id,
            GeneratedMaterialModel.visual_status,
            GeneratedMaterialModel.visual_asset_key,
            GeneratedMaterialModel.texture_asset_key,
            GeneratedMaterialModel.thumbnail_asset_key,
        ).where(
            GeneratedMaterialModel.id == material_id
        )
        material = db.execute(stmt).one_or_none()

        if not material:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail="Material not found",
            )

        return {
            "material_id": material.id,
            "visual_status": material.visual_status,
            "visual_asset_key": material.visual_asset_key,
            "texture_asset_key": material.texture_asset_key,
            "thumbnail_asset_key": material.thumbnail_asset_key,
        }
