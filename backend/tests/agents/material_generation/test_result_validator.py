"""MaterialResultValidator 클래스에 대한 단위 테스트입니다."""

from __future__ import annotations

from agents.material_generation.result_validator import MaterialResultValidator
from agents.material_generation.schemas import (
    MaterialProperties,
    MaterialProposal,
    MaterialProposalResult,
)


def test_validator_clamps_out_of_bounds_properties() -> None:
    proposal = MaterialProposal(
        proposal_type="new_material",
        confidence=0.9,
        reason="Test properties bounds clamping",
        result=MaterialProposalResult(
            id_hint="heavy_alloy",
            name="Heavy Alloy",
            category="alloy",
            rarity="common",
            description="Strength and stability are way too high, others too low.",
            properties=MaterialProperties(
                strength=15.5,
                conductivity=-1.2,
                stability=10.1,
                reactivity=-0.0,
            ),
            risks=[],
            usage=[],
            next_recipe_candidates=[],
            visual_prompt="heavy alloy ingot",
        ),
    )

    validated = MaterialResultValidator.validate_and_correct(proposal)
    assert validated.result is not None
    p = validated.result.properties
    assert p.strength == 10.0
    assert p.conductivity == 0.0
    assert p.stability == 10.0
    assert p.reactivity == 0.0


def test_validator_defaults_invalid_rarity() -> None:
    proposal = MaterialProposal(
        proposal_type="new_material",
        confidence=0.9,
        reason="Test invalid rarity mapping",
        result=MaterialProposalResult(
            id_hint="alloy_rarity",
            name="Alloy Rarity",
            category="alloy",
            rarity="legendary",  # 유효하지 않은 희귀도
            description="Legendary is not a valid rarity.",
            properties=MaterialProperties(
                strength=5.0, conductivity=5.0, stability=5.0, reactivity=5.0
            ),
            risks=[],
            usage=[],
            next_recipe_candidates=[],
            visual_prompt="alloy ingot",
        ),
    )

    validated = MaterialResultValidator.validate_and_correct(proposal)
    assert validated.result is not None
    assert validated.result.rarity == "common"


def test_validator_cleans_forbidden_keywords() -> None:
    proposal = MaterialProposal(
        proposal_type="new_material",
        confidence=0.9,
        reason="Test forbidden name and id cleaning",
        result=MaterialProposalResult(
            id_hint="alloy_dummy_test_shit",
            name="Invalid Trash Dummy Alloy",
            category="alloy",
            rarity="rare",
            description="Clean properties.",
            properties=MaterialProperties(
                strength=5.0, conductivity=5.0, stability=5.0, reactivity=5.0
            ),
            risks=[],
            usage=[],
            next_recipe_candidates=[],
            visual_prompt="alloy ingot",
        ),
    )

    validated = MaterialResultValidator.validate_and_correct(proposal)
    assert validated.result is not None
    # id_hint 내의 'dummy', 'test', 'shit'이 'alloy'로 대체됨
    assert "dummy" not in validated.result.id_hint
    assert "test" not in validated.result.id_hint
    assert "shit" not in validated.result.id_hint
    # name 내의 'Trash', 'Dummy', 'Invalid'가 'Alloy' 또는 'alloy'로 대체됨
    assert "trash" not in validated.result.name.lower()
    assert "dummy" not in validated.result.name.lower()
    assert "invalid" not in validated.result.name.lower()


def test_validator_limits_recipe_candidates() -> None:
    proposal = MaterialProposal(
        proposal_type="new_material",
        confidence=0.9,
        reason="Test candidates capping",
        result=MaterialProposalResult(
            id_hint="alloy_ingot",
            name="Alloy Ingot",
            category="alloy",
            rarity="epic",
            description="Too many candidate recipe suggestions.",
            properties=MaterialProperties(
                strength=5.0, conductivity=5.0, stability=5.0, reactivity=5.0
            ),
            risks=[],
            usage=[],
            next_recipe_candidates=["c1", "c2", "c3", "c4", "c5", "c6", "c7"],
            visual_prompt="alloy ingot",
        ),
    )

    validated = MaterialResultValidator.validate_and_correct(proposal)
    assert validated.result is not None
    assert len(validated.result.next_recipe_candidates) == 5
    assert validated.result.next_recipe_candidates == ["c1", "c2", "c3", "c4", "c5"]
