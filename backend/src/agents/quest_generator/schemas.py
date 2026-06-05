"""퀘스트 응답 데이터가 올바른 모양인지 검사하는 Pydantic 모델들입니다."""

from __future__ import annotations

from typing import Literal

from pydantic import BaseModel, Field


class QuestObjective(BaseModel):
    """퀘스트 안에서 플레이어가 달성해야 하는 하나의 수량 목표입니다."""

    target_item_id: str = Field(min_length=1)
    quantity: int = Field(gt=0)


class Quest(BaseModel):
    """클라이언트로 보낼 퀘스트 한 개의 전체 구조를 정의합니다."""

    id: int = Field(gt=0)
    type: Literal["production", "tutorial", "exploration", "economy"]
    title: str = Field(min_length=1)
    description: str = Field(min_length=1)
    objectives: list[QuestObjective] = Field(min_length=1)


class QuestResponse(BaseModel):
    """클라이언트로 보낼 여러 개의 퀘스트 응답 묶음입니다."""

    quests: list[Quest] = Field(min_length=1)
