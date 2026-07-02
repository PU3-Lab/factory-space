"""Smoke tests for the CSV-backed Manual Q&A Agent proto."""

from __future__ import annotations

import csv
from pathlib import Path
from typing import Any

import pytest

from agents.base import AgentContext
from agents.operator_guide.service import ManualQAService

ROOT = Path(__file__).resolve().parents[2]
GAME_DATA = ROOT / "data" / "game"

ID_COLUMN_ALIASES: dict[str, tuple[str, ...]] = {
    "equipment_id": ("equipment_id", "장비ID"),
    "resource_id": ("resource_id", "자원ID"),
    "recipe_id": ("recipe_id", "레시피ID"),
    "issue_id": ("issue_id", "문제ID"),
    "action_id": ("action_id", "행동ID"),
}


SMOKE_CASES: list[dict[str, Any]] = [
    {
        "question": "제련기는 뭐야?",
        "expected_question_type": "equipment_question",
        "expected_source_ids": ["equipment_smelter"],
        "expected_action_ids": ["action_explain_equipment_role"],
    },
    {
        "question": "철괴는 어떻게 만들어?",
        "expected_question_type": "resource_question",
        "expected_source_ids": ["resource_iron_ingot", "recipe_smelt_iron"],
        "expected_action_ids": ["action_explain_resource_production"],
    },
    {
        "question": "철괴 만들려면 뭐가 필요해?",
        "expected_question_type": "recipe_question",
        "expected_source_ids": ["recipe_smelt_iron", "resource_iron_ore"],
        "expected_action_ids": ["action_explain_recipe_requirements"],
    },
    {
        "question": "제련기가 왜 안 돌아가?",
        "expected_question_type": "troubleshooting_question",
        "expected_source_ids": [
            "issue_machine_stopped",
            "equipment_smelter",
        ],
        "expected_action_ids": [
            "action_check_power",
            "action_check_input_resource",
            "action_check_storage",
        ],
    },
    {
        "question": "우주 엘리베이터는 어떻게 업그레이드해?",
        "expected_question_type": "unknown_question",
        "expected_source_ids": [],
        "expected_action_ids": ["action_answer_unknown_without_guessing"],
    },
]


def csv_ids(filename: str, id_column: str) -> set[str]:
    id_columns = ID_COLUMN_ALIASES.get(id_column, (id_column,))
    with (GAME_DATA / filename).open(encoding="utf-8-sig", newline="") as csv_file:
        return {
            row[column]
            for row in csv.DictReader(csv_file)
            for column in id_columns
            if column in row
        }


def answer_manual_qa(question: str) -> dict[str, Any]:
    _ = AgentContext(
        session_id="smoke-session",
        request_id="smoke-request",
        client_id="smoke-client",
    )
    result = ManualQAService().answer(question)
    metadata = result.to_metadata()
    return {
        "question": metadata["question"],
        "question_type": metadata["question_type"],
        "final_answer": result.final_answer,
        "answer": result.answer,
        "sources": metadata["sources"],
        "recommended_actions": metadata["recommended_actions"],
        "confidence": metadata["confidence"],
        "payload_actions": [],
    }


def test_manual_qa_proto_uses_operator_guide_package_name() -> None:
    import importlib.util

    assert importlib.util.find_spec("agents.operator_guide.service") is not None
    assert (
        importlib.util.find_spec("agents.operator_guide.question_classifier")
        is not None
    )
    assert importlib.util.find_spec("agents.qa_chatbot") is None


def test_expected_source_ids_exist_in_proto_csv_files() -> None:
    known_ids = (
        csv_ids("equipment.csv", "equipment_id")
        | csv_ids("resources.csv", "resource_id")
        | csv_ids("recipes.csv", "recipe_id")
        | csv_ids("troubleshooting_rules.csv", "issue_id")
    )

    for case in SMOKE_CASES:
        assert set(case["expected_source_ids"]) <= known_ids


def test_expected_action_ids_exist_in_action_policy() -> None:
    action_ids = csv_ids("action_policy.csv", "action_id")

    for case in SMOKE_CASES:
        assert set(case["expected_action_ids"]) <= action_ids


@pytest.mark.parametrize("case", SMOKE_CASES)
def test_manual_qa_agent_smoke_contract(case: dict[str, Any]) -> None:
    response = answer_manual_qa(case["question"])

    assert response["question_type"] == case["expected_question_type"]
    assert response["question"] == case["question"]
    assert response["answer"]
    assert response["final_answer"] == response["answer"]
    assert response["payload_actions"] == []

    source_ids = {source["doc_id"] for source in response["sources"]}
    assert set(case["expected_source_ids"]) <= source_ids

    action_ids = {action["action_id"] for action in response["recommended_actions"]}
    assert set(case["expected_action_ids"]) <= action_ids


def test_manual_qa_agent_unknown_question_does_not_hallucinate() -> None:
    response = answer_manual_qa("우주 엘리베이터는 어떻게 업그레이드해?")

    assert response["question_type"] == "unknown_question"
    assert response["sources"] == []
    assert response["confidence"] == "low"
    assert "관련 매뉴얼 근거를 찾지 못했습니다" in response["answer"]


def test_manual_qa_troubleshooting_fallback_keeps_csv_metadata() -> None:
    response = answer_manual_qa("제련기가 왜 안 돌아가?")

    assert response["question_type"] == "troubleshooting_question"
    assert "전력, 입력 자원, 출력 저장 공간" in response["final_answer"]
    assert "먼저 전력 → 입력 자원" in response["final_answer"]
    source_ids = {source["doc_id"] for source in response["sources"]}
    assert {"issue_machine_stopped", "equipment_smelter"} <= source_ids
    action_ids = {action["action_id"] for action in response["recommended_actions"]}
    assert {
        "action_check_power",
        "action_check_input_resource",
        "action_check_storage",
    } <= action_ids


def test_manual_qa_fallback_builds_csv_based_template_answers() -> None:
    equipment_response = answer_manual_qa("제련기는 뭐야?")
    recipe_response = answer_manual_qa("철괴 만들려면 뭐가 필요해?")
    unknown_response = answer_manual_qa("우주 엘리베이터는 어떻게 업그레이드해?")

    assert "제련기는 광석이나 원재료를 가공" in equipment_response["final_answer"]
    assert "대표 출력 자원은 철괴" in equipment_response["final_answer"]
    assert "철괴는 제련기에서 만들 수 있어요" in recipe_response["final_answer"]
    assert "필요 재료는 철광석 2개" in recipe_response["final_answer"]
    assert unknown_response["final_answer"] == (
        "지금은 관련 매뉴얼 근거를 찾지 못했습니다.\n\n"
        "장비 이름이나 자원 이름을 조금 더 구체적으로 말해주면 다시 확인해볼게요."
    )


def test_manual_qa_fallback_explains_telecommunication_tower_recipe() -> None:
    response = answer_manual_qa("통신탑 어떻게 만들어?")

    assert response["question_type"] == "resource_question"
    assert "통신탑은 합성기에서 만들 수 있어요" in response["final_answer"]
    assert "철근 20개" in response["final_answer"]
    assert "구리선 20개" in response["final_answer"]
    assert "주석판 20개" in response["final_answer"]
    assert "resource_" not in response["final_answer"]


def test_manual_qa_answers_use_formal_tone() -> None:
    for case in SMOKE_CASES:
        response = answer_manual_qa(case["question"])

        assert "한다" not in response["final_answer"]


def test_manual_qa_csv_paths_work_outside_project_root(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.chdir(ROOT / "docs")

    response = answer_manual_qa("철괴는 어떻게 만들어?")

    source_ids = {source["doc_id"] for source in response["sources"]}
    assert {"resource_iron_ingot", "recipe_smelt_iron"} <= source_ids
