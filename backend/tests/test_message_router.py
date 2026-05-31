from __future__ import annotations

from agents.pipeline import AgentPipeline
from tests.harness import (
    PipelineScenario,
    StubLLM,
    assert_agent_error,
    assert_agent_response,
    run_pipeline_scenario,
)


def test_pipeline_uses_prompt_based_top_level_routing() -> None:
    response, llm = run_pipeline_scenario(
        PipelineScenario(
            name="prompt routed quest",
            agent=None,
            payload={"message": "create an objective"},
            request_id="request-1",
            llm_responses=[
                '{"agent":"quest_generator","reason":"quest request"}',
                '{"sub_agent":"quest_generator.production_quest","reason":"production"}',
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
    assert "퀘스트 생성 도메인 서브 오케스트레이터" in llm.prompts[1]


def test_pipeline_uses_prompt_based_manual_sub_agent_routing() -> None:
    response, llm = run_pipeline_scenario(
        PipelineScenario(
            name="prompt routed manual",
            agent="manual_qa",
            payload={"question": "How do I use this panel?"},
            request_id="request-manual-routing",
            llm_responses=[
                '{"sub_agent":"manual_qa.machine_help","reason":"machine question"}',
                None,
            ],
        )
    )

    assert_agent_response(
        response,
        agent="manual_qa",
        sub_agent="manual_qa.machine_help",
    )
    assert response["payload"]["topic"] == "machine"
    assert "매뉴얼 Q&A 도메인 서브 오케스트레이터" in llm.prompts[0]


def test_pipeline_uses_explicit_agent_without_top_level_routing_prompt() -> None:
    llm = StubLLM([None])
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
    assert len(llm.prompts) == 1
    assert "공장 snapshot에서 공정 병목" in llm.prompts[0]


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


def test_pipeline_rejects_invalid_explicit_manual_sub_agent() -> None:
    pipeline = AgentPipeline(llm=StubLLM([]))

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-invalid-manual-sub-agent",
            "agent": "manual_qa",
            "payload": {"sub_agent": "quest_generator.production_quest"},
        }
    )

    assert_agent_error(response, code="INVALID_SUB_AGENT")


def test_pipeline_rejects_invalid_explicit_quest_sub_agent() -> None:
    pipeline = AgentPipeline(llm=StubLLM([]))

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-invalid-quest-sub-agent",
            "agent": "quest_generator",
            "payload": {"sub_agent": "manual_qa.machine_help"},
        }
    )

    assert_agent_error(response, code="INVALID_SUB_AGENT")


def test_pipeline_rejects_invalid_explicit_process_sub_agent() -> None:
    pipeline = AgentPipeline(llm=StubLLM([]))

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-invalid-process-sub-agent",
            "agent": "process_optimizer",
            "payload": {"sub_agent": "manual_qa.machine_help"},
        }
    )

    assert_agent_error(response, code="INVALID_SUB_AGENT")


def test_pipeline_rejects_invalid_explicit_material_sub_agent() -> None:
    pipeline = AgentPipeline(llm=StubLLM([]))

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
    pipeline = AgentPipeline(
        llm=StubLLM(['{"agent":"process_optimizer","reason":"process request"}'])
    )

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-invalid-routed-sub-agent",
            "payload": {"sub_agent": "manual_qa.machine_help"},
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
            '{"result":"site-a"}',
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
