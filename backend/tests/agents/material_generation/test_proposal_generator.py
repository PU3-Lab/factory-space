"""MaterialProposalGenerator 프롬프트·fallback 단위 테스트입니다.

이 테스트는 LLM 프롬프트 생성기인 MaterialProposalGenerator가
물리적 상태 및 카테고리에 대한 가이드를 프롬프트에 정상 포함시키는지,
또한 LLM 호출 실패 시 반환되는 Fallback 물질 정보에 state 필드가
올바르게 들어가는지 확인합니다.
"""

from __future__ import annotations

from agents.material_generation.proposal_generator import MaterialProposalGenerator
from agents.material_generation.schemas import ProcessConditionsSchema


def test_prompt_includes_state_and_naming_guide() -> None:
    """프롬프트 조립 시 물리적 상태(state) 정보와 화학 물질 스타일 명명 가이드가 올바르게 들어가는지 테스트합니다."""
    gen = MaterialProposalGenerator()
    prompt = gen._build_prompt(
        machine_type="Synthesizer",
        normalized_inputs=[{"item_id": "iron_ingot", "qty": 1}],
        process_conditions=ProcessConditionsSchema(),
        similar_experiments=[],
        derived_state="liquid",
        derived_category="alloy",
    )
    assert "liquid" in prompt
    assert "화학" in prompt  # 화학 물질 스타일 명명 가이드


def test_fallback_proposal_has_state() -> None:
    """LLM 에러 등으로 Fallback 처리가 될 때, 생성된 Fallback Proposal의 결과에 state 필드가 'solid'로 존재하는지 테스트합니다."""
    gen = MaterialProposalGenerator()
    proposal = gen.get_fallback_proposal([{"item_id": "iron_ingot", "qty": 1}])
    assert proposal.result is not None
    assert proposal.result.state == "solid"
