"""재료 생성 서브그래프를 위한 LangGraph 라우팅 함수 정의입니다."""

from __future__ import annotations

from langgraph.graph import END

from agents.material_generation.graph_state import MaterialGraphState


def check_cache_routing(state: MaterialGraphState) -> str:
    """캐시된 실험 조회 상태에 따라 라우팅합니다."""
    if state.get("response"):
        return END
    return "recipe_match"


def check_recipe_routing(state: MaterialGraphState) -> str:
    """작성된 레시피 일치 상태에 따라 라우팅합니다."""
    if state.get("response"):
        return END
    return "prevalidate"


def check_prevalidate_routing(state: MaterialGraphState) -> str:
    """결정론적 사전 검증 상태에 따라 라우팅합니다."""
    if state.get("response"):
        return END
    return "classify"


def check_handle_rule_routing(state: MaterialGraphState) -> str:
    """카테고리 패스트-실패(Fast-fail) 상태에 따라 라우팅합니다."""
    if state.get("response"):
        return END
    return "similarity_context"


def check_validate_result_routing(state: MaterialGraphState) -> str:
    """LLM 제안 검증 및 재시도 제한에 따라 라우팅합니다."""
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
