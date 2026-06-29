"""Unit and integration tests for Process Optimizer Subquest Sprint 2 Target Analyze."""

from __future__ import annotations

from agents.pipeline.runtime import AgentPipeline
from agents.process_optimizer.session_memory import process_optimizer_memory


class StubLLM:
    """Mock LLM responses in sequential order."""

    def __init__(self, responses: list[str | None]) -> None:
        self.responses = responses
        self.calls = 0

    def invoke(self, prompt: str) -> str | None:
        if self.calls >= len(self.responses):
            return None
        res = self.responses[self.calls]
        self.calls += 1
        return res


def test_target_analyze_input_shortage() -> None:
    """타겟 장비에 입력 부족이 있을 때 해당 타겟에 대한 제안이 첫 번째이고 요약문이 알맞게 생성되는지 검증합니다."""
    # LLM이 실패하도록 설정하여 return_preview_plan의 fallback summary_text가 보존되도록 함
    pipeline = AgentPipeline(llm=StubLLM([None, None]))
    session_id = "session-target-input-shortage"
    process_optimizer_memory.clear(session_id)

    request_msg = {
        "type": "agent.request",
        "request_id": "req-target-input-1",
        "session_id": session_id,
        "client_id": "unreal",
        "agent": "process_optimizer",
        "payload": {
            "operation": "analyze",
            "goal": "balance",
            "factoryRevision": 10,
            "target": {"type": "machine", "id": "smelter_1"},
            "factory_state": {
                "machines": [
                    {
                        "id": "constructor_1",
                        "type": "constructor",
                        "status": "operating",
                        "inputs": [{"item_id": "iron_ingot", "amount": 0.0}],
                    },
                    {
                        "id": "smelter_1",
                        "type": "smelter",
                        "status": "operating",
                        "inputs": [{"item_id": "iron_ore", "amount": 0.0}],
                    },
                ],
                "conveyors": [],
                "power_grid": {"produced": 100.0, "consumed": 50.0},
            },
        },
    }

    res = pipeline.run(request_msg)

    assert res["type"] == "agent.response"
    payload = res["payload"]
    assert payload["status"] == "preview"
    assert payload["factoryRevision"] == 10

    # smelter_1에 대한 제안이 첫 번째로 배치되어야 함
    assert len(payload["changes"]) == 2
    assert payload["changes"][0]["target"]["id"] == "smelter_1"
    assert payload["changes"][1]["target"]["id"] == "constructor_1"

    # 요약문에 smelter_1과 입력 재고 부족 내용이 포함되어야 함
    assert "smelter_1" in payload["summary"]
    assert "입력 재고 부족" in payload["summary"]


def test_target_analyze_output_blocked() -> None:
    """타겟 장비에 출력 적체가 있을 때 해당 타겟 제안이 첫 번째이고 요약문이 알맞게 생성되는지 검증합니다."""
    pipeline = AgentPipeline(llm=StubLLM([None, None]))
    session_id = "session-target-output-blocked"
    process_optimizer_memory.clear(session_id)

    request_msg = {
        "type": "agent.request",
        "request_id": "req-target-output-1",
        "session_id": session_id,
        "client_id": "unreal",
        "agent": "process_optimizer",
        "payload": {
            "operation": "analyze",
            "goal": "balance",
            "factoryRevision": 20,
            "target": {"type": "machine", "id": "constructor_1"},
            "factory_state": {
                "machines": [
                    {
                        "id": "smelter_1",
                        "type": "smelter",
                        "status": "operating",
                        "inputs": [{"item_id": "iron_ore", "amount": 0.0}],
                    },
                    {
                        "id": "constructor_1",
                        "type": "constructor",
                        "status": "operating",
                        "inputs": [{"item_id": "iron_ingot", "amount": 10.0}],
                        "outputs": [
                            {
                                "item_id": "iron_plate",
                                "amount": 100.0,
                                "max_amount": 100.0,
                            }
                        ],
                    },
                ],
                "conveyors": [],
                "power_grid": {"produced": 100.0, "consumed": 50.0},
            },
        },
    }

    res = pipeline.run(request_msg)

    assert res["type"] == "agent.response"
    payload = res["payload"]
    assert payload["status"] == "preview"

    # constructor_1에 대한 제안이 첫 번째로 배치되어야 함
    assert len(payload["changes"]) == 2
    assert payload["changes"][0]["target"]["id"] == "constructor_1"
    assert payload["changes"][1]["target"]["id"] == "smelter_1"

    # 요약문에 constructor_1과 생산품 출력 적체 내용이 포함되어야 함
    assert "constructor_1" in payload["summary"]
    assert "생산품 출력 적체" in payload["summary"]


def test_target_analyze_unrelated_target() -> None:
    """타겟 장비 자체에는 직접 병목이 없지만 다른 곳에 문제가 있을 때 기존 전체 공장 제안이 나오고 안내문이 출력되는지 검증합니다."""
    pipeline = AgentPipeline(llm=StubLLM([None, None]))
    session_id = "session-target-unrelated"
    process_optimizer_memory.clear(session_id)

    request_msg = {
        "type": "agent.request",
        "request_id": "req-target-unrelated-1",
        "session_id": session_id,
        "client_id": "unreal",
        "agent": "process_optimizer",
        "payload": {
            "operation": "analyze",
            "goal": "balance",
            "factoryRevision": 30,
            "target": {"type": "machine", "id": "smelter_2"},  # smelter_2는 문제 없음
            "factory_state": {
                "machines": [
                    {
                        "id": "smelter_1",
                        "type": "smelter",
                        "status": "operating",
                        "inputs": [{"item_id": "iron_ore", "amount": 0.0}],
                    },
                    {
                        "id": "smelter_2",
                        "type": "smelter",
                        "status": "operating",
                        "inputs": [{"item_id": "copper_ore", "amount": 10.0}],
                    },
                ],
                "conveyors": [],
                "power_grid": {"produced": 100.0, "consumed": 50.0},
            },
        },
    }

    res = pipeline.run(request_msg)

    assert res["type"] == "agent.response"
    payload = res["payload"]
    assert payload["status"] == "preview"

    # smelter_2 관련 제안은 없으므로 smelter_1에 대한 제안이 유지
    assert len(payload["changes"]) == 1
    assert payload["changes"][0]["target"]["id"] == "smelter_1"

    # 요약문에 smelter_2 자체의 직접 병목은 크지 않다는 내용이 표기되어야 함
    assert "smelter_2" in payload["summary"]
    assert "직접 병목은 크지 않지만" in payload["summary"]


def test_target_analyze_highlight_targets_prepended() -> None:
    """타겟 장비가 하이라이트 목록에 없더라도 highlight_targets의 맨 처음에 추가되는지 검증합니다."""
    pipeline = AgentPipeline(llm=StubLLM([None, None]))
    session_id = "session-target-highlight"
    process_optimizer_memory.clear(session_id)

    request_msg = {
        "type": "agent.request",
        "request_id": "req-target-highlight-1",
        "session_id": session_id,
        "client_id": "unreal",
        "agent": "process_optimizer",
        "payload": {
            "operation": "analyze",
            "goal": "balance",
            "factoryRevision": 40,
            "target": {
                "type": "machine",
                "id": "smelter_2",
            },  # 문제 없는 smelter_2 타겟
            "factory_state": {
                "machines": [
                    {
                        "id": "smelter_1",
                        "type": "smelter",
                        "status": "operating",
                        "inputs": [{"item_id": "iron_ore", "amount": 0.0}],
                    },
                    {
                        "id": "smelter_2",
                        "type": "smelter",
                        "status": "operating",
                        "inputs": [{"item_id": "copper_ore", "amount": 10.0}],
                    },
                ],
                "conveyors": [],
                "power_grid": {"produced": 100.0, "consumed": 50.0},
            },
        },
    }

    res = pipeline.run(request_msg)

    assert res["type"] == "agent.response"
    payload = res["payload"]

    # ui_hints의 highlight_targets 맨 앞에 smelter_2가 있어야 함
    assert "ui_hints" in payload
    highlights = payload["ui_hints"]["highlight_targets"]
    assert len(highlights) == 2
    assert highlights[0] == "smelter_2"
    assert highlights[1] == "smelter_1"


def test_target_analyze_malformed_target() -> None:
    """비정상적인 target 구조가 전달되었을 때 스키마 유효성 검사에서 에러를 반환하는지 검증합니다."""
    pipeline = AgentPipeline()
    session_id = "session-target-malformed"
    process_optimizer_memory.clear(session_id)

    request_msg = {
        "type": "agent.request",
        "request_id": "req-target-malformed-1",
        "session_id": session_id,
        "client_id": "unreal",
        "agent": "process_optimizer",
        "payload": {
            "operation": "analyze",
            "goal": "balance",
            "factoryRevision": 50,
            "target": {"type": "invalid_type", "id": "smelter_1"},  # type이 wrong
            "factory_state": {
                "machines": [
                    {
                        "id": "smelter_1",
                        "type": "smelter",
                        "status": "operating",
                    }
                ],
            },
        },
    }

    res = pipeline.run(request_msg)

    assert res["type"] == "agent.error"
    assert "INVALID_REQUEST_PAYLOAD" in res["error"]["code"]
