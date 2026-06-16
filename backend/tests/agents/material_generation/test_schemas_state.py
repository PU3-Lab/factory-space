"""state 필드 스키마 단위 테스트입니다.

이 테스트는 Pydantic 스키마인 MaterialProposalResult와 MaterialCreationResponse에
새롭게 추가된 state(물리적 상태) 필드가 기본값(solid) 및 데이터 유효성 검증을
올바르게 통과하는지 확인합니다.
"""

from __future__ import annotations

from agents.material_generation.schemas import (
    MaterialCreationResponse,
    MaterialProperties,
    MaterialProposalResult,
)


def test_proposal_result_state_defaults_solid() -> None:
    """MaterialProposalResult 생성 시 state 필드가 제공되지 않으면 기본값인 'solid'로 설정되는지 테스트합니다."""
    result = MaterialProposalResult(
        id_hint="x",
        name="테스트물질",
        category="alloy",
        rarity="common",
        description="d",
        properties=MaterialProperties(
            strength=1.0, conductivity=1.0, stability=1.0, reactivity=1.0
        ),
        visual_prompt="p",
    )
    assert result.state == "solid"


def test_response_state_optional() -> None:
    """MaterialCreationResponse 생성 시 state 필드가 기본적으로 None 상태가 될 수 있는지 테스트합니다."""
    resp = MaterialCreationResponse(result_type="new_material", experiment_hash="h")
    assert resp.state is None
