from __future__ import annotations

from agents.pipeline import AgentPipeline
from tests.harness import (
    PipelineScenario,
    StubLLM,
    assert_agent_error,
    assert_agent_response,
    leaf_agent_decision,
    run_pipeline_scenario,
    top_agent_decision,
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
                None,
            ],
        )
    )

    assert_agent_response(
        response,
        agent="quest_generator",
        sub_agent="quest_generator.production_quest",
    )
    assert response["payload"]["quest"]["type"] == "production"
    assert "서버 전체 오케스트레이터" in llm.prompts[0]
    assert "[OUTPUT_CONTRACT]" in llm.prompts[0]
    assert "퀘스트 생성 도메인 오케스트레이터" in llm.prompts[1]
    assert "[ALLOWED_LEAF_AGENT_IDS]" in llm.prompts[1]
    assert "[OUTPUT_CONTRACT]" in llm.prompts[1]


def test_pipeline_uses_prompt_based_operator_guide_sub_agent_routing() -> None:
    response, llm = run_pipeline_scenario(
        PipelineScenario(
            name="prompt routed operator guide",
            agent="operator_guide",
            payload={"question": "How do I use this panel?"},
            request_id="request-operator-guide-routing",
            llm_responses=[
                top_agent_decision("operator_guide"),
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
    assert "서버 전체 오케스트레이터" in llm.prompts[0]
    assert "운영자 가이드 도메인 오케스트레이터" in llm.prompts[1]
    assert "[ALLOWED_LEAF_AGENT_IDS]" in llm.prompts[1]
    assert "[OUTPUT_CONTRACT]" in llm.prompts[1]


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
    pipeline = AgentPipeline(llm=StubLLM([top_agent_decision("new_material_generator")]))

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-invalid-material-sub-agent",
            "agent": "new_material_generator",
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
