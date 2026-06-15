"""LangGraph routing function definitions for the material generation subgraph."""

from __future__ import annotations

from langgraph.graph import END

from agents.material_generation.graph_state import MaterialGraphState


def check_cache_routing(state: MaterialGraphState) -> str:
    """Route based on cached experiment lookup status."""
    if state.get("response"):
        return END
    return "recipe_match"


def check_recipe_routing(state: MaterialGraphState) -> str:
    """Route based on authored recipe match status."""
    if state.get("response"):
        return END
    return "prevalidate"


def check_prevalidate_routing(state: MaterialGraphState) -> str:
    """Route based on deterministic pre-validation status."""
    if state.get("response"):
        return END
    return "classify"


def check_handle_rule_routing(state: MaterialGraphState) -> str:
    """Route based on category fast-fail status."""
    if state.get("response"):
        return END
    return "similarity_context"


def check_validate_result_routing(state: MaterialGraphState) -> str:
    """Route based on LLM proposal validation and retry limits."""
    if state.get("response"):
        return END

    proposal = state.get("proposal")
    attempt = state.get("attempt", 1)

    if not proposal or proposal.proposal_type == "failed" or not proposal.result:
        if attempt <= 3:
            return "llm_propose"
        else:
            return END

    return "deduplicate_material"
