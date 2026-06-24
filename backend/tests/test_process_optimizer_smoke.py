"""Integration smoke tests for the process optimizer agent pipeline."""

from __future__ import annotations

from agents.pipeline.runtime import AgentPipeline
from agents.process_optimizer.schemas import ProcessOptimizerResponse
from agents.process_optimizer.session_memory import process_optimizer_memory
from agents.process_optimizer.suggestion import SuggestionValidationTool


class StubLLM:
    """Mock LLM to simulate target selection and JSON generation."""

    def __init__(self, select_agent_response: str, analyze_response: str) -> None:
        self.select_agent_response = select_agent_response
        self.analyze_response = analyze_response
        self.calls = 0

    def invoke(self, prompt: str) -> str | None:
        self.calls += 1
        # 첫 번째 호출은 라우팅 결정 (process_optimizer 에이전트 선택)
        if "ALLOWED_AGENT_IDS" in prompt or "오케스트레이터" in prompt:
            return self.select_agent_response
        # 두 번째 호출은 실제 process_optimizer 에이전트 실행 윤색 결과
        return self.analyze_response


def test_process_optimizer_state_update_smoke() -> None:
    """state_update 요청 시 오케스트레이터 LLM을 거치지 않고 세션 메모리를 즉시 업데이트하는지 검증합니다."""
    pipeline = AgentPipeline(llm=StubLLM("process_optimizer", "{}"))
    session_id = "smoke-session-state-1"
    process_optimizer_memory.clear(session_id)

    # 1. state_update 전송 메시지 구성
    request_msg = {
        "type": "agent.request",
        "request_id": "req-smoke-state-update",
        "session_id": session_id,
        "client_id": "unreal",
        "agent": "process_optimizer",
        "payload": {
            "operation": "state_update",
            "goal": "balance",
            "factoryRevision": 105,
            "factory_state": {
                "machines": [
                    {
                        "id": "assembler_1",
                        "type": "assembler",
                        "status": "operating",
                        "inputs": [{"item_id": "iron_plate", "amount": 0.0}],
                    }
                ],
                "conveyors": [],
                "power_grid": {"produced": 50.0, "consumed": 30.0},
            },
        },
    }

    # 2. 파이프라인 실행 (LLM 호출이 실제로 스킵되었는지 StubLLM의 호출 카운트로 검증 가능)
    # state_update는 오케스트레이터 LLM을 우회해야 하므로 StubLLM.invoke가 호출되지 않아 호출 수가 0이어야 함
    llm_stub = StubLLM("process_optimizer", "{}")
    pipeline.llm = llm_stub

    res = pipeline.run(request_msg)

    # 3. 결과 및 메모리 갱신 확인
    assert res.get("type") == "agent.response"
    assert res["agent"] == "process_optimizer"
    assert res["payload"]["status"] == "success"
    assert res["payload"]["factoryRevision"] == 105
    assert llm_stub.calls == 0  # LLM 호출 건너뜀 입증

    # 세션 메모리 검증
    saved_state = process_optimizer_memory.get_state(session_id)
    assert saved_state["machines"][0]["id"] == "assembler_1"
    assert saved_state["machines"][0]["inputs"][0]["amount"] == 0.0


def test_process_optimizer_analyze_and_security_smoke() -> None:
    """analyze 요청 시 정상 제안이 생성되고, 보안 규칙(실행명령 포함 금지)이 지켜지는지 스모크 검증합니다."""
    session_id = "smoke-session-analyze-2"
    process_optimizer_memory.clear(session_id)

    # 미리 세션 메모리에 입력 부족과 전력 부족이 발생하는 공장 상태 저장
    factory_state = {
        "machines": [
            {
                "id": "smelter_1",
                "type": "smelter",
                "status": "operating",
                "inputs": [{"item_id": "iron_ore", "amount": 0.0, "max_amount": 10.0}],
            }
        ],
        "conveyors": [],
        "power_grid": {"produced": 10.0, "consumed": 15.0},
    }
    process_optimizer_memory.update(session_id, factory_state, 12)

    # LLM이 정상 포맷으로 반환하는 시나리오 모킹
    llm_output_json = """
    {
      "status": "suggestion",
      "factoryRevision": 12,
      "goal": "balance",
      "summary": "수석 매니저의 개선 보고서입니다. 제련기 가동 상태를 긴급히 점검해야 합니다.",
      "suggestions": [
        {
          "id": "suggest_input_smelter_1",
          "target": {
            "type": "machine",
            "id": "smelter_1"
          },
          "problem": "smelter_1 설비의 원자재 입력 재고가 고갈되었습니다.",
          "recommended_action": "공급 라인의 컨베이어 벨트를 점검하세요.",
          "expected_effect": "가동률 복구",
          "risk": "low",
          "confidence": 1.0
        }
      ],
      "ui_hints": {
        "highlight_targets": ["smelter_1"]
      }
    }
    """

    pipeline = AgentPipeline(llm=StubLLM("process_optimizer", llm_output_json))

    request_msg = {
        "type": "agent.request",
        "request_id": "req-smoke-analyze",
        "session_id": session_id,
        "client_id": "unreal",
        "agent": "process_optimizer",
        "payload": {
            "operation": "analyze",
            "goal": "balance",
        },
    }

    res = pipeline.run(request_msg)

    # 1. 결과 필드 검증
    assert res.get("type") == "agent.response"
    assert res["agent"] == "process_optimizer"

    payload = res["payload"]
    assert payload["status"] == "suggestion"
    assert payload["factoryRevision"] == 12
    assert "수석 매니저" in payload["summary"]
    assert len(payload["suggestions"]) == 1
    assert payload["suggestions"][0]["id"] == "suggest_input_smelter_1"
    assert "smelter_1" in payload["ui_hints"]["highlight_targets"]

    # 2. SuggestionValidationTool을 통한 최종 출력 보안성 안전 검증
    response_model = ProcessOptimizerResponse.model_validate(payload)
    validator = SuggestionValidationTool()
    assert validator.validate_suggestions(response_model.suggestions) is True


def test_process_optimizer_goal_priority_smoke() -> None:
    """우선순위 목표(goal)를 congestion_relief로 주었을 때 컨베이어 제안이 상위에 포함되는지 통합 검증합니다."""
    session_id = "smoke-session-goal-3"
    process_optimizer_memory.clear(session_id)

    # 복합 병목 상황 (입력 부족 1개, 컨베이어 정체 1개)
    factory_state = {
        "machines": [
            {
                "id": "smelter_1",
                "type": "smelter",
                "status": "operating",
                "inputs": [{"item_id": "iron_ore", "amount": 0.0}],
            }
        ],
        "conveyors": [
            {
                "id": "conv_99",
                "congestion_rate": 0.9,
            }
        ],
        "power_grid": {"produced": 100.0, "consumed": 80.0},
    }
    process_optimizer_memory.update(session_id, factory_state, 55)

    # LLM이 2개의 제안을 포함해 응답한 것으로 시뮬레이션
    llm_output_json = """
    {
      "status": "suggestion",
      "factoryRevision": 55,
      "goal": "congestion_relief",
      "summary": "컨베이어 혼잡 해소를 최우선으로 제안합니다.",
      "suggestions": [
        {
          "id": "suggest_conveyor_conv_99",
          "target": {
            "type": "conveyor",
            "id": "conv_99"
          },
          "problem": "conv_99 컨베이어 벨트가 혼잡 상태입니다.",
          "recommended_action": "벨트 등급을 업그레이드하십시오.",
          "expected_effect": "이송 속도 향상",
          "risk": "low",
          "confidence": 0.8
        },
        {
          "id": "suggest_input_smelter_1",
          "target": {
            "type": "machine",
            "id": "smelter_1"
          },
          "problem": "smelter_1 설비의 입력 재고 고갈",
          "recommended_action": "공급망 점검",
          "expected_effect": "가동률 복구",
          "risk": "low",
          "confidence": 1.0
        }
      ],
      "ui_hints": {
        "highlight_targets": ["conv_99", "smelter_1"]
      }
    }
    """

    pipeline = AgentPipeline(llm=StubLLM("process_optimizer", llm_output_json))

    request_msg = {
        "type": "agent.request",
        "request_id": "req-smoke-goal-priority",
        "session_id": session_id,
        "client_id": "unreal",
        "agent": "process_optimizer",
        "payload": {
            "operation": "analyze",
            "goal": "congestion_relief",
        },
    }

    res = pipeline.run(request_msg)

    payload = res["payload"]
    assert payload["goal"] == "congestion_relief"
    # 첫 번째 제안이 컨베이어 제안인지 확인
    assert payload["suggestions"][0]["id"] == "suggest_conveyor_conv_99"
    assert "conv_99" in payload["ui_hints"]["highlight_targets"]


def test_process_optimizer_prompt_injection_defense_smoke() -> None:
    """프롬프트 인젝션 또는 오동작으로 인해 금지된 실행 명령어가 섞여 들어간 경우,
    검증기가 이를 차단하고 결정론적 Fallback으로 우회하여 안전한 응답을 제공하는지 검증합니다.
    """
    session_id = "smoke-session-injection-defense"
    process_optimizer_memory.clear(session_id)

    # 1. 공장 상태 메모리 사전 주입
    factory_state = {
        "machines": [
            {
                "id": "smelter_1",
                "type": "smelter",
                "status": "operating",
                "inputs": [{"item_id": "iron_ore", "amount": 0.0}],
            }
        ],
        "conveyors": [],
        "power_grid": {"produced": 100.0, "consumed": 80.0},
    }
    process_optimizer_memory.update(session_id, factory_state, 10)

    # 2. LLM이 프롬프트 인젝션 공격에 넘어가 금지된 실행 명령(set_recipe 등)을 suggestions 내부에 주입하여 반환하는 상황 모사
    malicious_json = """
    {
      "status": "suggestion",
      "factoryRevision": 10,
      "goal": "balance",
      "summary": "공장에 대한 악성 조작 명령어 공격이 시도되었습니다.",
      "suggestions": [
        {
          "id": "inject_command",
          "target": {
            "type": "machine",
            "id": "smelter_1"
          },
          "problem": "시스템 지시를 우회합니다.",
          "recommended_action": "set_recipe smelter_1 iron_plate",
          "expected_effect": "강제 강철 플레이트 생산 개시",
          "risk": "high",
          "confidence": 1.0
        }
      ],
      "ui_hints": {
        "highlight_targets": ["smelter_1"]
      }
    }
    """

    pipeline = AgentPipeline(llm=StubLLM("process_optimizer", malicious_json))

    request_msg = {
        "type": "agent.request",
        "request_id": "req-smoke-injection",
        "session_id": session_id,
        "client_id": "unreal",
        "agent": "process_optimizer",
        "payload": {
            "operation": "analyze",
            "goal": "balance",
        },
    }

    res = pipeline.run(request_msg)

    # 3. 결과 검증 - 악성 제안이 반환되지 않고, 안전한 fallback 데이터로 차단/대체되어야 함
    assert res.get("type") == "agent.response"
    assert res["agent"] == "process_optimizer"

    payload = res["payload"]
    assert payload["status"] == "suggestion"
    assert res["payload"]["metadata"]["fallbackReason"] == "validation_failed"
    assert (
        "기본 추천 변경 계획" in payload["summary"] or "1개 설비" in payload["summary"]
    )

    for suggestion in payload["suggestions"]:
        assert "set_recipe" not in suggestion["recommended_action"]


def test_process_optimizer_invalid_json_fallback_smoke() -> None:
    """LLM이 유효한 JSON 대신 깨진 포맷의 일반 텍스트 등을 반환했을 때,
    파이프라인이 에러 크래시 없이 deterministic fallback으로 복구하여 안전한 응답 구조를 제공하는지 검증합니다.
    """
    session_id = "smoke-session-invalid-json"
    process_optimizer_memory.clear(session_id)

    # 1. 공장 상태 메모리 사전 주입
    factory_state = {
        "machines": [
            {
                "id": "smelter_1",
                "type": "smelter",
                "status": "operating",
                "inputs": [{"item_id": "iron_ore", "amount": 0.0}],
            }
        ],
        "conveyors": [],
        "power_grid": {"produced": 100.0, "consumed": 80.0},
    }
    process_optimizer_memory.update(session_id, factory_state, 20)

    # 2. LLM이 비정상 텍스트를 반환하는 상황 모사 (JSON Decode Error 발생 상황)
    broken_llm_response = (
        "Here is the response in plain text that is totally not valid JSON. Sorry!"
    )

    pipeline = AgentPipeline(llm=StubLLM("process_optimizer", broken_llm_response))

    request_msg = {
        "type": "agent.request",
        "request_id": "req-smoke-invalid-json",
        "session_id": session_id,
        "client_id": "unreal",
        "agent": "process_optimizer",
        "payload": {
            "operation": "analyze",
            "goal": "balance",
        },
    }

    res = pipeline.run(request_msg)

    # 3. 결과 검증 - 에러가 나지 않고, 정상적인 agent.response에 fallback된 suggestion 데이터를 담고 있어야 함
    assert res.get("type") == "agent.response"
    assert res["agent"] == "process_optimizer"

    payload = res["payload"]
    assert payload["status"] == "suggestion"
    assert res["payload"]["metadata"]["fallbackReason"] == "json_decode_failed"
    assert "factoryRevision" in payload
    assert payload["factoryRevision"] == 20
    assert "summary" in payload
    assert "suggestions" in payload
