from __future__ import annotations

from agents.base import AgentContext, AgentRunResult
from agents.pipeline import AgentPipeline, build_agent_graph, run_agent_pipeline
from agents.router import AgentRouter
from tests.harness import StubLlm, assert_agent_error, assert_agent_response


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


def test_pipeline_uses_valid_llm_json_response_without_fallback() -> None:
    pipeline = AgentPipeline(llm=StubLlm(['{"summary":"from model"}']))

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


def test_pipeline_rejects_non_json_llm_response() -> None:
    pipeline = AgentPipeline(llm=StubLlm(["not json"]))

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
    pipeline = AgentPipeline(llm=StubLlm(['["not", "object"]']))

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-invalid-llm-list",
            "agent": "process_optimizer",
            "payload": {"machines": []},
        }
    )

    assert_agent_error(response, code="INVALID_LLM_RESPONSE")


def test_pipeline_returns_unknown_agent_for_invalid_explicit_agent() -> None:
    pipeline = AgentPipeline(llm=StubLlm([]))

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-unknown-agent",
            "agent": "unknown",
            "payload": {},
        }
    )

    assert_agent_error(response, code="UNKNOWN_AGENT")
    assert response["agent"] == "unknown"


def test_pipeline_rejects_invalid_fallback_payload_shape() -> None:
    router = AgentRouter()
    router.register(BrokenFallbackAgent())
    pipeline = AgentPipeline(router=router, llm=StubLlm([None]))

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
    llm = StubLlm(['{"summary":"first"}'])
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
    assert len(llm.prompts) == 1


def test_pipeline_cache_hit_preserves_original_response_metadata() -> None:
    llm = StubLlm(['{"summary":"first"}'])
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


def test_build_agent_graph_returns_compiled_graph() -> None:
    graph = build_agent_graph(llm=StubLlm([]))

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
