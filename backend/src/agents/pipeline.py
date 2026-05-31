"""Common AI agent execution pipeline."""

from __future__ import annotations

import hashlib
import json
from typing import Any, Literal, TypedDict

from langgraph.graph import END, START, StateGraph
from langgraph.graph.state import CompiledStateGraph
from pydantic import ValidationError

from agents.base import AgentContext, AgentRunResult
from agents.manual_qa.agent import MANUAL_QA_SUB_AGENT_IDS, ManualQaAgent
from agents.orchestrator import TOP_LEVEL_AGENT_IDS, OrchestratorAgent
from agents.quest_generator.agent import QUEST_SUB_AGENT_IDS, QuestGeneratorAgent
from agents.router import AgentRouter, UnknownAgentError, create_default_agent_router
from cache.response_cache import ResponseCache
from llm.adapter import LlmAdapter
from protocol.errors import build_error_payload
from protocol.messages import (
    AgentErrorEnvelope,
    AgentRequestEnvelope,
    AgentResponseEnvelope,
)

TopRoute = Literal[
    "process_optimizer",
    "manual_qa",
    "quest_generator",
    "new_material_generator",
    "error",
]


class AgentGraphState(TypedDict, total=False):
    """Shared LangGraph state for one agent request."""

    envelope: AgentRequestEnvelope
    context: AgentContext
    selectedAgent: str
    selectedSubAgent: str
    typedPayload: dict[str, Any]
    cacheKey: str
    cachedPayload: dict[str, Any]
    cachedMetadata: dict[str, Any]
    prompt: str
    routingPrompt: str
    routingRaw: str | None
    llmRaw: str | None
    fallbackReason: str
    responsePayload: dict[str, Any]
    responseMetadata: dict[str, Any]
    streams: list[dict[str, Any]]
    error: dict[str, Any]
    responseEnvelope: dict[str, Any]


class AgentPipeline:
    """LangGraph-backed execution pipeline for agent requests."""

    def __init__(
        self,
        *,
        router: AgentRouter | None = None,
        cache: ResponseCache | None = None,
        llm: LlmAdapter | None = None,
    ) -> None:
        self.router = router or create_default_agent_router()
        self.cache = cache or ResponseCache()
        self.llm = llm or LlmAdapter()
        self.graph = build_agent_graph(
            router=self.router,
            cache=self.cache,
            llm=self.llm,
        )

    def run(self, message: AgentRequestEnvelope | dict[str, Any]) -> dict[str, Any]:
        """Run one request through the compiled graph."""

        try:
            envelope = (
                message
                if isinstance(message, AgentRequestEnvelope)
                else AgentRequestEnvelope.model_validate(message)
            )
        except ValidationError as exc:
            return _build_validation_error(exc)

        state = self.graph.invoke({"envelope": envelope})
        return state["responseEnvelope"]


def build_agent_graph(
    *,
    router: AgentRouter | None = None,
    cache: ResponseCache | None = None,
    llm: LlmAdapter | None = None,
) -> CompiledStateGraph:
    """Build and compile the LangGraph agent pipeline."""

    agent_router = router or create_default_agent_router()
    response_cache = cache or ResponseCache()
    llm_adapter = llm or LlmAdapter()
    orchestrator = OrchestratorAgent()
    manual_qa = ManualQaAgent()
    quest_generator = QuestGeneratorAgent()

    def build_context(state: AgentGraphState) -> AgentGraphState:
        envelope = state["envelope"]
        return {
            "context": AgentContext(
                request_id=envelope.request_id,
                session_id=envelope.session_id,
                client_id=envelope.client_id,
                metadata=envelope.context,
            ),
            "typedPayload": envelope.payload,
            "streams": [],
        }

    def validate_envelope(state: AgentGraphState) -> AgentGraphState:
        envelope = state["envelope"]
        if envelope.type != "agent.request":
            return {
                "error": build_error_payload(
                    "INVALID_MESSAGE_TYPE",
                    "Only agent.request messages can enter the agent pipeline.",
                    details={"type": envelope.type},
                )
            }
        if not isinstance(envelope.payload, dict):
            return {
                "error": build_error_payload(
                    "INVALID_PAYLOAD",
                    "Agent request payload must be an object.",
                )
            }
        return {}

    def route_top_agent(state: AgentGraphState) -> AgentGraphState:
        if state.get("error"):
            return {}

        envelope = state["envelope"]
        context = state["context"]
        payload = state["typedPayload"]
        selected = envelope.agent
        if selected is None:
            routing_prompt = orchestrator.build_routing_prompt(payload, context)
            routing_raw = llm_adapter.invoke(routing_prompt)
            selected = orchestrator.parse_agent_selection(routing_raw)
            if selected is None:
                return {
                    "routingPrompt": routing_prompt,
                    "routingRaw": routing_raw,
                    "error": build_error_payload(
                        "ROUTING_UNAVAILABLE",
                        "Top-level agent routing requires a valid orchestrator model decision.",
                    ),
                }

        if selected not in TOP_LEVEL_AGENT_IDS:
            return {
                "selectedAgent": selected,
                "error": build_error_payload(
                    "UNKNOWN_AGENT",
                    f"Unknown top-level agent: {selected}",
                    details={"agent": selected},
                ),
            }
        return {"selectedAgent": selected}

    def validate_process_payload(state: AgentGraphState) -> AgentGraphState:
        explicit_sub_agent = state["typedPayload"].get("sub_agent")
        if explicit_sub_agent is not None and explicit_sub_agent != "process_optimizer":
            return {
                "error": build_error_payload(
                    "INVALID_SUB_AGENT",
                    "Explicit sub_agent is not valid for process_optimizer.",
                    details={"sub_agent": explicit_sub_agent},
                )
            }
        return {"selectedSubAgent": "process_optimizer"}

    def validate_material_payload(state: AgentGraphState) -> AgentGraphState:
        explicit_sub_agent = state["typedPayload"].get("sub_agent")
        if (
            explicit_sub_agent is not None
            and explicit_sub_agent != "new_material_generator"
        ):
            return {
                "error": build_error_payload(
                    "INVALID_SUB_AGENT",
                    "Explicit sub_agent is not valid for new_material_generator.",
                    details={"sub_agent": explicit_sub_agent},
                )
            }
        return {"selectedSubAgent": "new_material_generator"}

    def route_manual_sub_agent(state: AgentGraphState) -> AgentGraphState:
        explicit_sub_agent = state["typedPayload"].get("sub_agent")
        if explicit_sub_agent is not None:
            if (
                isinstance(explicit_sub_agent, str)
                and explicit_sub_agent in MANUAL_QA_SUB_AGENT_IDS
            ):
                return {"selectedSubAgent": explicit_sub_agent}
            return {
                "error": build_error_payload(
                    "INVALID_SUB_AGENT",
                    "Explicit sub_agent is not valid for manual_qa.",
                    details={"sub_agent": explicit_sub_agent},
                )
            }

        routing_prompt = manual_qa.build_routing_prompt(
            state["typedPayload"],
            state["context"],
        )
        routing_raw = llm_adapter.invoke(routing_prompt)
        selected = manual_qa.parse_sub_agent_selection(routing_raw)
        if selected is None:
            return {
                "routingPrompt": routing_prompt,
                "routingRaw": routing_raw,
                "error": build_error_payload(
                    "ROUTING_UNAVAILABLE",
                    "Manual Q&A sub-agent routing requires a valid model decision.",
                ),
            }
        return {"selectedSubAgent": selected}

    def route_quest_sub_agent(state: AgentGraphState) -> AgentGraphState:
        explicit_sub_agent = state["typedPayload"].get("sub_agent")
        if explicit_sub_agent is not None:
            if (
                isinstance(explicit_sub_agent, str)
                and explicit_sub_agent in QUEST_SUB_AGENT_IDS
            ):
                return {"selectedSubAgent": explicit_sub_agent}
            return {
                "error": build_error_payload(
                    "INVALID_SUB_AGENT",
                    "Explicit sub_agent is not valid for quest_generator.",
                    details={"sub_agent": explicit_sub_agent},
                )
            }

        routing_prompt = quest_generator.build_routing_prompt(
            state["typedPayload"],
            state["context"],
        )
        routing_raw = llm_adapter.invoke(routing_prompt)
        selected = quest_generator.parse_sub_agent_selection(routing_raw)
        if selected is None:
            return {
                "routingPrompt": routing_prompt,
                "routingRaw": routing_raw,
                "error": build_error_payload(
                    "ROUTING_UNAVAILABLE",
                    "Quest sub-agent routing requires a valid model decision.",
                ),
            }
        return {"selectedSubAgent": selected}

    def cache_lookup(state: AgentGraphState) -> AgentGraphState:
        cache_key = _cache_key(
            state["selectedAgent"],
            state["selectedSubAgent"],
            state["typedPayload"],
            state["context"],
        )
        cached_entry = response_cache.get_entry(cache_key)
        output: AgentGraphState = {"cacheKey": cache_key}
        if cached_entry is not None:
            output["cachedPayload"] = cached_entry.payload
            output["cachedMetadata"] = cached_entry.metadata
        return output

    def build_cached_response(state: AgentGraphState) -> AgentGraphState:
        return {
            "responsePayload": state["cachedPayload"],
            "responseMetadata": {
                **state.get("cachedMetadata", {}),
                "cache": "hit",
            },
        }

    def build_prompt(state: AgentGraphState) -> AgentGraphState:
        try:
            agent = agent_router.get(state["selectedSubAgent"])
        except UnknownAgentError:
            return {
                "error": build_error_payload(
                    "UNKNOWN_AGENT",
                    f"Unknown leaf agent: {state['selectedSubAgent']}",
                    details={"agent": state["selectedSubAgent"]},
                )
            }
        return {"prompt": agent.build_prompt(state["typedPayload"], state["context"])}

    def call_llm(state: AgentGraphState) -> AgentGraphState:
        if state.get("error"):
            return {}
        return {"llmRaw": llm_adapter.invoke(state["prompt"])}

    def parse_llm_response(state: AgentGraphState) -> AgentGraphState:
        raw = state.get("llmRaw")
        if not raw:
            return {}

        try:
            payload = json.loads(raw)
        except json.JSONDecodeError:
            return {
                "error": build_error_payload(
                    "INVALID_LLM_RESPONSE",
                    "LLM response must be a JSON object.",
                )
            }

        if not isinstance(payload, dict):
            return {
                "error": build_error_payload(
                    "INVALID_LLM_RESPONSE",
                    "LLM response must be a JSON object.",
                )
            }

        return {"responsePayload": payload, "responseMetadata": {"llm": "used"}}

    def build_fallback(state: AgentGraphState) -> AgentGraphState:
        result = _run_fallback(agent_router, state)
        return {
            "fallbackReason": "llm_unavailable",
            "responsePayload": result.payload,
            "responseMetadata": result.metadata,
        }

    def validate_response_schema(state: AgentGraphState) -> AgentGraphState:
        payload = state.get("responsePayload")
        if not isinstance(payload, dict):
            return {
                "error": build_error_payload(
                    "INVALID_AGENT_RESPONSE",
                    "Agent response payload must be an object.",
                )
            }
        return {}

    def cache_write(state: AgentGraphState) -> AgentGraphState:
        response_cache.set(
            state["cacheKey"],
            state["responsePayload"],
            state.get("responseMetadata", {}),
        )
        return {}

    def build_agent_response(state: AgentGraphState) -> AgentGraphState:
        envelope = state["envelope"]
        response = AgentResponseEnvelope(
            request_id=envelope.request_id,
            session_id=envelope.session_id,
            client_id=envelope.client_id,
            agent=state["selectedAgent"],
            payload={
                **state["responsePayload"],
                "metadata": {
                    **state.get("responseMetadata", {}),
                    "selectedAgent": state["selectedAgent"],
                    "selectedSubAgent": state["selectedSubAgent"],
                },
            },
            streams=state.get("streams", []),
        )
        return {"responseEnvelope": response.model_dump(mode="json")}

    def build_agent_error(state: AgentGraphState) -> AgentGraphState:
        envelope = state["envelope"]
        response = AgentErrorEnvelope(
            request_id=envelope.request_id,
            session_id=envelope.session_id,
            client_id=envelope.client_id,
            agent=state.get("selectedAgent") or envelope.agent,
            error=state.get("error")
            or build_error_payload("AGENT_PIPELINE_ERROR", "Agent pipeline failed."),
        )
        return {"responseEnvelope": response.model_dump(mode="json")}

    graph = StateGraph(AgentGraphState)
    graph.add_node("build_context", build_context)
    graph.add_node("validate_envelope", validate_envelope)
    graph.add_node("route_top_agent", route_top_agent)
    graph.add_node("validate_process_payload", validate_process_payload)
    graph.add_node("manual_qa.route_sub_agent", route_manual_sub_agent)
    graph.add_node("quest_generator.route_sub_agent", route_quest_sub_agent)
    graph.add_node("validate_material_payload", validate_material_payload)
    graph.add_node("cache_lookup", cache_lookup)
    graph.add_node("build_cached_response", build_cached_response)
    graph.add_node("build_prompt", build_prompt)
    graph.add_node("call_llm", call_llm)
    graph.add_node("parse_llm_response", parse_llm_response)
    graph.add_node("build_fallback", build_fallback)
    graph.add_node("validate_response_schema", validate_response_schema)
    graph.add_node("cache_write", cache_write)
    graph.add_node("build_agent_response", build_agent_response)
    graph.add_node("build_agent_error", build_agent_error)

    graph.add_edge(START, "build_context")
    graph.add_edge("build_context", "validate_envelope")
    graph.add_edge("validate_envelope", "route_top_agent")
    graph.add_conditional_edges(
        "route_top_agent",
        _route_selected_agent,
        {
            "process_optimizer": "validate_process_payload",
            "manual_qa": "manual_qa.route_sub_agent",
            "quest_generator": "quest_generator.route_sub_agent",
            "new_material_generator": "validate_material_payload",
            "error": "build_agent_error",
        },
    )
    graph.add_conditional_edges(
        "validate_process_payload",
        _route_sub_agent_result,
        {
            "valid": "cache_lookup",
            "error": "build_agent_error",
        },
    )
    graph.add_conditional_edges(
        "manual_qa.route_sub_agent",
        _route_sub_agent_result,
        {
            "valid": "cache_lookup",
            "error": "build_agent_error",
        },
    )
    graph.add_conditional_edges(
        "quest_generator.route_sub_agent",
        _route_sub_agent_result,
        {
            "valid": "cache_lookup",
            "error": "build_agent_error",
        },
    )
    graph.add_conditional_edges(
        "validate_material_payload",
        _route_sub_agent_result,
        {
            "valid": "cache_lookup",
            "error": "build_agent_error",
        },
    )
    graph.add_conditional_edges(
        "cache_lookup",
        _route_cache_result,
        {
            "hit": "build_cached_response",
            "miss": "build_prompt",
        },
    )
    graph.add_edge("build_cached_response", "build_agent_response")
    graph.add_edge("build_prompt", "call_llm")
    graph.add_conditional_edges(
        "call_llm",
        _route_llm_result,
        {
            "valid": "parse_llm_response",
            "fallback": "build_fallback",
            "error": "build_agent_error",
        },
    )
    graph.add_conditional_edges(
        "parse_llm_response",
        _route_response_validation,
        {
            "valid": "validate_response_schema",
            "error": "build_agent_error",
        },
    )
    graph.add_edge("build_fallback", "validate_response_schema")
    graph.add_conditional_edges(
        "validate_response_schema",
        _route_response_validation,
        {
            "valid": "cache_write",
            "error": "build_agent_error",
        },
    )
    graph.add_edge("cache_write", "build_agent_response")
    graph.add_edge("build_agent_response", END)
    graph.add_edge("build_agent_error", END)
    return graph.compile()


def _route_selected_agent(state: AgentGraphState) -> TopRoute:
    if state.get("error"):
        return "error"
    selected_agent = state.get("selectedAgent")
    if selected_agent in {
        "process_optimizer",
        "manual_qa",
        "quest_generator",
        "new_material_generator",
    }:
        return selected_agent  # type: ignore[return-value]
    return "error"


def _route_cache_result(state: AgentGraphState) -> Literal["hit", "miss"]:
    return "hit" if state.get("cachedPayload") is not None else "miss"


def _route_sub_agent_result(state: AgentGraphState) -> Literal["valid", "error"]:
    return "error" if state.get("error") else "valid"


def _route_llm_result(state: AgentGraphState) -> Literal["valid", "fallback", "error"]:
    if state.get("error"):
        return "error"
    return "valid" if state.get("llmRaw") else "fallback"


def _route_response_validation(state: AgentGraphState) -> Literal["valid", "error"]:
    return "error" if state.get("error") else "valid"


def _run_fallback(
    router: AgentRouter,
    state: AgentGraphState,
) -> AgentRunResult:
    agent = router.get(state["selectedSubAgent"])
    return agent.fallback(state["typedPayload"], state["context"])


def _cache_key(
    agent: str,
    sub_agent: str,
    payload: dict[str, Any],
    context: AgentContext,
) -> str:
    raw = json.dumps(
        {
            "agent": agent,
            "sub_agent": sub_agent,
            "payload": payload,
            "context": {
                "session_id": context.session_id,
                "client_id": context.client_id,
                "metadata": context.metadata,
            },
        },
        sort_keys=True,
        default=str,
    )
    return hashlib.sha256(raw.encode("utf-8")).hexdigest()


def run_agent_pipeline(message: AgentRequestEnvelope | dict[str, Any]) -> dict[str, Any]:
    """Run one message through a default agent pipeline."""

    try:
        return AgentPipeline().run(message)
    except ValidationError as exc:
        return _build_validation_error(exc)


def _build_validation_error(exc: ValidationError) -> dict[str, Any]:
    error = AgentErrorEnvelope(
        error=build_error_payload(
            "INVALID_ENVELOPE",
            "Agent request envelope validation failed.",
            details={"errors": exc.errors()},
        )
    )
    return error.model_dump(mode="json")
