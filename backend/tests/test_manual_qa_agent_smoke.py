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
        "expected_source_ids": ["resource_iron_ingot", "recipe_iron_ingot"],
        "expected_action_ids": ["action_explain_resource_production"],
    },
    {
        "question": "기어 만들려면 뭐가 필요해?",
        "expected_question_type": "recipe_question",
        "expected_source_ids": ["recipe_gear", "resource_iron_ingot"],
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
    with (GAME_DATA / filename).open(encoding="utf-8-sig", newline="") as csv_file:
        return {row[id_column] for row in csv.DictReader(csv_file)}


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
    assert importlib.util.find_spec("agents.operator_guide.question_classifier") is not None
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
    assert "현재 매뉴얼 데이터에서 확인할 수 없습니다" in response["answer"]


def test_manual_qa_troubleshooting_answer_is_player_friendly() -> None:
    response = answer_manual_qa("제련기가 왜 안 돌아가?")

    assert response["question_type"] == "troubleshooting_question"
    assert "멈췄군요" in response["final_answer"]
    assert "전력이 제대로 들어오는지" in response["final_answer"]
    assert "철광석" in response["final_answer"]
    assert "컨베이어나 저장고" in response["final_answer"]
    assert "check_power" not in response["final_answer"]


def test_manual_qa_answers_use_conversational_guidance() -> None:
    equipment_response = answer_manual_qa("제련기는 뭐야?")
    recipe_response = answer_manual_qa("기어 만들려면 뭐가 필요해?")
    unknown_response = answer_manual_qa("우주 엘리베이터는 어떻게 업그레이드해?")

    assert equipment_response["final_answer"].startswith("좋아요.")
    assert "먼저 확인해보세요" in equipment_response["final_answer"]
    assert recipe_response["final_answer"].startswith("좋아요.")
    assert "확인해볼까요" in recipe_response["final_answer"]
    assert "예를 들면" in unknown_response["final_answer"]


def test_manual_qa_answers_use_formal_tone() -> None:
    for case in SMOKE_CASES:
        response = answer_manual_qa(case["question"])

        assert "한다" not in response["final_answer"]
        for action in response["recommended_actions"]:
            assert "한다" not in action["description"]


def test_manual_qa_csv_paths_work_outside_project_root(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.chdir(ROOT / "docs")

    response = answer_manual_qa("철괴는 어떻게 만들어?")

    source_ids = {source["doc_id"] for source in response["sources"]}
    assert {"resource_iron_ingot", "recipe_iron_ingot"} <= source_ids
