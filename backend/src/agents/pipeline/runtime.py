"""Common AI agent execution pipeline."""

from __future__ import annotations

import json
from collections.abc import Callable
from typing import Any

from langgraph.graph import StateGraph
from langgraph.graph.state import CompiledStateGraph
from pydantic import ValidationError

from agents.base import AgentContext
from agents.manual_qa.agent import MANUAL_QA_SUB_AGENT_IDS, ManualQaAgent
from agents.orchestrator import TOP_LEVEL_AGENT_IDS, OrchestratorAgent
from agents.pipeline.graph_edges import wire_agent_graph
from agents.pipeline.llm_fallback import build_llm_call_slots, invoke_llm_call_slot
from agents.pipeline.state import AgentGraphState
from agents.pipeline.utils import (
    build_cache_key,
    build_validation_error,
    run_fallback,
)
from agents.quest_generator.agent import QUEST_SUB_AGENT_IDS, QuestGeneratorAgent
from agents.router import AgentRouter, UnknownAgentError, create_default_agent_router
from cache.response_cache import ResponseCache
from llm.adapter import LLMAdapter, create_llm_adapter
from llm.settings import LLMModelSlot, LLMSettings
from protocol.errors import build_error_payload
from protocol.messages import (
    AgentErrorEnvelope,
    AgentRequestEnvelope,
    AgentResponseEnvelope,
)


class AgentPipeline:
    """LangGraph-backed execution pipeline for agent requests."""

    def __init__(
        self,
        *,
        router: AgentRouter | None = None,
        cache: ResponseCache | None = None,
        llm: LLMAdapter | None = None,
        llm_settings: LLMSettings | None = None,
        llm_adapter_factory: Callable[[LLMModelSlot], LLMAdapter] = create_llm_adapter,
    ) -> None:
        self.router = router or create_default_agent_router()
        self.cache = cache or ResponseCache()
        self.llm = llm
        self.llm_settings = llm_settings
        self.llm_adapter_factory = llm_adapter_factory
        self.graph = self._build_graph()

    def run(self, message: AgentRequestEnvelope | dict[str, Any]) -> dict[str, Any]:
        """Run one request through the compiled graph."""

        try:
            envelope = (
                message
                if isinstance(message, AgentRequestEnvelope)
                else AgentRequestEnvelope.model_validate(message)
            )
        except ValidationError as exc:
            return build_validation_error(exc, message)

        state = self.graph.invoke({"envelope": envelope})
        return state["responseEnvelope"]


    def _build_graph(self) -> CompiledStateGraph:
        """Build and compile the LangGraph agent pipeline."""

        agent_router = self.router
        response_cache = self.cache
        llm_slots = build_llm_call_slots(
            llm=self.llm,
            settings=self.llm_settings,
            adapter_factory=self.llm_adapter_factory,
        )
        routing_llm = self.llm or llm_slots[0].adapter
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
                routing_raw = routing_llm.invoke(routing_prompt)
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
            routing_raw = routing_llm.invoke(routing_prompt)
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
            routing_raw = routing_llm.invoke(routing_prompt)
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
            cache_key = build_cache_key(
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

        def call_llm_default(state: AgentGraphState) -> AgentGraphState:
            if state.get("error"):
                return {}
            return invoke_llm_call_slot(llm_slots[0], state["prompt"])

        def call_llm_fallback1(state: AgentGraphState) -> AgentGraphState:
            if state.get("error"):
                return {}
            return invoke_llm_call_slot(llm_slots[1], state["prompt"])

        def call_llm_fallback2(state: AgentGraphState) -> AgentGraphState:
            if state.get("error"):
                return {}
            return invoke_llm_call_slot(llm_slots[2], state["prompt"])

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

            metadata = {"llm": "used"}
            if state.get("llmSlot"):
                metadata["llmSlot"] = state["llmSlot"]
            if state.get("llmProvider"):
                metadata["llmProvider"] = state["llmProvider"]
            if state.get("llmModel"):
                metadata["llmModel"] = state["llmModel"]
            return {"responsePayload": payload, "responseMetadata": metadata}

        def build_fallback(state: AgentGraphState) -> AgentGraphState:
            result = run_fallback(agent_router, state)
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
        graph.add_node("call_llm.default", call_llm_default)
        graph.add_node("call_llm.fallback1", call_llm_fallback1)
        graph.add_node("call_llm.fallback2", call_llm_fallback2)
        graph.add_node("parse_llm_response", parse_llm_response)
        graph.add_node("build_fallback", build_fallback)
        graph.add_node("validate_response_schema", validate_response_schema)
        graph.add_node("cache_write", cache_write)
        graph.add_node("build_agent_response", build_agent_response)
        graph.add_node("build_agent_error", build_agent_error)

        wire_agent_graph(graph)
        return graph.compile()



def run_agent_pipeline(message: AgentRequestEnvelope | dict[str, Any]) -> dict[str, Any]:
    """Run one message through a default agent pipeline."""

    try:
        return AgentPipeline().run(message)
    except ValidationError as exc:
        return build_validation_error(exc, message)
