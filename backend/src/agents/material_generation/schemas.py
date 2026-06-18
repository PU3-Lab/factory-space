"""material_generation 에이전트를 위한 Pydantic 스키마 정의입니다."""

from __future__ import annotations

from pydantic import BaseModel, Field


class InputItemSchema(BaseModel):
    """입력 아이템 스펙입니다."""

    item_id: str
    qty: int


class ProcessConditionsSchema(BaseModel):
    """합성을 위한 선택적 환경 공정 조건입니다."""

    temperature: str = "default"
    pressure: str = "default"
    catalyst: str | None = None


class MaterialCreationRequest(BaseModel):
    """재료 합성을 요청하는 수신 요청 스키마입니다."""

    machine_type: str
    inputs: list[InputItemSchema]
    process_conditions: ProcessConditionsSchema = Field(
        default_factory=ProcessConditionsSchema
    )
    player_id: str
    generate_visual_asset: bool = True


class MaterialProperties(BaseModel):
    """재료의 물리적 속성을 정의하는 특성들입니다."""

    strength: float = 5.0
    conductivity: float = 5.0
    stability: float = 5.0
    reactivity: float = 2.0


class MaterialProposalResult(BaseModel):
    """LLM에 의해 제안된 새로운 재료의 속성 정의입니다."""

    id_hint: str
    name: str
    category: str = "alloy"
    state: str = "solid"
    rarity: str = "common"
    description: str
    properties: MaterialProperties = Field(default_factory=MaterialProperties)
    risks: list[str] = Field(default_factory=list)
    usage: list[str] = Field(default_factory=list)
    next_recipe_candidates: list[str] = Field(default_factory=list)
    visual_prompt: str


class MaterialProposal(BaseModel):
    """LLM의 분류 및 제안 세부 정보를 캡슐화하는 래퍼(Wrapper) 스키마입니다."""

    proposal_type: str
    confidence: float
    reason: str
    result: MaterialProposalResult | None = None


class OutputItemSchema(BaseModel):
    """출력 아이템 스펙입니다."""

    item_id: str
    qty: int


class MaterialCreationResponse(BaseModel):
    """사용자에게 반환되는 최종 결과 스키마입니다."""

    result_type: str  # "existing_recipe", "new_material", "cached_experiment", "failed_result", "invalid_input"
    experiment_hash: str

    recipe_name: str | None = None
    outputs: list[OutputItemSchema] | None = None

    material_id: str | None = None
    material_hash: str | None = None
    name: str | None = None
    rarity: str | None = None
    state: str | None = None
    generation_status: str | None = None
    visual_status: str | None = None
    fallback_icon: str | None = None
    visual_asset_key: str | None = None  # 비주얼 이미지 에셋 키 (materials/{material_id}/icon.png 형식)
    texture_asset_key: str | None = None  # 텍스처 에셋 키 (materials/{material_id}/texture.png 형식)
    thumbnail_asset_key: str | None = None  # 썸네일 에셋 키 (materials/{material_id}/thumbnail.png 형식)
    message: str | None = None

    cached: bool | None = None
    failure_reason: str | None = None
