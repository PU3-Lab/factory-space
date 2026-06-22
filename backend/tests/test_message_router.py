from __future__ import annotations

import json

from agents.pipeline import AgentPipeline
from agents.quest_generator.service import QuestAgentService
from agents.quest_generator.tools import PRODUCTION_QUEST_SELECTION_TOOL_NAME
from tests.harness import (
    PipelineScenario,
    StubLLM,
    assert_agent_error,
    assert_agent_response,
    leaf_agent_decision,
    run_pipeline_scenario,
    top_agent_decision,
)

QUEST_SELECTED_IDS = [10, 9, 8, 7, 6]
QUEST_TOOL_CALL = json.dumps(
    {
        "tool_call": {
            "name": PRODUCTION_QUEST_SELECTION_TOOL_NAME,
            "args": {"selected_quest_ids": QUEST_SELECTED_IDS},
        }
    },
)
QUEST_TOOL_RESPONSE = json.dumps(
    QuestAgentService().generate_quest_json_from_ids(QUEST_SELECTED_IDS),
    ensure_ascii=False,
)


def test_pipeline_uses_prompt_based_top_level_routing() -> None:
    response, llm = run_pipeline_scenario(
        PipelineScenario(
            name="prompt routed quest",
            agent=None,
            payload={"message": "create an objective"},
            request_id="request-1",
            llm_responses=[
                top_agent_decision("quest_generator"),
                leaf_agent_decision("quest_generator.production_quest"),
                QUEST_TOOL_CALL,
                QUEST_TOOL_RESPONSE,
            ],
        )
    )

    assert_agent_response(
        response,
        agent="quest_generator",
        sub_agent="quest_generator.production_quest",
    )
    assert len(response["payload"]["quests"]) == 5
    assert response["payload"]["quests"][0]["type"] == "production"
    assert response["payload"]["quests"][0]["id"] == 10
    assert response["payload"]["metadata"]["llm"] == "used"
    assert response["payload"]["metadata"]["toolCalls"] == [
        {"name": PRODUCTION_QUEST_SELECTION_TOOL_NAME, "ok": True},
    ]
    assert "서버 전체 오케스트레이터" in llm.prompts[0]
    assert "[OUTPUT_CONTRACT]" in llm.prompts[0]
    assert "퀘스트 생성 도메인 오케스트레이터" in llm.prompts[1]
    assert "[ALLOWED_LEAF_AGENT_IDS]" in llm.prompts[1]
    assert "[OUTPUT_CONTRACT]" in llm.prompts[1]


def test_pipeline_routes_empty_quest_request_through_llm() -> None:
    llm = StubLLM(
        [
            top_agent_decision("quest_generator"),
            leaf_agent_decision("quest_generator.production_quest"),
            QUEST_TOOL_CALL,
            QUEST_TOOL_RESPONSE,
        ]
    )
    pipeline = AgentPipeline(llm=llm)

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-empty-quest",
            "agent": "quest_generator",
        }
    )

    assert_agent_response(
        response,
        agent="quest_generator",
        sub_agent="quest_generator.production_quest",
    )
    assert len(response["payload"]["quests"]) == 5
    assert response["payload"]["quests"][0]["id"] == 10
    assert response["payload"]["metadata"]["llm"] == "used"
    assert response["payload"]["metadata"]["toolCalls"] == [
        {"name": PRODUCTION_QUEST_SELECTION_TOOL_NAME, "ok": True},
    ]
    assert len(llm.prompts) == 4
    assert "팩토리 스페이스 생산 퀘스트 선택 에이전트입니다." in llm.prompts[2]
    tool_call_prefix = '"tool_call":{"name":"quest_generator.select_production_quests"'
    assert tool_call_prefix in llm.prompts[2]
    assert "그대로 따라 쓰지 마세요" in llm.prompts[2]
    assert "[AVAILABLE_QUESTS]" in llm.prompts[2]
    assert "[TOOL_RESULT]" in llm.prompts[-1]


def test_pipeline_accepts_tool_call_with_missing_outer_closing_brace() -> None:
    malformed_tool_call = (
        '{"tool_call":{"name":"quest_generator.select_production_quests",'
        '"args":{"selected_quest_ids":[10,9,8,7,6]}}'
    )
    llm = StubLLM(
        [
            top_agent_decision("quest_generator"),
            malformed_tool_call,
            QUEST_TOOL_RESPONSE,
        ]
    )
    pipeline = AgentPipeline(llm=llm)

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-missing-tool-brace",
            "agent": "quest_generator",
            "payload": {"sub_agent": "quest_generator.production_quest"},
        }
    )

    assert_agent_response(
        response,
        agent="quest_generator",
        sub_agent="quest_generator.production_quest",
    )
    assert response["payload"]["metadata"]["toolCalls"] == [
        {"name": PRODUCTION_QUEST_SELECTION_TOOL_NAME, "ok": True},
    ]


def test_pipeline_rejects_generated_quest_payload_for_production_quest() -> None:
    pipeline = AgentPipeline(
        llm=StubLLM(
            [
                top_agent_decision("quest_generator"),
                leaf_agent_decision("quest_generator.production_quest"),
                '{"quests":[]}',
            ]
        )
    )

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-generated-quest-payload",
            "agent": "quest_generator",
        }
    )

    assert_agent_error(response, code="INVALID_LLM_RESPONSE")
    assert response["agent"] == "quest_generator"


def test_pipeline_uses_prompt_based_operator_guide_sub_agent_routing() -> None:
    response, llm = run_pipeline_scenario(
        PipelineScenario(
            name="prompt routed operator guide",
            agent="operator_guide",
            payload={"question": "How do I use this panel?"},
            request_id="request-operator-guide-routing",
            llm_responses=[
                leaf_agent_decision("operator_guide.machine_help"),
                None,
            ],
        )
    )

    assert_agent_response(
        response,
        agent="operator_guide",
        sub_agent="operator_guide.machine_help",
    )
    assert response["payload"]["topic"] == "machine"
    assert "운영자 가이드 도메인 오케스트레이터" in llm.prompts[0]
    assert "[ALLOWED_LEAF_AGENT_IDS]" in llm.prompts[0]
    assert "[OUTPUT_CONTRACT]" in llm.prompts[0]


def test_pipeline_operator_guide_uses_llm_prompt_with_manual_csv_evidence() -> None:
    response, llm = run_pipeline_scenario(
        PipelineScenario(
            name="operator guide manual qa llm answer",
            agent="operator_guide",
            payload={"question": "제련기는 뭐야?"},
            request_id="request-operator-guide-manual-qa",
            llm_responses=[
                leaf_agent_decision("operator_guide.machine_help"),
                (
                    '{"final_answer":"LLM tutorial answer from CSV evidence.",'
                    '"actions":[],"question":"제련기는 뭐야?",'
                    '"topic":"machine"}'
                ),
            ],
        )
    )

    assert_agent_response(
        response,
        agent="operator_guide",
        sub_agent="operator_guide.machine_help",
    )
    assert (
        response["payload"]["final_answer"] == "LLM tutorial answer from CSV evidence."
    )
    assert response["payload"]["actions"] == []
    assert "answer" not in response["payload"]
    assert "text" not in response["payload"]
    assert len(llm.prompt_messages) == 1
    messages = llm.prompt_messages[0]
    assert messages[0]["role"] == "system"
    assert "tutorial operator inside Factory Space" in messages[0]["content"]
    assert messages[1]["role"] == "user"
    assert "[CSV_EVIDENCE]" in messages[1]["content"]
    assert "equipment_smelter" in messages[1]["content"]
    assert "action_explain_equipment_role" in messages[1]["content"]
    assert "[OUTPUT_CONTRACT]" in messages[1]["content"]


def test_pipeline_routes_valid_explicit_agent_without_top_level_llm() -> None:
    llm = StubLLM([None])
    pipeline = AgentPipeline(llm=llm)

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-explicit-agent-direct",
            "agent": "operator_guide",
            "payload": {
                "question": "How do I use this panel?",
                "sub_agent": "operator_guide.machine_help",
            },
        }
    )

    assert_agent_response(
        response,
        agent="operator_guide",
        sub_agent="operator_guide.machine_help",
    )
    assert llm.prompts == []
    assert len(llm.prompt_messages) == 1


def test_pipeline_routes_explicit_agent_through_top_level_prompt() -> None:
    llm = StubLLM(
        [
            top_agent_decision("process_optimizer"),
            None,
        ]
    )
    pipeline = AgentPipeline(llm=llm)

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-2",
            "agent": "process_optimizer",
            "payload": {"machines": [{"id": "m-1"}]},
        }
    )

    assert_agent_response(response, agent="process_optimizer")
    assert len(llm.prompts) == 2
    assert "서버 전체 오케스트레이터" in llm.prompts[0]
    assert "[REQUEST_HINT]\nagent: process_optimizer" in llm.prompts[0]
    assert "공장 snapshot에서 공정 병목" in llm.prompts[1]


def test_pipeline_treats_explicit_agent_as_top_level_prompt_hint_only() -> None:
    llm = StubLLM([top_agent_decision("operator_guide"), None])
    pipeline = AgentPipeline(llm=llm)

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-explicit-agent-hint-only",
            "agent": "process_optimizer",
            "payload": {
                "sub_agent": "operator_guide.machine_help",
                "question": "How do I use this panel?",
            },
        }
    )

    assert_agent_response(
        response,
        agent="operator_guide",
        sub_agent="operator_guide.machine_help",
    )
    assert "[REQUEST_HINT]\nagent: process_optimizer" in llm.prompts[0]


def test_pipeline_rejects_json_top_level_routing_output() -> None:
    pipeline = AgentPipeline(
        llm=StubLLM(['{"agent":"process_optimizer","reason":"old contract"}'])
    )

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-json-routing-output",
            "agent": "process_optimizer",
            "payload": {"machines": [{"id": "m-1"}]},
        }
    )

    assert_agent_error(response, code="ROUTING_UNAVAILABLE")
    assert response["agent"] == "process_optimizer"


def test_pipeline_includes_safe_details_for_invalid_top_level_routing_output() -> None:
    pipeline = AgentPipeline(llm=StubLLM(["not-a-real-agent"]))

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-invalid-routing-details",
            "agent": "material_generation",
            "payload": {"prompt": "make a material"},
        }
    )

    assert_agent_error(response, code="ROUTING_UNAVAILABLE")
    assert response["agent"] == "material_generation"
    assert response["error"]["details"] == {
        "scope": "top_level",
        "reason": "invalid_model_decision",
        "requestedAgent": "material_generation",
        "routingRawPresent": True,
        "allowedAgentIds": [
            "process_optimizer",
            "operator_guide",
            "quest_generator",
            "material_generation",
            "new_material_generator",
        ],
    }


def test_pipeline_rejects_json_sub_agent_routing_output() -> None:
    pipeline = AgentPipeline(
        llm=StubLLM(
            [
                top_agent_decision("quest_generator"),
                '{"sub_agent":"quest_generator.production_quest","reason":"old contract"}',
            ]
        )
    )

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-json-sub-agent-routing-output",
            "payload": {"message": "create a production objective"},
        }
    )

    assert_agent_error(response, code="ROUTING_UNAVAILABLE")
    assert response["agent"] == "quest_generator"


def test_pipeline_returns_error_when_agent_routing_model_is_unavailable() -> None:
    pipeline = AgentPipeline(llm=StubLLM([None]))

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-3",
            "payload": {"message": "ambiguous request"},
        }
    )

    assert_agent_error(response, code="ROUTING_UNAVAILABLE")
    assert response["error"]["details"]["reason"] == "missing_model_decision"


def test_pipeline_rejects_invalid_explicit_operator_guide_sub_agent() -> None:
    pipeline = AgentPipeline(llm=StubLLM([top_agent_decision("operator_guide")]))

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-invalid-operator-guide-sub-agent",
            "agent": "operator_guide",
            "payload": {"sub_agent": "quest_generator.production_quest"},
        }
    )

    assert_agent_error(response, code="INVALID_SUB_AGENT")


def test_pipeline_rejects_invalid_explicit_quest_sub_agent() -> None:
    pipeline = AgentPipeline(llm=StubLLM([top_agent_decision("quest_generator")]))

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-invalid-quest-sub-agent",
            "agent": "quest_generator",
            "payload": {"sub_agent": "operator_guide.machine_help"},
        }
    )

    assert_agent_error(response, code="INVALID_SUB_AGENT")


def test_pipeline_rejects_invalid_explicit_process_sub_agent() -> None:
    pipeline = AgentPipeline(llm=StubLLM([top_agent_decision("process_optimizer")]))

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-invalid-process-sub-agent",
            "agent": "process_optimizer",
            "payload": {"sub_agent": "operator_guide.machine_help"},
        }
    )

    assert_agent_error(response, code="INVALID_SUB_AGENT")


def test_pipeline_rejects_invalid_explicit_material_sub_agent() -> None:
    pipeline = AgentPipeline(llm=StubLLM([top_agent_decision("material_generation")]))

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-invalid-material-sub-agent",
            "agent": "material_generation",
            "payload": {"sub_agent": "quest_generator.production_quest"},
        }
    )

    assert_agent_error(response, code="INVALID_SUB_AGENT")


def test_pipeline_rejects_invalid_explicit_sub_agent_after_top_level_routing() -> None:
    pipeline = AgentPipeline(llm=StubLLM([top_agent_decision("process_optimizer")]))

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-invalid-routed-sub-agent",
            "payload": {"sub_agent": "operator_guide.machine_help"},
        }
    )

    assert_agent_error(response, code="INVALID_SUB_AGENT")


def test_pipeline_returns_error_for_invalid_envelope() -> None:
    pipeline = AgentPipeline(llm=StubLLM([]))

    response = pipeline.run(
        {
            "type": "wrong.type",
            "request_id": "request-invalid-envelope",
            "payload": {},
        }
    )

    assert_agent_error(response, code="INVALID_ENVELOPE")


def test_cache_key_separates_context() -> None:
    llm = StubLLM(
        [
            top_agent_decision("process_optimizer"),
            '{"result":"site-a"}',
            top_agent_decision("process_optimizer"),
            '{"result":"site-b"}',
        ]
    )
    pipeline = AgentPipeline(llm=llm)

    first = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-cache-a",
            "agent": "process_optimizer",
            "payload": {"machines": []},
            "context": {"site": "a"},
        }
    )
    second = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-cache-b",
            "agent": "process_optimizer",
            "payload": {"machines": []},
            "context": {"site": "b"},
        }
    )

    assert first["payload"]["result"] == "site-a"
    assert second["payload"]["result"] == "site-b"
    assert_agent_response(first, agent="process_optimizer")
    assert_agent_response(second, agent="process_optimizer")
