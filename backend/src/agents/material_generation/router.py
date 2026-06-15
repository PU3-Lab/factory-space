"""API Router for material generation agent experiments."""

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
    """Synthesize a new material using rules and LLM validation."""
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
    """Retrieve the status and asset keys of a generated material's visual assets."""
    with get_db_session() as db:
        stmt = select(GeneratedMaterialModel).where(
            GeneratedMaterialModel.id == material_id
        )
        material = db.execute(stmt).scalar_one_or_none()

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
