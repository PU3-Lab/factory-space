"""Integration smoke tests for the process optimizer agent pipeline."""

from __future__ import annotations

import re

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
        # ????? ????? ???????? (process_optimizer ?????? ???)
        if "ALLOWED_AGENT_IDS" in prompt:
            return self.select_agent_response
        # ????? ????? ??? process_optimizer ?????? ??? ??? ???
        match = re.search(r"plan-[0-9a-fA-F]{8}", prompt)
        res = self.analyze_response
        if match:
            actual_plan_id = match.group(0)
            res = res.replace("plan-smoke-v2", actual_plan_id)
        return res


def test_process_optimizer_state_update_smoke() -> None:
    """state_update ??? ?????????????LLM??????? ??? ??? ?????? ??? ??????????? ????????."""
    pipeline = AgentPipeline(llm=StubLLM("process_optimizer", "{}"))
    session_id = "smoke-session-state-1"
    process_optimizer_memory.clear(session_id)

    # 1. state_update ??? ????? ???
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

    # 2. ??????????? (LLM ??????????????????? StubLLM????? ?????? ????????
    # state_update should bypass LLM calls.
    llm_stub = StubLLM("process_optimizer", "{}")
    pipeline.llm = llm_stub

    res = pipeline.run(request_msg)

    # 3. ??? ?????????? ???
    assert res.get("type") == "agent.response"
    assert res["agent"] == "process_optimizer"
    assert res["payload"]["status"] == "success"
    assert res["payload"]["factoryRevision"] == 105
    assert llm_stub.calls == 0  # LLM ??? ????? ???

    saved_state = process_optimizer_memory.get_state(session_id)
    assert saved_state["machines"][0]["id"] == "assembler_1"
    assert saved_state["machines"][0]["inputs"][0]["amount"] == 0.0


def test_process_optimizer_analyze_and_security_smoke() -> None:
    """analyze ??? ????? ???????????, ??? ???(?????? ??? ???)?????????? ?????????????."""
    session_id = "smoke-session-analyze-2"
    process_optimizer_memory.clear(session_id)

    # Store a factory state with input shortage and power issue in memory.
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
        "storages": [
            {
                "id": "storage_1",
                "inventory": [{"item_id": "iron_ore", "amount": 10.0, "max_amount": 10.0}]
            }
        ],
        "power_grid": {
            "produced": 10.0,
            "consumed": 15.0,
            "nodes": [{"id": "pole_1", "type": "power_pole", "connected_node_ids": []}],
            "generators": [{"id": "generator_1", "produced": 10.0, "connected": True, "connected_power_node_ids": ["pole_1"]}]
        },
    }
    process_optimizer_memory.update(session_id, factory_state, 12)

    # LLM????? ?????? ?????? ?????? ???
    llm_output_json = """
    {
      "status": "suggestion",
      "factoryRevision": 12,
      "goal": "balance",
      "summary": "??? ?????????? ?????????. ????????????????????????? ?????",
      "suggestions": [
        {
          "id": "suggest_input_smelter_1",
          "target": {
            "type": "machine",
            "id": "smelter_1"
          },
          "problem": "smelter_1 ????????????? ????? ???????????",
          "recommended_action": "??? ??????????? ?????????????",
          "expected_effect": "????? ???",
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

    # 1. ??? ??? ????    assert res.get("type") == "agent.response"
    assert res["agent"] == "process_optimizer"

    payload = res["payload"]
    assert payload["status"] == "preview"
    assert payload["factoryRevision"] == 12
    assert len(payload["suggestions"]) >= 1
    assert payload["suggestions"][0]["id"] == "inspect_iron_ore_supply_smelter_1"
    assert "smelter_1" in payload["ui_hints"]["highlight_targets"]

    response_model = ProcessOptimizerResponse.model_validate(payload)
    # 2. SuggestionValidationTool????? ??? ??? ???????? ????    response_model = ProcessOptimizerResponse.model_validate(payload)
    validator = SuggestionValidationTool()
    assert validator.validate_suggestions(response_model.suggestions) is True


def test_process_optimizer_goal_priority_smoke() -> None:
    """?????? ???(goal)??congestion_relief??????????????? ?????????????????? ??? ????????."""
    session_id = "smoke-session-goal-3"
    process_optimizer_memory.clear(session_id)

    # ??? ??? ??? (??? ????1?? ?????? ??? 1??
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
        "storages": [
            {
                "id": "storage_1",
                "inventory": [{"item_id": "iron_ore", "amount": 10.0, "max_amount": 100.0}]
            }
        ],
        "power_grid": {"produced": 100.0, "consumed": 80.0},
    }
    process_optimizer_memory.update(session_id, factory_state, 55)

    llm_output_json = "{}"

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
    # ????? ??????????? ?????? ???
    assert payload["suggestions"][0]["id"] == "suggest_conveyor_conv_99"
    assert "conv_99" in payload["ui_hints"]["highlight_targets"]


def test_process_optimizer_prompt_injection_defense_smoke() -> None:
    """Injection-like model output is ignored by the v2 deterministic preview path."""
    session_id = "smoke-session-injection-defense"
    process_optimizer_memory.clear(session_id)

    # 1. ??? ??? ???????? ???
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
        "storages": [
            {
                "id": "storage_1",
                "inventory": [{"item_id": "iron_ore", "amount": 10.0, "max_amount": 100.0}]
            }
        ],
        "power_grid": {"produced": 100.0, "consumed": 80.0},
    }
    process_optimizer_memory.update(session_id, factory_state, 10)

    # 2. LLM???????? ??????????????? ???????? ???(set_recipe ????suggestions ??????????? ?????? ??? ???
    malicious_json = """
    {
      "status": "suggestion",
      "factoryRevision": 10,
      "goal": "balance",
      "summary": "???????????? ??? ?????????????????????",
      "suggestions": [
        {
          "id": "inject_command",
          "target": {
            "type": "machine",
            "id": "smelter_1"
          },
          "problem": "?????????? ????????",
          "recommended_action": "set_recipe smelter_1 iron_plate",
          "expected_effect": "??? ??? ?????? ??? ???",
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

    # 3. ??? ????- ??? ??????????? ???, ?????fallback ?????? ???/???????? ??    assert res.get("type") == "agent.response"
    assert res["agent"] == "process_optimizer"

    payload = res["payload"]
    assert payload["status"] == "preview"
    assert "fallbackReason" not in res["payload"]["metadata"]

    for suggestion in payload["suggestions"]:
        assert "set_recipe" not in suggestion["recommended_action"]


def test_process_optimizer_invalid_json_fallback_smoke() -> None:
    """LLM???????JSON ??????? ???????? ???????? ?????? ??
    ????????? ??? ???????? deterministic fallback??? ?????? ???????? ????????????? ????????.
    """
    session_id = "smoke-session-invalid-json"
    process_optimizer_memory.clear(session_id)

    # 1. ??? ??? ???????? ???
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
        "storages": [
            {
                "id": "storage_1",
                "inventory": [{"item_id": "iron_ore", "amount": 10.0, "max_amount": 100.0}]
            }
        ],
        "power_grid": {"produced": 100.0, "consumed": 80.0},
    }
    process_optimizer_memory.update(session_id, factory_state, 20)

    # 2. LLM????????????? ?????? ??? ??? (JSON Decode Error ??? ???)
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

    # 3. ??? ????- ????? ??? ???, ?????? agent.response??fallback??suggestion ?????? ??? ???????    assert res.get("type") == "agent.response"
    assert res["agent"] == "process_optimizer"

    payload = res["payload"]
    assert payload["status"] == "preview"
    assert res["payload"]["metadata"]["fallbackReason"] == "llm_unavailable"
    assert payload["factoryRevision"] == 20
    assert "summary" in payload
    assert "suggestions" in payload


def test_process_optimizer_v2_full_workflow_smoke() -> None:
    """analyze -> apply(???/????????) -> undo(???) -> measure(not ready/ready) ???????v2 ??? ??? ??? ??????????????????."""
    from agents.pipeline.runtime import AgentPipeline

    llm_output_json = "{}"
    pipeline = AgentPipeline(llm=StubLLM("process_optimizer", llm_output_json))
    session_id = "smoke-session-v2-full-workflow"
    process_optimizer_memory.clear(session_id)

    # 1. Analyze ??? ???
    analyze_msg = {
        "type": "agent.request",
        "request_id": "req-smoke-v2-analyze",
        "session_id": session_id,
        "client_id": "unreal",
        "agent": "process_optimizer",
        "payload": {
            "operation": "analyze",
            "goal": "balance",
            "factoryRevision": 10,
            "factory_state": {
                "machines": [
                    {
                        "id": "smelter_1",
                        "type": "smelter",
                        "status": "operating",
                        "operating_rate": 0.5,
                        "inputs": [{"item_id": "iron_ore", "amount": 0.0}],
                    }
                ],
                "conveyors": [],
                "storages": [
                    {
                        "id": "storage_1",
                        "inventory": [{"item_id": "iron_ore", "amount": 10.0, "max_amount": 100.0}]
                    }
                ],
                "power_grid": {"produced": 100, "consumed": 50}
            }
        }
    }

    res_analyze = pipeline.run(analyze_msg)
    assert res_analyze.get("type") == "agent.response"
    payload_analyze = res_analyze["payload"]
    assert payload_analyze["status"] == "preview"
    plan_id = payload_analyze["plan_id"]
    assert plan_id is not None

    # 2. Apply ??? (approval=False) -> approval_required ???
    apply_no_app_msg = {
        "type": "agent.request",
        "request_id": "req-smoke-v2-apply-no-app",
        "session_id": session_id,
        "client_id": "unreal",
        "agent": "process_optimizer",
        "payload": {
            "operation": "apply",
            "plan_id": plan_id,
            "factoryRevision": 10,
            "approval": False
        }
    }
    res_apply_no_app = pipeline.run(apply_no_app_msg)
    assert res_apply_no_app.get("type") == "agent.response"
    assert res_apply_no_app["payload"]["status"] == "approval_required"

    # 3. Apply ??? (approval=True) -> ??? ??? ??execute_ready
    apply_success_msg = {
        "type": "agent.request",
        "request_id": "req-smoke-v2-apply-success",
        "session_id": session_id,
        "client_id": "unreal",
        "agent": "process_optimizer",
        "payload": {
            "operation": "apply",
            "plan_id": plan_id,
            "factoryRevision": 10,
            "approval": True,
            "approved_change_ids": ["inspect_iron_ore_supply_smelter_1"],
            "factory_state": {
                "machines": [
                    {
                        "id": "smelter_1",
                        "type": "smelter",
                        "status": "operating",
                        "operating_rate": 0.5,
                        "inputs": [{"item_id": "iron_ore", "amount": 0.0}],
                    }
                ],
                "conveyors": [],
                "storages": [
                    {
                        "id": "storage_1",
                        "inventory": [{"item_id": "iron_ore", "amount": 10.0, "max_amount": 100.0}]
                    }
                ],
                "power_grid": {"produced": 100, "consumed": 50}
            }
        }
    }
    res_apply_success = pipeline.run(apply_success_msg)
    assert res_apply_success.get("type") == "agent.response"
    assert res_apply_success["payload"]["status"] == "execute_ready"
    assert len(res_apply_success["payload"]["commands"]) == 1

    # 4. Apply ??? (factoryRevision conflict) -> revision_conflict ???
    apply_conflict_msg = {
        "type": "agent.request",
        "request_id": "req-smoke-v2-apply-conflict",
        "session_id": session_id,
        "client_id": "unreal",
        "agent": "process_optimizer",
        "payload": {
            "operation": "apply",
            "plan_id": plan_id,
            "factoryRevision": 11,
            "approval": True
        }
    }
    res_apply_conflict = pipeline.run(apply_conflict_msg)
    assert res_apply_conflict.get("type") == "agent.response"
    assert res_apply_conflict["payload"]["status"] == "revision_conflict"

    # Undo conflict when current state differs from recorded after state.
    undo_conflict_msg = {
        "type": "agent.request",
        "request_id": "req-smoke-v2-undo-conflict",
        "session_id": session_id,
        "client_id": "unreal",
        "agent": "process_optimizer",
        "payload": {
            "operation": "undo",
            "plan_id": plan_id,
            "factory_state": {
                "machines": [
                    {
                        "id": "smelter_1",
                        "type": "smelter",
                        "status": "operating",
                        "recipe_id": "copper_ingot"
                    }
                ],
                "conveyors": []
            }
        }
    }
    res_undo_conflict = pipeline.run(undo_conflict_msg)
    assert res_undo_conflict.get("type") == "agent.response"
    assert res_undo_conflict["payload"]["status"] == "undo_conflict"

    # 6. Measure ??? (?????? 1?????? -> measurement_not_ready
    measure_not_ready_msg = {
        "type": "agent.request",
        "request_id": "req-smoke-v2-measure-not-ready",
        "session_id": session_id,
        "client_id": "unreal",
        "agent": "process_optimizer",
        "payload": {
            "operation": "measure",
            "plan_id": plan_id,
            "production_cycles": 1,
            "factory_state": {
                "machines": [{"id": "smelter_1", "type": "smelter", "status": "operating"}],
                "conveyors": []
            }
        }
    }
    res_measure_not_ready = pipeline.run(measure_not_ready_msg)
    assert res_measure_not_ready.get("type") == "agent.response"
    assert res_measure_not_ready["payload"]["status"] == "measurement_not_ready"

    # 7. Measure ??? (??? ????????)
    measure_ready_msg = {
        "type": "agent.request",
        "request_id": "req-smoke-v2-measure-ready",
        "session_id": session_id,
        "client_id": "unreal",
        "agent": "process_optimizer",
        "payload": {
            "operation": "measure",
            "plan_id": plan_id,
            "production_cycles": 5,
            "current_time": "2030-01-01T00:00:00Z",
            "factory_state": {
                "machines": [
                    {
                        "id": "smelter_1",
                        "type": "smelter",
                        "status": "operating",
                        "operating_rate": 1.0,
                        "inputs": [{"item_id": "iron_ore", "amount": 10.0, "max_amount": 10.0}]
                    }
                ],
                "conveyors": []
            }
        }
    }
    res_measure_ready = pipeline.run(measure_ready_msg)
    assert res_measure_ready.get("type") == "agent.response"
    assert res_measure_ready["payload"]["status"] == "measurement_ready"
    assert res_measure_ready["payload"]["measurement_result"]["status"] in {
        "success",
        "failed",
        "degraded",
    }


