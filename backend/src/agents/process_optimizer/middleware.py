"""Process Optimizer 요청을 그래프 실행 전에 검사하고 보완합니다.

공개 요청 형식을 검증하고, 주기적으로 받은 공장 상태를 세션 메모리에 저장하며,
분석 요청에 빠진 상태 정보를 최신 메모리 값으로 채웁니다.
"""

from __future__ import annotations

from typing import Any

from agents.base import AgentContext
from agents.process_optimizer.analyzer import FactoryStateAnalyzerTool
from agents.process_optimizer.schemas import FactoryState, ProcessOptimizerPayload
from agents.process_optimizer.session_memory import process_optimizer_memory
from agents.process_optimizer.subquest_alert import SubquestAlertBuilder
from protocol.errors import build_error_payload


def validate_process_optimizer_payload(
    payload: dict[str, Any],
) -> dict[str, Any] | None:
    """공개 요청 payload가 v2 그래프 계약을 만족하는지 검사합니다.

    Args:
        payload: WebSocket 요청에서 전달된 Process Optimizer 입력입니다.

    Returns:
        검증에 실패하면 구조화된 오류 payload를, 통과하면 ``None``을 반환합니다.
    """
    explicit_sub_agent = payload.get("sub_agent")
    if explicit_sub_agent is not None and explicit_sub_agent != "process_optimizer":
        return build_error_payload(
            "INVALID_SUB_AGENT",
            "Explicit sub_agent is not valid for process_optimizer.",
            details={"sub_agent": explicit_sub_agent},
        )

    try:
        ProcessOptimizerPayload.model_validate(payload)

        revision = payload.get("factoryRevision")
        if revision is not None:
            if not isinstance(revision, int) or isinstance(revision, bool):
                raise ValueError("factoryRevision must be an integer")
            if revision < 0:
                raise ValueError("factoryRevision cannot be negative")

        factory_state = payload.get("factory_state")
        if factory_state is not None:
            if not isinstance(factory_state, dict):
                raise ValueError("factory_state must be a dictionary")
            FactoryState.model_validate(factory_state)

    except Exception as exc:
        return build_error_payload(
            "INVALID_REQUEST_PAYLOAD",
            f"Request payload validation failed: {exc}",
        )

    return None


def build_state_update_response(
    context: AgentContext,
    payload: dict[str, Any],
) -> dict[str, Any]:
    """주기 상태 업데이트를 세션 메모리에 저장하고 확인 응답을 만듭니다.

    Args:
        context: 세션 식별자를 포함한 현재 에이전트 실행 문맥입니다.
        payload: 최신 공장 상태와 revision을 포함한 요청 데이터입니다.

    Returns:
        저장 성공 상태와 응답 메타데이터를 담은 파이프라인 결과입니다.
    """
    factory_state = payload.get("factory_state")
    revision = payload.get("factoryRevision")
    if revision is None:
        revision = 0

    process_optimizer_memory.update(context.session_id, factory_state, revision)

    subquest_mode = payload.get("subquest_mode")
    if subquest_mode is None:
        subquest_mode = True

    goal = payload.get("goal") or "balance"

    analyzer = FactoryStateAnalyzerTool()
    report = analyzer.analyze(factory_state, factory_revision=revision, goal=goal)

    alert_builder = SubquestAlertBuilder()
    alert = alert_builder.build_alert(
        report, factory_state, subquest_mode=subquest_mode
    )

    response_payload = {
        "status": "success",
        "factoryRevision": revision,
        "goal": goal,
        "optimization_alert": alert.model_dump(),
    }
    return {
        "responsePayload": response_payload,
        "responseMetadata": {"memory": "updated"},
    }


def build_graph_payload_with_memory(
    context: AgentContext,
    payload: dict[str, Any],
) -> dict[str, Any]:
    """그래프 입력에 빠진 공장 상태나 revision을 세션 메모리로 보완합니다.

    Args:
        context: 최신 상태를 찾는 데 사용할 세션 문맥입니다.
        payload: 그래프에 전달할 원본 요청 데이터입니다.

    Returns:
        필요한 경우 메모리 값이 추가된 새로운 payload 딕셔너리입니다.
    """
    graph_payload = dict(payload)

    if "factory_state" not in graph_payload:
        remembered_state = process_optimizer_memory.get_state(context.session_id)
        if remembered_state:
            graph_payload["factory_state"] = remembered_state

    if graph_payload.get("factoryRevision") is None:
        remembered_revision = process_optimizer_memory.get_revision(context.session_id)
        if remembered_revision:
            graph_payload["factoryRevision"] = remembered_revision

    return graph_payload
