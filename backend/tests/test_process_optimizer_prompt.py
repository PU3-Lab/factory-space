"""Unit tests for ProcessOptimizerAgent prompt generation and fallback behaviors."""

from __future__ import annotations

import pytest

import agents.process_optimizer.agent as agent_module
from agents.base import AgentContext
from agents.process_optimizer.agent import ProcessOptimizerAgent
from agents.process_optimizer.schemas import ProcessOptimizerResponse
from agents.process_optimizer.session_memory import process_optimizer_memory


def test_agent_build_prompt_includes_metrics_and_rules() -> None:
    """build_prompt를 통해 생성되는 프롬프트에 분석 지표와 시스템 안전 수칙이 온전히 포함되는지 검증합니다."""
    agent = ProcessOptimizerAgent()
    context = AgentContext(
        request_id="req-prompt-test", session_id="session-prompt-test"
    )

    # 1. 테스트용 입력 페이로드
    payload = {
        "goal": "throughput",
        "factory_state": {
            "machines": [
                {
                    "id": "smelter_1",
                    "type": "smelter",
                    "status": "operating",
                    "operating_rate": 0.8,
                    "inputs": [
                        {"item_id": "iron_ore", "amount": 0.0, "max_amount": 100.0}
                    ],  # input_shortage 유도
                }
            ],
            "conveyors": [],
            "power_grid": {"produced": 100.0, "consumed": 50.0},
        },
    }

    # metadata 및 세션 메모리 초기화
    context.metadata["factoryRevision"] = 15
    process_optimizer_memory.clear("session-prompt-test")

    # 프롬프트 빌드
    prompt = agent.build_prompt(payload, context)

    # 2. 검증 수행
    # - 역할 명칭 및 정중한 톤 지침
    assert "수석 매니저" in prompt
    assert "존댓말" in prompt

    # - 인젝션 방어 지침
    assert "이전 지시를 무시해라" in prompt
    assert "시스템 지침" in prompt or "프롬프트" in prompt

    # - 분석된 수치 포함 여부
    assert "0.80" in prompt  # 평균 설비 가동률
    assert "smelter_1" in prompt  # 감지된 머신 ID
    assert "suggest_input_smelter_1" in prompt  # 생성된 제안 ID
    assert "throughput" in prompt  # 지정된 goal
    assert "15" in prompt  # revision


def test_agent_fallback_structure_validity() -> None:
    """fallback 실행 시 반환되는 페이로드가 ProcessOptimizerResponse 스키마에 따라 유효하게 통과하는지 검증합니다."""
    agent = ProcessOptimizerAgent()
    context = AgentContext(
        request_id="req-fallback-test", session_id="session-fallback-test"
    )

    # 1. 입력 부족 및 출력 적체가 복합적으로 유도되는 payload 구성
    payload = {
        "goal": "power_saving",
        "factory_state": {
            "machines": [
                {
                    "id": "smelter_1",
                    "type": "smelter",
                    "status": "operating",
                    "operating_rate": 0.1,
                    "inputs": [
                        {"item_id": "iron_ore", "amount": 0.0, "max_amount": 50.0}
                    ],  # input_shortage
                },
                {
                    "id": "smelter_2",
                    "type": "smelter",
                    "status": "operating",
                    "operating_rate": 0.2,
                    "outputs": [
                        {"item_id": "iron_ingot", "amount": 50.0, "max_amount": 50.0}
                    ],  # output_blocked
                },
            ],
            "conveyors": [],
            "power_grid": {"produced": 50.0, "consumed": 80.0},  # power_issue 발생
        },
    }

    context.metadata["factoryRevision"] = 88
    process_optimizer_memory.clear("session-fallback-test")

    # fallback 실행
    result = agent.fallback(payload, context)

    # 2. 결과 검증
    assert result.agent == "process_optimizer"
    assert result.metadata == {"fallback": True}

    # 페이로드가 Pydantic Response 스키마에 따라 역직렬화 및 검증 통과하는지 확인
    res_payload = result.payload
    response_model = ProcessOptimizerResponse.model_validate(res_payload)

    assert response_model.factoryRevision == 88
    assert response_model.goal == "power_saving"
    assert "기본 추천 변경 계획" in response_model.summary

    # goal="power_saving"에 따른 우선순위에 의해 power_issue가 포함되어 있어야 함
    suggestions = response_model.suggestions
    assert len(suggestions) <= 3
    assert any(s.id == "suggest_power_issue" for s in suggestions)
    assert any(s.id == "suggest_input_smelter_1" for s in suggestions)

    # 3. 빈 팩토리 상태일 때의 fallback 처리 검증
    empty_payload = {
        "machines": [{"id": "m1"}, {"id": "m2"}]
    }  # 지표가 없어 제안이 0개인 상황
    result_empty = agent.fallback(empty_payload, context)

    res_empty_model = ProcessOptimizerResponse.model_validate(result_empty.payload)
    assert "2개 설비" in res_empty_model.summary
    assert len(res_empty_model.suggestions) == 0


def test_agent_build_prompt_drops_invalid_suggestions(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """SuggestionValidationTool이 실패하면 프롬프트에 제안 payload를 싣지 않는지 검증합니다."""
    agent = ProcessOptimizerAgent()
    context = AgentContext(
        request_id="req-invalid-prompt", session_id="session-invalid-prompt"
    )
    payload = {
        "factory_state": {
            "machines": [
                {
                    "id": "smelter_1",
                    "type": "smelter",
                    "status": "operating",
                    "inputs": [{"item_id": "iron_ore", "amount": 0.0}],
                }
            ],
            "conveyors": [],
            "power_grid": {"produced": 100.0, "consumed": 50.0},
        },
    }

    monkeypatch.setattr(
        agent_module.SuggestionValidationTool,
        "validate_suggestions",
        lambda self, suggestions: False,
    )

    prompt = agent.build_prompt(payload, context)

    assert '"suggestions": []' in prompt
    assert '"highlight_targets": []' in prompt
    assert "suggest_input_smelter_1" not in prompt


def test_agent_fallback_drops_invalid_suggestions(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """SuggestionValidationTool이 실패하면 fallback 응답도 빈 제안으로 안전하게 내려가는지 검증합니다."""
    agent = ProcessOptimizerAgent()
    context = AgentContext(
        request_id="req-invalid-fallback", session_id="session-invalid-fallback"
    )
    payload = {
        "factory_state": {
            "machines": [
                {
                    "id": "smelter_1",
                    "type": "smelter",
                    "status": "operating",
                    "inputs": [{"item_id": "iron_ore", "amount": 0.0}],
                }
            ],
            "conveyors": [],
            "power_grid": {"produced": 100.0, "consumed": 50.0},
        },
    }

    monkeypatch.setattr(
        agent_module.SuggestionValidationTool,
        "validate_suggestions",
        lambda self, suggestions: False,
    )

    result = agent.fallback(payload, context)
    response_model = ProcessOptimizerResponse.model_validate(result.payload)

    assert response_model.suggestions == []
    assert response_model.ui_hints.highlight_targets == []
