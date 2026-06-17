"""Sprint 15.1. Current Game State 보완 검증을 위한 테스트 모듈.

초보자용 설명:
    이 테스트는 ContextNeedClassifier가 LLM 어댑터(mock)를 이용해 똑똑하게 실시간 정황이 필요한지
    판단하는 기능, 그리고 누락된 2대 스코프(connectedConveyors, recentErrorEvents)가 정상 포함되는지,
    LLM 호출 오류 시 안전하게 규칙 기반 fallback으로 대처하는지를 보증합니다.
"""

from __future__ import annotations

import json

from agents.operator_guide.question_classifier import ContextNeedClassifier
from agents.operator_guide.service import ManualQAService


class MockLLMAdapter:
    """테스트를 위해 외부 API 통신 없이 가상의 LLM 응답을 시뮬레이션하는 가짜 어댑터입니다.

    초보자용 설명:
        테스트 시에 실제 LLM API를 호출하면 비용과 속도 이슈가 있으며, 비결정론적인 결과가 나올 수 있습니다.
        이 클래스는 지정한 응답 문자열을 리턴하거나 인위적으로 오류를 일으켜서 에이전트의 예외 처리와 로직 분기를 독립 검증합니다.
    """

    def __init__(self, response: str | None = None, raise_error: bool = False) -> None:
        self.response = response
        self.raise_error = raise_error
        self.calls: list[str] = []

    def invoke(self, prompt: str) -> str | None:
        self.calls.append(prompt)
        if self.raise_error:
            raise ValueError("Simulated LLM Timeout/Error")
        return self.response

    def invoke_messages(self, messages: list[dict[str, str]]) -> str | None:
        return None


def test_llm_classifier_success() -> None:
    """LLM이 성공적으로 분석하여 특정 스코프 정보 연동을 결정했을 때의 시나리오를 테스트합니다."""
    # LLM이 powerStatus와 inputInventory만 필요하다고 판별한 가짜 JSON 응답 설정
    mock_response = json.dumps({
        "requires_current_game_state": True,
        "required_state_scopes": ["powerStatus", "inputInventory", "invalidScopeIgnored"]
    })
    mock_adapter = MockLLMAdapter(response=mock_response)
    classifier = ContextNeedClassifier(llm_adapter=mock_adapter)

    requires_state, scopes = classifier.classify_need(
        "철괴가 왜 안 만들어져?",
        "troubleshooting_question",
    )

    assert requires_state is True
    assert "powerStatus" in scopes
    assert "inputInventory" in scopes
    # 허용되지 않은 스코프("invalidScopeIgnored")는 필터링되어 없어야 함
    assert "invalidScopeIgnored" not in scopes
    assert len(scopes) == 2
    assert len(mock_adapter.calls) == 1


def test_llm_classifier_fallback() -> None:
    """LLM 어댑터에서 예외가 발생했을 때, 예외가 캐치되며 기존 규칙 기반(Rule-based) 방식으로 정상 fallback하는지 테스트합니다."""
    mock_adapter = MockLLMAdapter(raise_error=True)
    classifier = ContextNeedClassifier(llm_adapter=mock_adapter)

    # 예외가 발생해도 크래시 없이 rule-based fallback이 작동하여 7대 스코프 전체가 반환되어야 함
    requires_state, scopes = classifier.classify_need(
        "철괴가 안 만들어져. 왜 그래?",
        "troubleshooting_question",
    )

    assert requires_state is True
    assert len(scopes) == 7
    assert "connectedConveyors" in scopes
    assert "recentErrorEvents" in scopes


def test_new_scopes_inclusion() -> None:
    """새로이 추가된 스코프 connectedConveyors와 recentErrorEvents가 룰 기반 fallback 시 포함되는지 검증합니다."""
    classifier = ContextNeedClassifier()  # llm_adapter가 없는 경우 기본적으로 룰 기반 작동
    requires_state, scopes = classifier.classify_need(
        "기계가 안 돌아가. 원인이 뭐야?",
        "troubleshooting_question",
    )

    assert requires_state is True
    assert "connectedConveyors" in scopes
    assert "recentErrorEvents" in scopes
    assert len(scopes) == 7


def test_static_question_no_state() -> None:
    """일반 지식 질문 시에는 실시간 정황이 필요하지 않다고 판단하여 도구 호출이 스킵되는지 검증합니다."""
    classifier = ContextNeedClassifier()
    requires_state, scopes = classifier.classify_need(
        "기어는 어떻게 만들어?",
        "recipe_question",
    )

    assert requires_state is False
    assert len(scopes) == 0


def test_sprint_15_1_success_criteria_integration() -> None:
    """ManualQAService를 통해 실제로 전체 7대 스코프가 프롬프트 및 메타데이터에 연동되는지 검증하는 통합 시나리오 테스트입니다."""
    # LLM 어댑터 없이 Rule-based로 돌려 7대 스코프를 검출하게 유도
    service = ManualQAService()
    context = {
        "current_game_state": {
            "selectedMachine": "Constructor",
            "powerStatus": "OFF",
            "connectedConveyors": "Conveyor A -> B",
            "recentErrorEvents": "No power supply",
        }
    }

    prompt_context = service.build_prompt_context("철괴가 안 만들어져. 왜 그래?", context=context)
    result = prompt_context.result

    # 7대 스코프가 모두 required_state_scopes에 포함되어야 함
    assert result.requires_current_game_state is True
    assert "connectedConveyors" in result.required_state_scopes
    assert "recentErrorEvents" in result.required_state_scopes

    # 실제 context에 기재되어 제공된 scope만 available_scopes로 식별됨
    assert "connectedConveyors" in result.available_scopes
    assert "recentErrorEvents" in result.available_scopes
    assert "inputInventory" not in result.available_scopes  # 제공하지 않았으므로 제외됨

    # 프롬프트에도 주입되었는지 확인
    prompt = service.build_prompt("철괴가 안 만들어져. 왜 그래?", topic="troubleshooting", sub_agent="operator_guide", context=context)
    assert "[CURRENT_GAME_STATE]" in prompt
    assert "connectedConveyors: Conveyor A -> B" in prompt
    assert "recentErrorEvents: No power supply" in prompt
