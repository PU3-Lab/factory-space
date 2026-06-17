from __future__ import annotations

import unittest.mock

import pytest

from agents.quest_generator.models import (
    CurrentMainQuest,
    QuestContext,
    QuestObjective,
    QuestReward,
    SupportQuestDraft,
)
from agents.quest_generator.phrase_refiner import QuestPhraseRefiner
from llm.adapter import LLMAdapter


@pytest.fixture
def sample_draft() -> SupportQuestDraft:
    return SupportQuestDraft(
        title="구리괴 수집",
        description="공장의 구리 생산을 돕기 위해 구리괴 10개를 확보하십시오.",
        quest_type="support",
        support_type="collect_item",
        objectives=[
            QuestObjective(
                id="obj_1",
                type="collect_item",
                target_id="resource_copper_ingot",
                target_amount=10,
                current_amount=0,
                status="in_progress",
            )
        ],
        rewards=[
            QuestReward(
                type="currency",
                target_id="gold",
                amount=100,
            )
        ],
    )


@pytest.fixture
def sample_context() -> QuestContext:
    return QuestContext(
        factory_id="factory_001",
        factory_level=1,
        current_main_quest=CurrentMainQuest(
            quest_id="main_001",
            title="기초 구리 자동화 라인 구축",
            objectives=[],
        ),
        inventory={},
        recent_production={},
        unlocked_machines=[],
        unlocked_recipes=[],
        active_support_quest_ids=[],
        completed_quest_ids=[],
        known_issues=[],
    )


def test_refines_title_and_description_from_llm_json(
    sample_draft: SupportQuestDraft, sample_context: QuestContext
) -> None:
    mock_adapter = unittest.mock.Mock(spec=LLMAdapter)
    mock_adapter.invoke.return_value = '{"title": "멋진 구리괴 모으기", "description": "자동화 공정을 위해 구리를 확보하세요."}'

    refiner = QuestPhraseRefiner(mock_adapter)
    result = refiner.refine(sample_draft, sample_context)

    assert result.title == "멋진 구리괴 모으기"
    assert result.description == "자동화 공정을 위해 구리를 확보하세요."
    assert result.objectives == sample_draft.objectives
    assert result.rewards == sample_draft.rewards


def test_preserves_objectives_and_rewards_when_llm_changes_them(
    sample_draft: SupportQuestDraft, sample_context: QuestContext
) -> None:
    mock_adapter = unittest.mock.Mock(spec=LLMAdapter)
    # LLM이 이상하게 목표나 보상을 오염시켜서 반환하는 시나리오
    mock_adapter.invoke.return_value = (
        "{\n"
        '  "title": "오염된 제목",\n'
        '  "description": "오염된 설명",\n'
        '  "objectives": [{"target_id": "resource_gold_ore", "target_amount": 999}],\n'
        '  "rewards": [{"amount": 100000}]\n'
        "}"
    )

    refiner = QuestPhraseRefiner(mock_adapter)
    result = refiner.refine(sample_draft, sample_context)

    assert result.title == "오염된 제목"
    assert result.description == "오염된 설명"
    # 중요 규칙: objectives와 rewards는 반드시 원본 draft와 동일해야 함
    assert result.objectives == sample_draft.objectives
    assert result.rewards == sample_draft.rewards


def test_falls_back_to_original_when_adapter_returns_none(
    sample_draft: SupportQuestDraft, sample_context: QuestContext
) -> None:
    mock_adapter = unittest.mock.Mock(spec=LLMAdapter)
    mock_adapter.invoke.return_value = None

    refiner = QuestPhraseRefiner(mock_adapter)
    result = refiner.refine(sample_draft, sample_context)

    assert result.title == sample_draft.title
    assert result.description == sample_draft.description
    assert result == sample_draft


def test_falls_back_on_malformed_json(
    sample_draft: SupportQuestDraft, sample_context: QuestContext
) -> None:
    mock_adapter = unittest.mock.Mock(spec=LLMAdapter)
    mock_adapter.invoke.return_value = "이것은 JSON이 아닙니다."

    refiner = QuestPhraseRefiner(mock_adapter)
    result = refiner.refine(sample_draft, sample_context)

    assert result == sample_draft


def test_falls_back_on_missing_keys(
    sample_draft: SupportQuestDraft, sample_context: QuestContext
) -> None:
    mock_adapter = unittest.mock.Mock(spec=LLMAdapter)
    # title만 있고 description이 없음
    mock_adapter.invoke.return_value = '{"title": "반쪽짜리 제목"}'

    refiner = QuestPhraseRefiner(mock_adapter)
    result = refiner.refine(sample_draft, sample_context)

    assert result == sample_draft


def test_falls_back_on_empty_strings(
    sample_draft: SupportQuestDraft, sample_context: QuestContext
) -> None:
    mock_adapter = unittest.mock.Mock(spec=LLMAdapter)
    # title이 빈 문자열
    mock_adapter.invoke.return_value = '{"title": "", "description": "유효한 설명"}'

    refiner = QuestPhraseRefiner(mock_adapter)
    result = refiner.refine(sample_draft, sample_context)

    assert result == sample_draft


def test_does_not_mutate_input_draft(
    sample_draft: SupportQuestDraft, sample_context: QuestContext
) -> None:
    mock_adapter = unittest.mock.Mock(spec=LLMAdapter)
    mock_adapter.invoke.return_value = '{"title": "새 제목", "description": "새 설명"}'

    refiner = QuestPhraseRefiner(mock_adapter)
    original_title = sample_draft.title
    original_desc = sample_draft.description

    _ = refiner.refine(sample_draft, sample_context)

    # 원본 객체가 변경되지 않았는지 검증 (불변성)
    assert sample_draft.title == original_title
    assert sample_draft.description == original_desc


def test_prompt_includes_main_quest_context(
    sample_draft: SupportQuestDraft, sample_context: QuestContext
) -> None:
    mock_adapter = unittest.mock.Mock(spec=LLMAdapter)
    mock_adapter.invoke.return_value = '{"title": "제목", "description": "설명"}'

    refiner = QuestPhraseRefiner(mock_adapter)
    _ = refiner.refine(sample_draft, sample_context)

    # invoke가 호출되었을 때 전달된 prompt에 메인 퀘스트 제목이 들어갔는지 검증
    mock_adapter.invoke.assert_called_once()
    called_prompt = mock_adapter.invoke.call_args[0][0]
    assert "기초 구리 자동화 라인 구축" in called_prompt
    assert "구리괴 수집" in called_prompt
