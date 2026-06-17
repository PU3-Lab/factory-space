"""Sprint 15. Current Game State Tool & Context Need Classifier 테스트 모듈.

초보자용 설명:
    이 테스트는 플레이어가 고장 및 문제 분석 질문을 할 때,
    에이전트가 실시간 게임 상태(전력 상태, 인벤토리 등)를 적절히 가져와 프롬프트에 반영하는지
    그리고 정적인 백과사전식 질문일 때는 상태 정보를 가져오지 않는지를 검증합니다.
"""

from __future__ import annotations

from agents.operator_guide.question_classifier import ContextNeedClassifier
from agents.operator_guide.service import CurrentGameStateTool, ManualQAService


def test_context_need_classifier_troubleshooting() -> None:
    """트러블슈팅성 질문일 경우 게임 상태 연동 필요성(True)과 필요한 scope 목록을 판별하는지 테스트합니다."""
    classifier = ContextNeedClassifier()
    requires_state, scopes = classifier.classify_need(
        "철괴가 안 만들어져. 왜 그래?",
        "troubleshooting_question",
    )
    assert requires_state is True
    assert "powerStatus" in scopes
    assert "inputInventory" in scopes
    assert "outputInventory" in scopes
    assert "selectedMachine" in scopes
    assert "currentRecipe" in scopes


def test_context_need_classifier_static_question() -> None:
    """일반 지식 질문일 경우 게임 상태 연동이 필요 없음(False)으로 판별되는지 테스트합니다."""
    classifier = ContextNeedClassifier()
    requires_state, scopes = classifier.classify_need(
        "기어는 어떻게 만들어?",
        "recipe_question",
    )
    assert requires_state is False
    assert len(scopes) == 0


def test_game_state_tool_filtering() -> None:
    """CurrentGameStateTool이 플레이어 상태 context에서 필요한 scopes의 데이터만 걸러내는지 검증합니다."""
    tool = CurrentGameStateTool()
    raw_state = {
        "selectedMachine": "Smelter",
        "powerStatus": "OFF",
        "inputInventory": "Iron Ore: 0",
        "outputInventory": "Iron Ingot: 10",
        "currentRecipe": "Iron Ingot Recipe",
        "extraUnneededField": "ignored_value",
    }
    requested_scopes = ["powerStatus", "inputInventory", "nonExistentScope"]
    state_text, available_scopes = tool.extract_state(raw_state, requested_scopes)

    # 요청한 scope 중 실제 raw_state에 있는 것만 포함되어야 함
    assert "powerStatus: OFF" in state_text
    assert "inputInventory: Iron Ore: 0" in state_text
    assert "extraUnneededField" not in state_text
    assert "nonExistentScope" not in state_text

    assert "powerStatus" in available_scopes
    assert "inputInventory" in available_scopes
    assert "nonExistentScope" not in available_scopes


def test_sprint_15_success_criteria_gear() -> None:
    """성공 기준 1: '기어는 어떻게 만들어?' 질문 시 게임 상태 도구를 호출하지 않는지 확인합니다."""
    service = ManualQAService()

    # context에 현재 게임 상태 데이터를 넘기더라도
    context = {
        "current_game_state": {
            "selectedMachine": "Constructor",
            "powerStatus": "ON",
        }
    }

    prompt_context = service.build_prompt_context(
        "기어는 어떻게 만들어?", context=context
    )
    result = prompt_context.result

    # RAG만 사용되고 게임 상태는 연동되지 않아야 함
    assert result.requires_current_game_state is False
    assert result.used_current_game_state is False
    assert len(result.required_state_scopes) == 0
    assert len(result.available_scopes) == 0
    assert prompt_context.current_game_state_text == ""

    # 프롬프트에 [CURRENT_GAME_STATE] 섹션이 없어야 함
    prompt = service.build_prompt(
        "기어는 어떻게 만들어?",
        topic="recipe",
        sub_agent="operator_guide",
        context=context,
    )
    assert "[CURRENT_GAME_STATE]" not in prompt


def test_sprint_15_success_criteria_iron_ingot_stopped() -> None:
    """성공 기준 2: '철괴가 안 만들어져. 왜 그래?' 질문 시 게임 상태 도구를 호출하고 연동하는지 확인합니다."""
    service = ManualQAService()

    context = {
        "current_game_state": {
            "selectedMachine": "Smelter",
            "powerStatus": "OFF",
            "inputInventory": "Iron Ore: 0",
            "outputInventory": "Iron Ingot: 10",
            "currentRecipe": "Iron Ingot Recipe",
        }
    }

    prompt_context = service.build_prompt_context(
        "철괴가 안 만들어져. 왜 그래?", context=context
    )
    result = prompt_context.result

    # 게임 상태 연동 정보가 설정되어 있어야 함
    assert result.requires_current_game_state is True
    assert result.used_current_game_state is True
    assert "powerStatus" in result.required_state_scopes
    assert "powerStatus" in result.available_scopes
    assert "powerStatus: OFF" in prompt_context.current_game_state_text

    # 프롬프트에 [CURRENT_GAME_STATE] 섹션이 포함되어야 함
    prompt = service.build_prompt(
        "철괴가 안 만들어져. 왜 그래?",
        topic="troubleshooting",
        sub_agent="operator_guide",
        context=context,
    )
    assert "[CURRENT_GAME_STATE]" in prompt
    assert "powerStatus: OFF" in prompt
    assert "selectedMachine: Smelter" in prompt
