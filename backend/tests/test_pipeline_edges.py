from __future__ import annotations

import logging

import pytest

from agents.base import AgentContext, AgentRunResult
from agents.pipeline import AgentPipeline, run_agent_pipeline
from agents.router import AgentRouter
from llm.settings import LLMModelSlot, LLMSettings
from tests.harness import (
    StubLLM,
    assert_agent_error,
    assert_agent_response,
    leaf_agent_decision,
    top_agent_decision,
)


class BrokenFallbackAgent:
    agent_id = "process_optimizer"

    def build_prompt(self, payload: dict[str, object], context: AgentContext) -> str:
        return "broken fallback prompt"

    def fallback(
        self,
        payload: dict[str, object],
        context: AgentContext,
    ) -> AgentRunResult:
        return AgentRunResult(agent=self.agent_id, payload=[])  # type: ignore[arg-type]


def test_pipeline_default_settings_without_api_returns_routing_unavailable() -> None:
    pipeline = AgentPipeline(
        llm_settings=LLMSettings(
            default=LLMModelSlot(name="default", provider="none"),
            fallback1=LLMModelSlot(name="fallback1", provider="none"),
            fallback2=LLMModelSlot(name="fallback2", provider="none"),
        )
    )

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-settings-no-api",
            "agent": "process_optimizer",
            "payload": {"machines": []},
        }
    )

    assert_agent_error(response, code="ROUTING_UNAVAILABLE")


def test_pipeline_default_constructor_without_api_returns_routing_unavailable(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setenv("FACTORY_LLM_DEFAULT_PROVIDER", "none")
    monkeypatch.setenv("FACTORY_LLM_FALLBACK1_PROVIDER", "none")
    monkeypatch.setenv("FACTORY_LLM_FALLBACK2_PROVIDER", "none")
    pipeline = AgentPipeline()

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-default-no-api",
            "agent": "process_optimizer",
            "payload": {"machines": []},
        }
    )

    assert_agent_error(response, code="ROUTING_UNAVAILABLE")


def test_pipeline_uses_settings_slot_adapters_before_deterministic_fallback() -> None:
    settings = LLMSettings(
        default=LLMModelSlot(name="default", provider="none"),
        fallback1=LLMModelSlot(
            name="fallback1",
            provider="openai",
            model="gpt-5.5",
            api_key="key",
        ),
        fallback2=LLMModelSlot(name="fallback2", provider="none"),
    )
    created_slots: list[str] = []
    adapters = {
        "default": StubLLM([top_agent_decision("process_optimizer"), None]),
        "fallback1": StubLLM(['{"summary":"from fallback1"}']),
        "fallback2": StubLLM(['{"summary":"should not be used"}']),
    }

    def create_stub_adapter(slot: LLMModelSlot) -> StubLLM:
        created_slots.append(slot.name)
        return adapters[slot.name]

    pipeline = AgentPipeline(
        llm_settings=settings,
        llm_adapter_factory=create_stub_adapter,
    )

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-settings-fallback1",
            "agent": "process_optimizer",
            "payload": {"machines": []},
        }
    )

    assert created_slots == ["default", "fallback1", "fallback2"]
    assert_agent_response(response, agent="process_optimizer")
    assert response["payload"]["summary"] == "from fallback1"
    assert response["payload"]["metadata"]["llm"] == "used"
    assert response["payload"]["metadata"]["llmSlot"] == "fallback1"
    assert response["payload"]["metadata"]["llmProvider"] == "openai"
    assert response["payload"]["metadata"]["llmModel"] == "gpt-5.5"
    assert response["payload"]["metadata"]["currentModel"] == {
        "slot": "fallback1",
        "provider": "openai",
        "model": "gpt-5.5",
    }
    assert len(adapters["default"].prompts) == 2
    assert len(adapters["fallback1"].prompts) == 1
    assert len(adapters["fallback2"].prompts) == 0


def test_pipeline_uses_fallback2_when_default_and_fallback1_fail() -> None:
    settings = LLMSettings(
        default=LLMModelSlot(name="default", provider="none"),
        fallback1=LLMModelSlot(
            name="fallback1",
            provider="openai",
            model="gpt-5.5",
            api_key="key",
        ),
        fallback2=LLMModelSlot(
            name="fallback2",
            provider="local",
            model="llama3.1:8b",
            base_url="http://localhost:11434/v1",
        ),
    )
    adapters = {
        "default": StubLLM([top_agent_decision("process_optimizer"), None]),
        "fallback1": StubLLM([None]),
        "fallback2": StubLLM(['{"summary":"from fallback2"}']),
    }

    pipeline = AgentPipeline(
        llm_settings=settings,
        llm_adapter_factory=lambda slot: adapters[slot.name],
    )

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-settings-fallback2",
            "agent": "process_optimizer",
            "payload": {"machines": []},
        }
    )

    assert_agent_response(response, agent="process_optimizer")
    assert response["payload"]["summary"] == "from fallback2"
    assert response["payload"]["metadata"]["llmSlot"] == "fallback2"
    assert response["payload"]["metadata"]["llmProvider"] == "local"
    assert response["payload"]["metadata"]["llmModel"] == "llama3.1:8b"
    assert len(adapters["default"].prompts) == 2
    assert len(adapters["fallback1"].prompts) == 1
    assert len(adapters["fallback2"].prompts) == 1


def test_pipeline_clears_stale_model_metadata_when_next_slot_has_no_model() -> None:
    settings = LLMSettings(
        default=LLMModelSlot(name="default", provider="none"),
        fallback1=LLMModelSlot(
            name="fallback1",
            provider="openai",
            model="gpt-5.5",
            api_key="key",
        ),
        fallback2=LLMModelSlot(name="fallback2", provider="none"),
    )
    adapters = {
        "default": StubLLM([top_agent_decision("process_optimizer"), None]),
        "fallback1": StubLLM([None]),
        "fallback2": StubLLM(['{"summary":"from fallback2 without model"}']),
    }
    pipeline = AgentPipeline(
        llm_settings=settings,
        llm_adapter_factory=lambda slot: adapters[slot.name],
    )

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-clear-stale-model",
            "agent": "process_optimizer",
            "payload": {"machines": []},
        }
    )

    assert_agent_response(response, agent="process_optimizer")
    metadata = response["payload"]["metadata"]
    assert metadata["llmSlot"] == "fallback2"
    assert metadata["llmProvider"] == "none"
    assert "llmModel" not in metadata
    assert metadata["currentModel"] == {
        "slot": "fallback2",
        "provider": "none",
    }


def test_pipeline_uses_deterministic_fallback_after_all_slots_fail() -> None:
    settings = LLMSettings(
        default=LLMModelSlot(name="default", provider="none"),
        fallback1=LLMModelSlot(name="fallback1", provider="none"),
        fallback2=LLMModelSlot(name="fallback2", provider="none"),
    )
    adapters = {
        "default": StubLLM([top_agent_decision("process_optimizer"), None]),
        "fallback1": StubLLM([None]),
        "fallback2": StubLLM([None]),
    }

    pipeline = AgentPipeline(
        llm_settings=settings,
        llm_adapter_factory=lambda slot: adapters[slot.name],
    )

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-settings-all-fail",
            "agent": "process_optimizer",
            "payload": {"machines": []},
        }
    )

    assert_agent_response(response, agent="process_optimizer")
    assert response["payload"]["metadata"]["fallback"] is True
    assert len(adapters["default"].prompts) == 2
    assert len(adapters["fallback1"].prompts) == 1
    assert len(adapters["fallback2"].prompts) == 1


def test_pipeline_deterministic_fallback_clears_stale_model_metadata() -> None:
    settings = LLMSettings(
        default=LLMModelSlot(name="default", provider="none"),
        fallback1=LLMModelSlot(
            name="fallback1",
            provider="openai",
            model="gpt-5.5",
            api_key="key",
        ),
        fallback2=LLMModelSlot(name="fallback2", provider="none"),
    )
    adapters = {
        "default": StubLLM([top_agent_decision("process_optimizer"), None]),
        "fallback1": StubLLM([None]),
        "fallback2": StubLLM([None]),
    }
    pipeline = AgentPipeline(
        llm_settings=settings,
        llm_adapter_factory=lambda slot: adapters[slot.name],
    )

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-fallback-clear-stale-model",
            "agent": "process_optimizer",
            "payload": {"machines": []},
        }
    )

    assert_agent_response(response, agent="process_optimizer")
    metadata = response["payload"]["metadata"]
    assert metadata["fallback"] is True
    assert metadata["currentModel"] == {
        "slot": "fallback2",
        "provider": "none",
    }


def test_pipeline_attaches_log_and_fallback_middleware_as_langgraph_nodes() -> None:
    graph = AgentPipeline(llm=StubLLM([])).graph.get_graph()

    assert "agent.middleware.before" in graph.nodes
    assert "agent.middleware.fallback" in graph.nodes
    assert "agent.middleware.after" in graph.nodes


def test_pipeline_records_middleware_logs_and_current_model(
    caplog: pytest.LogCaptureFixture,
) -> None:
    caplog.set_level(logging.INFO, logger="agents.pipeline.runtime")
    settings = LLMSettings(
        default=LLMModelSlot(name="default", provider="none"),
        fallback1=LLMModelSlot(name="fallback1", provider="none"),
        fallback2=LLMModelSlot(name="fallback2", provider="none"),
    )
    adapters = {
        "default": StubLLM([top_agent_decision("process_optimizer"), None]),
        "fallback1": StubLLM([None]),
        "fallback2": StubLLM([None]),
    }
    pipeline = AgentPipeline(
        llm_settings=settings,
        llm_adapter_factory=lambda slot: adapters[slot.name],
    )

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-middleware-log",
            "agent": "process_optimizer",
            "payload": {"machines": []},
        }
    )

    assert_agent_response(response, agent="process_optimizer")
    metadata = response["payload"]["metadata"]
    assert metadata["currentModel"] == {
        "slot": "fallback2",
        "provider": "none",
    }
    assert [
        log["node"] for log in metadata["middlewareLogs"]
    ] == [
        "agent.middleware.before",
        "agent.middleware.fallback",
        "agent.middleware.after",
    ]
    assert [
        log["event"] for log in metadata["middlewareLogs"]
    ] == ["agent_started", "deterministic_fallback", "agent_finished"]
    assert "agent.middleware.before agent_started" in caplog.messages
    assert "agent.middleware.fallback deterministic_fallback" in caplog.messages
    assert "agent.middleware.after agent_finished" in caplog.messages


def test_pipeline_uses_valid_llm_json_response_without_fallback() -> None:
    pipeline = AgentPipeline(
        llm=StubLLM(
            [
                top_agent_decision("process_optimizer"),
                '{"summary":"from model"}',
            ]
        )
    )

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-llm-json",
            "agent": "process_optimizer",
            "payload": {"machines": []},
        }
    )

    assert_agent_response(response, agent="process_optimizer")
    assert response["payload"]["summary"] == "from model"
    assert response["payload"]["metadata"]["llm"] == "used"
    assert response["payload"]["metadata"]["currentModel"] == {
        "slot": "injected",
        "provider": "injected",
    }
    assert [
        log["node"] for log in response["payload"]["metadata"]["middlewareLogs"]
    ] == [
        "agent.middleware.before",
        "agent.middleware.after",
    ]


def test_pipeline_rejects_non_json_llm_response() -> None:
    pipeline = AgentPipeline(
        llm=StubLLM([top_agent_decision("process_optimizer"), "not json"])
    )

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-invalid-llm",
            "agent": "process_optimizer",
            "payload": {"machines": []},
        }
    )

    assert_agent_error(response, code="INVALID_LLM_RESPONSE")


def test_pipeline_rejects_non_object_llm_response() -> None:
    pipeline = AgentPipeline(
        llm=StubLLM([top_agent_decision("process_optimizer"), '["not", "object"]'])
    )

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-invalid-llm-list",
            "agent": "process_optimizer",
            "payload": {"machines": []},
        }
    )

    assert_agent_error(response, code="INVALID_LLM_RESPONSE")


def test_pipeline_returns_routing_unavailable_for_invalid_explicit_agent_without_model_decision() -> None:
    pipeline = AgentPipeline(llm=StubLLM([]))

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-unknown-agent",
            "agent": "unknown",
            "payload": {},
        }
    )

    assert_agent_error(response, code="ROUTING_UNAVAILABLE")
    assert response["agent"] == "unknown"


def test_pipeline_routes_operator_guide_from_llm_top_level_decision() -> None:
    llm = StubLLM(
        [
            top_agent_decision("operator_guide"),
            leaf_agent_decision("operator_guide.recipe_explainer"),
            None,
        ]
    )
    pipeline = AgentPipeline(llm=llm)

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-edge-operator-guide-decision",
            "agent": "process_optimizer",
            "payload": {"question": "How do I run this recipe?"},
        }
    )

    assert_agent_response(
        response,
        agent="operator_guide",
        sub_agent="operator_guide.recipe_explainer",
    )
    assert response["payload"]["topic"] == "recipe"
    assert "서버 전체 오케스트레이터" in llm.prompts[0]
    assert "운영자 가이드 도메인 오케스트레이터" in llm.prompts[1]


def test_pipeline_routes_production_quest_from_llm_leaf_decision() -> None:
    llm = StubLLM(
        [
            top_agent_decision("quest_generator"),
            leaf_agent_decision("quest_generator.production_quest"),
            None,
        ]
    )
    pipeline = AgentPipeline(llm=llm)

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-edge-production-quest-decision",
            "payload": {"message": "Create the next production objective."},
        }
    )

    assert_agent_response(
        response,
        agent="quest_generator",
        sub_agent="quest_generator.production_quest",
    )
    assert response["payload"]["quest"]["type"] == "production"
    assert "퀘스트 생성 도메인 오케스트레이터" in llm.prompts[1]


def test_pipeline_rejects_json_top_level_routing_decision_in_edges() -> None:
    llm = StubLLM(['{"agent":"operator_guide","reason":"old contract"}'])
    pipeline = AgentPipeline(llm=llm)

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-edge-json-top-level-routing",
            "agent": "operator_guide",
            "payload": {"question": "How do I use this panel?"},
        }
    )

    assert_agent_error(response, code="ROUTING_UNAVAILABLE")
    assert response["agent"] == "operator_guide"
    assert len(llm.prompts) == 1


def test_pipeline_rejects_json_sub_agent_routing_decision_in_edges() -> None:
    llm = StubLLM(
        [
            top_agent_decision("quest_generator"),
            '{"sub_agent":"quest_generator.production_quest","reason":"old contract"}',
        ]
    )
    pipeline = AgentPipeline(llm=llm)

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-edge-json-leaf-routing",
            "payload": {"message": "Create the next production objective."},
        }
    )

    assert_agent_error(response, code="ROUTING_UNAVAILABLE")
    assert response["agent"] == "quest_generator"
    assert len(llm.prompts) == 2


def test_pipeline_rejects_invalid_fallback_payload_shape() -> None:
    router = AgentRouter()
    router.register(BrokenFallbackAgent())
    pipeline = AgentPipeline(
        router=router,
        llm=StubLLM([top_agent_decision("process_optimizer"), None]),
    )

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-invalid-fallback",
            "agent": "process_optimizer",
            "payload": {},
        }
    )

    assert_agent_error(response, code="INVALID_AGENT_RESPONSE")


def test_pipeline_cache_hit_skips_second_llm_call() -> None:
    llm = StubLLM(
        [
            top_agent_decision("process_optimizer"),
            '{"summary":"first"}',
            top_agent_decision("process_optimizer"),
        ]
    )
    pipeline = AgentPipeline(llm=llm)
    message = {
        "type": "agent.request",
        "request_id": "request-cache-first",
        "agent": "process_optimizer",
        "payload": {"machines": [{"id": "m-1"}]},
        "context": {"site": "a"},
    }

    first = pipeline.run(message)
    second = pipeline.run({**message, "request_id": "request-cache-second"})

    assert first["payload"]["summary"] == "first"
    assert second["payload"]["summary"] == "first"
    assert second["payload"]["metadata"]["cache"] == "hit"
    assert "middlewareLogs" not in second["payload"]["metadata"]
    assert_agent_response(first, agent="process_optimizer")
    assert_agent_response(second, agent="process_optimizer")
    assert len(llm.prompts) == 3


def test_pipeline_cache_hit_preserves_original_response_metadata() -> None:
    llm = StubLLM(
        [
            top_agent_decision("process_optimizer"),
            '{"summary":"first"}',
            top_agent_decision("process_optimizer"),
        ]
    )
    pipeline = AgentPipeline(llm=llm)
    message = {
        "type": "agent.request",
        "request_id": "request-cache-metadata-first",
        "agent": "process_optimizer",
        "payload": {"machines": [{"id": "m-1"}]},
    }

    first = pipeline.run(message)
    second = pipeline.run({**message, "request_id": "request-cache-metadata-second"})

    assert first["payload"]["metadata"]["llm"] == "used"
    assert second["payload"]["metadata"]["llm"] == "used"
    assert second["payload"]["metadata"]["cache"] == "hit"
    assert "middlewareLogs" not in second["payload"]["metadata"]


def test_agent_pipeline_builds_compiled_graph() -> None:
    graph = AgentPipeline(llm=StubLLM([])).graph

    assert callable(graph.invoke)


def test_run_agent_pipeline_returns_validation_error_for_bad_envelope() -> None:
    response = run_agent_pipeline(
        {
            "type": "wrong.type",
            "request_id": "request-run-agent-invalid",
            "payload": {},
        }
    )

    assert_agent_error(response, code="INVALID_ENVELOPE")


def test_pipeline_validation_error_preserves_raw_correlation_fields() -> None:
    response = run_agent_pipeline(
        {
            "type": "wrong.type",
            "request_id": "request-invalid-correlated",
            "session_id": "session-1",
            "client_id": "client-1",
            "agent": "process_optimizer",
            "payload": {},
        }
    )

    assert_agent_error(response, code="INVALID_ENVELOPE")
    assert response["request_id"] == "request-invalid-correlated"
    assert response["session_id"] == "session-1"
    assert response["client_id"] == "client-1"
    assert response["agent"] == "process_optimizer"
