"""Schemas for the CSV-backed Manual Q&A proto.

이 모듈은 Manual Q&A 에이전트에서 사용하는 데이터 검증 및 구조 정의를 담당합니다.
Pydantic을 사용하여 입력 페이로드와 내부 처리 객체, 최종 응답 형태를 강제합니다.
"""

from __future__ import annotations

from typing import Literal

from pydantic import BaseModel, ConfigDict, Field, computed_field

QuestionType = Literal[
    "equipment_question",
    "resource_question",
    "recipe_question",
    "troubleshooting_question",
    "unknown_question",
]


class QAChatbotPayload(BaseModel):
    """Unreal 등 외부 클라이언트에서 보내오는 챗봇 질문 요청 페이로드입니다.

    question: 유저가 입력한 질문 텍스트
    context: 게임의 현재 세션 정보나 추가 컨텍스트 딕셔너리
    """

    model_config = ConfigDict(extra="forbid")

    question: str = Field(min_length=1)
    context: dict[str, object] = Field(default_factory=dict)


class ManualQAIntent(BaseModel):
    """질문 의도 분류(Question Classification) 결과를 정의하는 모델입니다.

    question_type: 질문 카테고리 (장비, 리소스, 제작법, 문제해결 등)
    primary_manual: 검색할 주 매뉴얼 파일 이름
    supporting_manuals: 보조로 검색할 매뉴얼 파일 이름들
    target_ids: 질문과 직접 연관된 장비/아이템 ID 목록
    """

    model_config = ConfigDict(extra="forbid")

    question_type: QuestionType
    primary_manual: str
    supporting_manuals: list[str] = Field(default_factory=list)
    target_ids: list[str] = Field(default_factory=list)


class ManualQASource(BaseModel):
    """답변의 근거가 된 매뉴얼의 소스 행(row) 메타데이터입니다.

    doc_id: 문서 ID
    type: 문서 타입 (예: equipment, recipe 등)
    title: 문서 제목
    """

    model_config = ConfigDict(extra="forbid")

    doc_id: str
    type: str
    title: str


class RecommendedAction(BaseModel):
    """클라이언트(Unreal Engine 등) UI에 추천 동작으로 표시할 추천 액션입니다.

    action_id: 추천 액션 고유 식별자
    label: 사용자에게 보여줄 버튼 라벨
    description: 액션에 대한 설명
    priority: 추천 우선순위 (낮을수록 우선됨)
    """

    model_config = ConfigDict(extra="forbid")

    action_id: str
    label: str
    description: str
    priority: int


class ManualQAResult(BaseModel):
    """Manual Q&A 서비스가 LLM 및 RAG 검색, Game State Tool을 연동한 뒤 반환하는 최종 결과 모델입니다.

    answer: LLM이 생성한 최종 답변
    sources: 답변 생성에 참고한 RAG 소스 문서 목록
    recommended_actions: 사용자 추천 액션 목록
    confidence: 답변 신뢰도 (high, medium, low)
    requires_current_game_state: 현재 게임 상태 도구 호출이 필요한 질문인지 여부
    used_current_game_state: 실제로 게임 상태 정보를 조회하여 답변에 활용했는지 여부
    required_state_scopes: 분석을 위해 필요했던 게임 상태 데이터 범위 (예: selectedMachine, inputInventory 등)
    available_scopes: 클라이언트에서 실제로 제공하여 조회 가능했던 상태 범위 목록
    """

    model_config = ConfigDict(extra="forbid")

    question: str
    question_type: QuestionType
    answer: str
    sources: list[ManualQASource] = Field(default_factory=list)
    recommended_actions: list[RecommendedAction] = Field(default_factory=list)
    confidence: Literal["high", "medium", "low"]
    primary_manual: str
    supporting_manuals: list[str] = Field(default_factory=list)
    target_ids: list[str] = Field(default_factory=list)
    retrieval: dict[str, object] = Field(default_factory=dict)
    requires_current_game_state: bool = False
    used_current_game_state: bool = False
    required_state_scopes: list[str] = Field(default_factory=list)
    available_scopes: list[str] = Field(default_factory=list)

    @computed_field
    @property
    def final_answer(self) -> str:
        """이전 버전 클라이언트와의 호환성을 위한 최종 답변 텍스트 필드입니다."""

        return self.answer

    def to_metadata(self) -> dict[str, object]:
        """클라이언트로 전송하기 위해 카멜케이스(CamelCase) 형식의 키를 포함하는 메타데이터 딕셔너리를 반환합니다."""

        return {
            "question": self.question,
            "question_type": self.question_type,
            "sources": [source.model_dump() for source in self.sources],
            "recommended_actions": [
                action.model_dump() for action in self.recommended_actions
            ],
            "confidence": self.confidence,
            "primary_manual": self.primary_manual,
            "supporting_manuals": self.supporting_manuals,
            "target_ids": self.target_ids,
            "retrieval": self.retrieval,
            "requiresCurrentGameState": self.requires_current_game_state,
            "usedCurrentGameState": self.used_current_game_state,
            "requiredStateScopes": self.required_state_scopes,
            "availableScopes": self.available_scopes,
        }
