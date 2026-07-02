"""Tests for the LLM-assisted Intent Classifier (Sprint 19)."""

from __future__ import annotations

import pytest
from agents.operator_guide.csv_repository import CsvManualQARepository
from agents.operator_guide.question_classifier import (
    ManualQAQuestionClassifier,
    LLMIntentClassifier,
)
from agents.operator_guide.schemas import ManualQAIntent


class MockLLMAdapter:
    """모의(Mock) LLM 아답터.

    지정된 응답 문자열을 반환하거나, 에러를 시뮬레이션할 수 있습니다.
    """

    def __init__(self, response_text: str | None = None, error: Exception | None = None) -> None:
        self.response_text = response_text
        self.error = error
        self.invoked_prompts: list[str] = []

    def invoke(self, prompt: str) -> str | None:
        self.invoked_prompts.append(prompt)
        if self.error is not None:
            raise self.error
        return self.response_text

    def invoke_messages(self, messages: list[dict[str, str]]) -> str | None:
        return None


@pytest.fixture
def repo() -> CsvManualQARepository:
    return CsvManualQARepository()


@pytest.fixture
def rule_classifier(repo: CsvManualQARepository) -> ManualQAQuestionClassifier:
    return ManualQAQuestionClassifier(repo)


def test_llm_intent_classifier_success(repo: CsvManualQARepository, rule_classifier: ManualQAQuestionClassifier) -> None:
    # 1. 룰 분류기로부터 모호한 인텐트 획득
    question = "통신탑 준비하려면 뭐가 필요해?"
    rule_intent = rule_classifier.classify(question)
    assert rule_intent.is_ambiguous is True

    # 2. Mock LLM이 성공적으로 JSON을 반환하는 케이스
    mock_response = """
    {
      "question_type": "resource_question",
      "target_ids": ["resource_TeleCommunicationTower", "recipe_make_telecommunication_tower"],
      "confidence": "high",
      "reason": "질문이 통신탑의 재료 수급 및 제작법을 묻고 있습니다."
    }
    """
    mock_adapter = MockLLMAdapter(response_text=mock_response)
    classifier = LLMIntentClassifier(mock_adapter, repo)

    corrected_intent = classifier.classify_ambiguous(question, rule_intent)

    # 검증: 의도가 resource_question으로 보정되고, target_ids가 필터링되었으며, is_ambiguous가 해결됨
    assert corrected_intent.question_type == "resource_question"
    assert corrected_intent.is_ambiguous is False
    assert "resource_TeleCommunicationTower" in corrected_intent.target_ids
    assert "recipe_make_telecommunication_tower" in corrected_intent.target_ids
    assert len(mock_adapter.invoked_prompts) == 1


def test_llm_intent_classifier_json_parse_failure_fallback(repo: CsvManualQARepository, rule_classifier: ManualQAQuestionClassifier) -> None:
    question = "통신탑 알려줘"
    rule_intent = rule_classifier.classify(question)
    assert rule_intent.is_ambiguous is True

    # JSON이 아닌 이상한 텍스트가 오는 경우
    mock_adapter = MockLLMAdapter(response_text="Error: Cannot classify this question.")
    classifier = LLMIntentClassifier(mock_adapter, repo)

    corrected_intent = classifier.classify_ambiguous(question, rule_intent)

    # 검증: 원래 룰 기반 인텐트로 폴백
    assert corrected_intent.question_type == rule_intent.question_type
    assert corrected_intent.is_ambiguous is True
    assert corrected_intent.target_ids == rule_intent.target_ids


def test_llm_intent_classifier_disallowed_question_type_fallback(repo: CsvManualQARepository, rule_classifier: ManualQAQuestionClassifier) -> None:
    question = "통신탑"
    rule_intent = rule_classifier.classify(question)
    assert rule_intent.is_ambiguous is True

    # 허용되지 않은 question_type 반환
    mock_response = """
    {
      "question_type": "super_fancy_invalid_question_type",
      "target_ids": ["resource_TeleCommunicationTower"],
      "confidence": "high",
      "reason": "Invalid type"
    }
    """
    mock_adapter = MockLLMAdapter(response_text=mock_response)
    classifier = LLMIntentClassifier(mock_adapter, repo)

    corrected_intent = classifier.classify_ambiguous(question, rule_intent)

    # 원래 인텐트로 폴백
    assert corrected_intent.question_type == rule_intent.question_type
    assert corrected_intent.is_ambiguous is True


def test_llm_intent_classifier_non_existent_target_ids_fallback(repo: CsvManualQARepository, rule_classifier: ManualQAQuestionClassifier) -> None:
    question = "통신탑은 어떻게 써?"
    rule_intent = rule_classifier.classify(question)
    assert rule_intent.is_ambiguous is True

    # 존재하지 않는 target_ids를 리턴하는 경우
    mock_response = """
    {
      "question_type": "equipment_question",
      "target_ids": ["non_existent_id_xyz"],
      "confidence": "high",
      "reason": "This target does not exist"
    }
    """
    mock_adapter = MockLLMAdapter(response_text=mock_response)
    classifier = LLMIntentClassifier(mock_adapter, repo)

    corrected_intent = classifier.classify_ambiguous(question, rule_intent)

    # 원래 인텐트로 폴백
    assert corrected_intent.question_type == rule_intent.question_type
    assert corrected_intent.is_ambiguous is True


def test_llm_intent_classifier_exception_fallback(repo: CsvManualQARepository, rule_classifier: ManualQAQuestionClassifier) -> None:
    question = "통신탑 준비하려면 뭐가 필요해?"
    rule_intent = rule_classifier.classify(question)
    assert rule_intent.is_ambiguous is True

    # 호출 중 에러 발생
    mock_adapter = MockLLMAdapter(error=RuntimeError("API quota exceeded or Timeout"))
    classifier = LLMIntentClassifier(mock_adapter, repo)

    corrected_intent = classifier.classify_ambiguous(question, rule_intent)

    # 원래 인텐트로 폴백
    assert corrected_intent.question_type == rule_intent.question_type
    assert corrected_intent.is_ambiguous is True
