"""Tests for operator_guide ambiguity detection (Sprint 18)."""

from __future__ import annotations

import pytest
from agents.operator_guide.csv_repository import CsvManualQARepository
from agents.operator_guide.question_classifier import ManualQAQuestionClassifier


@pytest.fixture
def classifier() -> ManualQAQuestionClassifier:
    repo = CsvManualQARepository()
    return ManualQAQuestionClassifier(repo)


def test_ambiguity_detection_unambiguous_telecommunication_tower(
    classifier: ManualQAQuestionClassifier,
) -> None:
    # 명확한 제작법/설계 정보 요청은 ambiguous로 판단하지 않습니다.
    intent = classifier.classify("통신탑 어떻게 만들어?")
    assert intent.is_ambiguous is False
    assert intent.question_type == "resource_question"

    intent = classifier.classify("통신탑 어떻게 지어야 해?")
    assert intent.is_ambiguous is False
    assert intent.question_type == "resource_question"


@pytest.mark.parametrize(
    "question",
    [
        "통신탑 알려줘",
        "통신탑은 어떻게 써?",
        "통신탑 준비하려면 뭐가 필요해?",
    ],
)
def test_ambiguity_detection_ambiguous_telecommunication_tower(
    classifier: ManualQAQuestionClassifier,
    question: str,
) -> None:
    # 의도가 불명확하거나 장비/자원 해석이 중복되는 경우 ambiguous로 판단합니다.
    intent = classifier.classify(question)
    assert intent.is_ambiguous is True
    assert "equipment_telecommunication_tower" in intent.target_ids


def test_ambiguity_detection_unknown_with_csv_candidate(
    classifier: ManualQAQuestionClassifier,
) -> None:
    # 제련기 단독 질문은 장비 설명인지 다른 용도인지 명확하지 않아 ambiguous 처리됩니다.
    intent = classifier.classify("제련기")
    assert intent.is_ambiguous is True
    assert intent.question_type == "unknown_question"
    assert "equipment_smelter" in intent.target_ids


def test_ambiguity_detection_with_context_target(
    classifier: ManualQAQuestionClassifier,
) -> None:
    # context에 selectedMachine 정보가 있고 의도가 불명확할 때 ambiguous로 처리됩니다.
    context = {"current_game_state": {"selectedMachine": "equipment_synthesizer"}}
    
    # context 없이 질문 시에는 CSV 후보를 매칭하지 못해 ambiguous가 아닙니다.
    intent_no_context = classifier.classify("이거 설치하려면 뭐 해야 해?")
    assert intent_no_context.is_ambiguous is False

    # context가 주어지면 selectedMachine을 CSV 후보로 식별하여 ambiguous로 감지합니다.
    intent_with_context = classifier.classify("이거 설치하려면 뭐 해야 해?", context=context)
    assert intent_with_context.is_ambiguous is True
