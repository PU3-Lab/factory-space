"""MaterialResultValidator(이름/id_hint 검증) 단위 테스트입니다.

이 테스트는 LLM이 제안한 재료의 명칭(name)과 ID 힌트(id_hint)에 포함된 금지어 정화(sanitizing),
비허용 특수문자 제거, 길이 초과시 트림(trim) 처리, 그리고 추천 레시피 후보군 개수 제한(최대 5개)이
올바르게 작동하는지 확인합니다.
"""

from __future__ import annotations

from agents.material_generation.result_validator import (
    FALLBACK_NAME,
    MaterialResultValidator,
)
from agents.material_generation.schemas import (
    MaterialProperties,
    MaterialProposal,
    MaterialProposalResult,
)


def _make(
    name: str,
    id_hint: str = "alloy_x",
    candidates: list[str] | None = None,
) -> MaterialProposal:
    """테스트용 MaterialProposal 객체를 생성하는 헬퍼 함수입니다."""
    return MaterialProposal(
        proposal_type="new_material",
        confidence=0.9,
        reason="test",
        result=MaterialProposalResult(
            id_hint=id_hint,
            name=name,
            category="alloy",
            rarity="common",
            description="d",
            properties=MaterialProperties(
                strength=5.0, conductivity=5.0, stability=5.0, reactivity=5.0
            ),
            next_recipe_candidates=candidates or [],
            visual_prompt="p",
        ),
    )


def test_sanitize_strips_forbidden_keywords() -> None:
    """이름에 금지 키워드(trash, invalid 등)가 포함되어 있을 때 적절한 단어로 정화되는지 테스트합니다."""
    out = MaterialResultValidator.validate_and_correct(_make("Invalid Trash 합금"))
    assert out.result is not None
    assert "trash" not in out.result.name.lower()
    assert "invalid" not in out.result.name.lower()


def test_sanitize_removes_special_chars() -> None:
    """이름에 비허용 특수문자가 섞여 있을 때 정상 제거되는지 테스트합니다."""
    out = MaterialResultValidator.validate_and_correct(_make("초강 #합금@! 정"))
    assert out.result is not None
    assert "#" not in out.result.name
    assert "@" not in out.result.name
    assert "!" not in out.result.name


def test_empty_name_falls_back() -> None:
    """이름이 공백일 경우 기본 대체 이름(FALLBACK_NAME)으로 변환되는지 테스트합니다."""
    out = MaterialResultValidator.validate_and_correct(_make("   "))
    assert out.result is not None
    assert out.result.name == FALLBACK_NAME


def test_symbol_only_name_falls_back() -> None:
    """이름에 특수문자만 존재하여 정화 후 빈 문자열이 될 때 대체 이름으로 처리되는지 테스트합니다."""
    out = MaterialResultValidator.validate_and_correct(_make("###@@@"))
    assert out.result is not None
    assert out.result.name == FALLBACK_NAME


def test_too_long_name_is_truncated() -> None:
    """이름이 너무 길 경우 최대 허용 길이(24자)로 트림되는지 테스트합니다."""
    out = MaterialResultValidator.validate_and_correct(_make("가" * 40))
    assert out.result is not None
    assert len(out.result.name) == 24


def test_id_hint_sanitized() -> None:
    """ID 힌트 문자열에서 대문자, 금지어, 비허용 문자가 정상 규격(소문자/숫자/언더스코어)으로 변환되는지 테스트합니다."""
    out = MaterialResultValidator.validate_and_correct(
        _make("정상물질", id_hint="alloy-Test!! Dummy")
    )
    assert out.result is not None
    assert out.result.id_hint == "alloy_alloy_alloy"


def test_candidates_capped_at_five() -> None:
    """다음 레시피 후보군 리스트의 크기가 최대 5개로 제한되는지 테스트합니다."""
    out = MaterialResultValidator.validate_and_correct(
        _make("정상물질", candidates=["c1", "c2", "c3", "c4", "c5", "c6", "c7"])
    )
    assert out.result is not None
    assert out.result.next_recipe_candidates == ["c1", "c2", "c3", "c4", "c5"]


def test_sanitize_does_not_strip_substring_keywords() -> None:
    """단어 내부에 금지 키워드가 부분 문자열로 섞여 있을 때 치환되지 않는지 테스트합니다."""
    out = MaterialResultValidator.validate_and_correct(
        _make("Latest Protest", id_hint="latest_protest")
    )
    assert out.result is not None
    assert out.result.name == "Latest Protest"
    assert out.result.id_hint == "latest_protest"


def test_sanitize_forbidden_keywords_on_boundaries() -> None:
    """언더스코어로 연결된 id_hint나 한글에 접한 금지 키워드가 단어 경계 문제 없이 정상 정화되는지 테스트합니다."""
    out1 = MaterialResultValidator.validate_and_correct(
        _make("정상물질", id_hint="dummy_alloy")
    )
    assert out1.result is not None
    assert out1.result.id_hint == "alloy_alloy"

    out2 = MaterialResultValidator.validate_and_correct(_make("철test합금"))
    assert out2.result is not None
    assert out2.result.name == "철합금합금"
