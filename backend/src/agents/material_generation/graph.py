"""재료 생성 워크플로를 오케스트레이션하는 LangGraph 서브그래프입니다."""

from __future__ import annotations

from langgraph.graph import END, StateGraph

from agents.material_generation.graph_state import MaterialGraphState
from agents.material_generation.nodes import (
    classify_node,
    deduplicate_material_node,
    handle_rule_node,
    llm_propose_node,
    lookup_cache_node,
    normalize_node,
    prevalidate_node,
    recipe_match_node,
    register_material_node,
    similarity_context_node,
    validate_result_node,
)
from agents.material_generation.routing import (
    check_cache_routing,
    check_handle_rule_routing,
    check_prevalidate_routing,
    check_recipe_routing,
    check_validate_result_routing,
)


def build_material_subgraph() -> StateGraph:
    """재료 생성 StateGraph 서브그래프를 컴파일하고 반환합니다."""
    builder = StateGraph(MaterialGraphState)

    # 노드 추가
    builder.add_node("normalize", normalize_node)
    builder.add_node("lookup_cache", lookup_cache_node)
    builder.add_node("recipe_match", recipe_match_node)
    builder.add_node("prevalidate", prevalidate_node)
    builder.add_node("classify", classify_node)
    builder.add_node("handle_rule", handle_rule_node)
    builder.add_node("similarity_context", similarity_context_node)
    builder.add_node("llm_propose", llm_propose_node)
    builder.add_node("validate_result", validate_result_node)
    builder.add_node("deduplicate_material", deduplicate_material_node)
    builder.add_node("register_material", register_material_node)

    # 진입점 설정
    builder.set_entry_point("normalize")

    # 선형 엣지(Edge) 및 조건부 라우팅 추가
    builder.add_edge("normalize", "lookup_cache")

    builder.add_conditional_edges(
        "lookup_cache",
        check_cache_routing,
        {
            END: END,
            "recipe_match": "recipe_match",
        },
    )

    builder.add_conditional_edges(
        "recipe_match",
        check_recipe_routing,
        {
            END: END,
            "prevalidate": "prevalidate",
        },
    )

    builder.add_conditional_edges(
        "prevalidate",
        check_prevalidate_routing,
        {
            END: END,
            "classify": "classify",
        },
    )

    builder.add_edge("classify", "handle_rule")

    builder.add_conditional_edges(
        "handle_rule",
        check_handle_rule_routing,
        {
            END: END,
            "similarity_context": "similarity_context",
        },
    )

    builder.add_edge("similarity_context", "llm_propose")
    builder.add_edge("llm_propose", "validate_result")

    builder.add_conditional_edges(
        "validate_result",
        check_validate_result_routing,
        {
            END: END,
            "llm_propose": "llm_propose",
            "deduplicate_material": "deduplicate_material",
        },
    )

    builder.add_edge("deduplicate_material", "register_material")
    builder.add_edge("register_material", END)

    return builder.compile()


# 컴파일된 서브그래프 내보내기
material_subgraph = build_material_subgraph()
