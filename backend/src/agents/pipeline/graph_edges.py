"""LangGraph edge wiring and routing predicates."""

from __future__ import annotations

from typing import Literal

from langgraph.graph import END, START, StateGraph

from agents.pipeline.state import AgentGraphState, TopRoute


def wire_agent_graph(graph: StateGraph) -> None:
    graph.add_edge(START, "build_context")
    graph.add_edge("build_context", "validate_envelope")
    graph.add_edge("validate_envelope", "route_top_agent")
    graph.add_conditional_edges(
        "route_top_agent",
        route_selected_agent,
        {
            "process_optimizer": "validate_process_payload",
            "manual_qa": "manual_qa.route_sub_agent",
            "quest_generator": "quest_generator.route_sub_agent",
            "new_material_generator": "validate_material_payload",
            "error": "build_agent_error",
        },
    )
    for node in (
        "validate_process_payload",
        "manual_qa.route_sub_agent",
        "quest_generator.route_sub_agent",
        "validate_material_payload",
    ):
        graph.add_conditional_edges(
            node,
            route_sub_agent_result,
            {
                "valid": "cache_lookup",
                "error": "build_agent_error",
            },
        )
    graph.add_conditional_edges(
        "cache_lookup",
        route_cache_result,
        {
            "hit": "build_cached_response",
            "miss": "build_prompt",
        },
    )
    graph.add_edge("build_cached_response", "build_agent_response")
    graph.add_edge("build_prompt", "call_llm.default")
    graph.add_conditional_edges(
        "call_llm.default",
        route_llm_result,
        {
            "valid": "parse_llm_response",
            "fallback": "call_llm.fallback1",
            "error": "build_agent_error",
        },
    )
    graph.add_conditional_edges(
        "call_llm.fallback1",
        route_llm_result,
        {
            "valid": "parse_llm_response",
            "fallback": "call_llm.fallback2",
            "error": "build_agent_error",
        },
    )
    graph.add_conditional_edges(
        "call_llm.fallback2",
        route_llm_result,
        {
            "valid": "parse_llm_response",
            "fallback": "build_fallback",
            "error": "build_agent_error",
        },
    )
    graph.add_conditional_edges(
        "parse_llm_response",
        route_response_validation,
        {
            "valid": "validate_response_schema",
            "error": "build_agent_error",
        },
    )
    graph.add_edge("build_fallback", "validate_response_schema")
    graph.add_conditional_edges(
        "validate_response_schema",
        route_response_validation,
        {
            "valid": "cache_write",
            "error": "build_agent_error",
        },
    )
    graph.add_edge("cache_write", "build_agent_response")
    graph.add_edge("build_agent_response", END)
    graph.add_edge("build_agent_error", END)


def route_selected_agent(state: AgentGraphState) -> TopRoute:
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


def route_cache_result(state: AgentGraphState) -> Literal["hit", "miss"]:
    return "hit" if state.get("cachedPayload") is not None else "miss"


def route_sub_agent_result(state: AgentGraphState) -> Literal["valid", "error"]:
    return "error" if state.get("error") else "valid"


def route_llm_result(state: AgentGraphState) -> Literal["valid", "fallback", "error"]:
    if state.get("error"):
        return "error"
    return "valid" if state.get("llmRaw") else "fallback"


def route_response_validation(state: AgentGraphState) -> Literal["valid", "error"]:
    return "error" if state.get("error") else "valid"
