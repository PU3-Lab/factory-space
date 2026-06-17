"""Pydantic schemas for the Quest Generator agent."""

from __future__ import annotations

from datetime import datetime
from typing import Literal

from pydantic import BaseModel, ConfigDict, Field


class QuestObjective(BaseModel):
    """퀘스트 안에서 플레이어가 달성해야 하는 하나의 수량 목표입니다."""

    model_config = ConfigDict(extra="forbid")

    id: str = Field(description="목표 식별자 (instance 비의존 스킴: obj_{uuid})")
    type: Literal["collect_item"] = Field(
        default="collect_item", description="목표 유형 (MVP: collect_item 고정)"
    )
    target_id: str = Field(
        description="대상 아이템 ID (resources.csv의 resource_* 형태)"
    )
    target_amount: int = Field(gt=0, description="목표 수량")
    current_amount: int = Field(default=0, ge=0, description="현재 수량")
    status: Literal["in_progress", "completed"] = Field(
        default="in_progress", description="목표 상태"
    )


class QuestReward(BaseModel):
    """퀘스트 완료 시 플레이어에게 지급되는 보상 정보입니다."""

    model_config = ConfigDict(extra="forbid")

    type: Literal["currency"] = Field(
        default="currency", description="보상 유형 (MVP: currency 고정)"
    )
    target_id: str = Field(default="gold", description="보상 대상 (예: gold)")
    amount: int = Field(gt=0, description="보상 수량")


class KnownIssue(BaseModel):
    """메인 퀘스트 진행을 위해 해결해야 하는 부족한 자원 이슈입니다."""

    model_config = ConfigDict(extra="forbid")

    item_id: str = Field(description="부족한 아이템 ID")
    shortage_amount: int = Field(gt=0, description="부족한 수량")
    main_objective_id: str = Field(description="해당 메인 퀘스트의 목표 ID")
    producible: bool = Field(description="해당 아이템을 생산할 레시피 해금 여부")


class MainQuestObjective(BaseModel):
    """현재 진행 중인 메인 퀘스트의 개별 목표입니다."""

    model_config = ConfigDict(extra="forbid")

    main_objective_id: str = Field(description="메인 퀘스트 목표 식별자")
    objective_type: str = Field(description="목표 유형 (예: collect_item)")
    item_id: str = Field(description="대상 아이템 ID")
    required: int = Field(gt=0, description="요구 수량")
    current: int = Field(ge=0, description="현재 수량")


class CurrentMainQuest(BaseModel):
    """현재 진행 중인 메인 퀘스트 정보입니다."""

    model_config = ConfigDict(extra="forbid")

    quest_id: str = Field(description="메인 퀘스트 식별자")
    title: str = Field(description="메인 퀘스트 제목")
    objectives: list[MainQuestObjective] = Field(description="메인 퀘스트 목표 목록")


class QuestContext(BaseModel):
    """클라이언트(Unreal)로부터 전달된 공장 상태 스냅샷 정보입니다."""

    model_config = ConfigDict(extra="forbid")

    factory_id: str = Field(description="공장 식별자")
    factory_level: int = Field(ge=1, description="공장 레벨")
    current_main_quest: CurrentMainQuest | None = Field(
        default=None, description="현재 진행 중인 메인 퀘스트"
    )
    inventory: dict[str, int] = Field(description="인벤토리 아이템 수량 매핑")
    recent_production: dict[str, int] = Field(
        default_factory=dict, description="최근 생산한 아이템 수량 매핑"
    )
    unlocked_machines: list[str] = Field(
        default_factory=list, description="해금된 기계 목록"
    )
    unlocked_recipes: list[str] = Field(
        default_factory=list, description="해금된 레시피 목록"
    )
    active_support_quest_ids: list[str] = Field(
        default_factory=list, description="현재 진행 중인 지원 퀘스트 ID 목록"
    )
    completed_quest_ids: list[str] = Field(
        default_factory=list, description="완료된 퀘스트 ID 목록"
    )
    known_issues: list[KnownIssue] = Field(
        default_factory=list, description="구조화된 자원 부족 이슈 목록"
    )


class SupportQuestDraft(BaseModel):
    """새로 생성될 지원 퀘스트의 초안 스키마입니다."""

    model_config = ConfigDict(extra="forbid")

    title: str = Field(description="퀘스트 제목")
    description: str = Field(description="퀘스트 설명")
    quest_type: Literal["support"] = Field(default="support", description="퀘스트 유형")
    support_type: Literal["collect_item"] = Field(
        default="collect_item", description="지원 퀘스트 유형"
    )
    objectives: list[QuestObjective] = Field(description="퀘스트 목표 목록")
    rewards: list[QuestReward] = Field(description="퀘스트 보상 목록")


class ValidationResult(BaseModel):
    """퀘스트 생성 초안에 대한 유효성 검사 결과입니다."""

    model_config = ConfigDict(extra="forbid")

    valid: bool = Field(description="유효 여부")
    reason: str | None = Field(default=None, description="실패 코드/원인")
    message: str | None = Field(default=None, description="실패 상세 메시지")


class QuestInstance(BaseModel):
    """데이터베이스에 영속화되는 퀘스트 인스턴스의 Pydantic 표현입니다."""

    model_config = ConfigDict(extra="forbid")

    id: str = Field(description="인스턴스 식별자 (qinst_xxx)")
    factory_id: str = Field(description="공장 식별자")
    quest_type: str = Field(description="퀘스트 유형")
    support_type: str = Field(description="지원 퀘스트 유형")
    related_main_quest_id: str | None = Field(
        default=None, description="관련 메인 퀘스트 식별자"
    )
    title: str = Field(description="퀘스트 제목")
    description: str = Field(description="퀘스트 설명")
    status: Literal["in_progress", "completed", "reward_claimed"] = Field(
        description="퀘스트 인스턴스 상태"
    )
    objectives: list[QuestObjective] = Field(description="퀘스트 목표 목록")
    rewards: list[QuestReward] = Field(description="퀘스트 보상 목록")
    created_by: str = Field(description="생성 주체 (예: rule)")
    level_reward_applied: bool = Field(description="레벨 보상 적용 여부")
    created_at: datetime = Field(description="생성 일시")
    completed_at: datetime | None = Field(default=None, description="완료 일시")


class QuestProgress(BaseModel):
    """퀘스트 개별 목표의 진행도에 대한 Pydantic 표현입니다."""

    model_config = ConfigDict(extra="forbid")

    id: str = Field(description="진행도 식별자 (qprog_xxx)")
    quest_instance_id: str = Field(description="관련 퀘스트 인스턴스 식별자")
    objective_id: str = Field(description="목표 식별자")
    objective_type: str = Field(description="목표 유형")
    target_id: str = Field(description="목표 대상 ID")
    current_amount: int = Field(ge=0, description="현재 누적량")
    target_amount: int = Field(gt=0, description="목표 요구량")
    status: Literal["in_progress", "completed"] = Field(description="목표 진행 상태")
    last_event_id: str | None = Field(
        default=None, description="멱등성 처리를 위한 마지막 처리 이벤트 ID"
    )
