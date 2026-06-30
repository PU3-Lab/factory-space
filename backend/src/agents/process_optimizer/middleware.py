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
from agents.process_optimizer.snapshot_store import process_optimizer_snapshot_store
from agents.process_optimizer.subquest_alert import SubquestAlertBuilder
from agents.process_optimizer.subquest_request_store import subquest_request_store
from protocol.errors import build_error_payload


def validate_process_optimizer_payload(
    payload: dict[str, Any],
    context: AgentContext | None = None,
) -> dict[str, Any] | None:
    """공개 요청 payload가 v2 그래프 계약을 만족하는지 검사합니다.

    Args:
        payload: WebSocket 요청에서 전달된 Process Optimizer 입력입니다.
        context: 스냅샷 요청 ID의 세션과 클라이언트를 확인할 실행 문맥입니다.

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

    if (
        payload.get("operation") == "state_update"
        and payload.get("request_source") == "subquest_check"
    ):
        snapshot_request_id = payload.get("snapshot_request_id")
        if not snapshot_request_id:
            return build_error_payload(
                "INVALID_REQUEST_PAYLOAD",
                "snapshot_request_id is required for a subquest_check state update.",
            )
        if context is None or not subquest_request_store.matches(
            snapshot_request_id,
            context.session_id or "default-session",
            context.client_id or "unreal",
        ):
            return build_error_payload(
                "INVALID_SNAPSHOT_REQUEST",
                "The snapshot request is unknown, expired, or belongs to another session.",
            )

    return None


def build_subquest_check_response(
    context: AgentContext,
    payload: dict[str, Any],
) -> dict[str, Any]:
    """Unreal에 서브퀘스트 판단용 최신 공장 스냅샷을 요청합니다.

    Args:
        context: 요청 세션과 클라이언트 식별자를 포함한 실행 문맥입니다.
        payload: 최적화 목표가 포함된 서브퀘스트 확인 요청입니다.

    Returns:
        Unreal이 후속 ``state_update``에 포함할 요청 ID와 상태 범위입니다.
    """
    request = subquest_request_store.create(
        context.session_id or "default-session",
        context.client_id or "unreal",
    )
    snapshot_request_id = request.snapshot_request_id
    goal = payload.get("goal") or "balance"
    response_payload = {
        "status": "need_more_state",
        "reason": "서브퀘스트 필요 여부를 판단하려면 최신 공장 상태가 필요합니다.",
        "snapshot_request_id": snapshot_request_id,
        "required_state_scopes": [
            "machines",
            "machine_condition",
            "storages",
            "conveyors",
            "power_grid",
            "resource_nodes",
        ],
        "next_request_hint": {
            "agent": "process_optimizer",
            "operation": "state_update",
            "request_source": "subquest_check",
            "snapshot_request_id": snapshot_request_id,
        },
        "goal": goal,
    }
    return {
        "responsePayload": response_payload,
        "responseMetadata": {"subquestCheck": "awaiting_snapshot"},
    }


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

    # Sprint 5: Snapshot Store에도 저장. 빈 상태 업데이트는 기존 스냅샷을 덮지 않는다.
    session_id = context.session_id or "default-session"
    client_id = context.client_id or "unreal"
    if factory_state:
        process_optimizer_snapshot_store.save(
            session_id=session_id,
            client_id=client_id,
            factory_state=factory_state,
            revision=revision,
            source="state_update"
        )


    snapshot_request_id = payload.get("snapshot_request_id")
    is_subquest_snapshot = (
        payload.get("request_source") == "subquest_check"
        and bool(snapshot_request_id)
    )
    subquest_mode = payload.get("subquest_mode") is True or is_subquest_snapshot

    goal = payload.get("goal") or "balance"

    analyzer = FactoryStateAnalyzerTool()
    report = analyzer.analyze(factory_state, factory_revision=revision, goal=goal)

    if is_subquest_snapshot and report.need_more_state:
        response_payload = dict(report.need_more_state)
        response_payload["snapshot_request_id"] = snapshot_request_id
        response_payload["factoryRevision"] = revision
        response_payload["goal"] = goal
        next_request_hint = dict(response_payload.get("next_request_hint") or {})
        next_request_hint["request_source"] = "subquest_check"
        next_request_hint["snapshot_request_id"] = snapshot_request_id
        response_payload["next_request_hint"] = next_request_hint
        return {
            "responsePayload": response_payload,
            "responseMetadata": {"memory": "updated"},
        }

    alert_builder = SubquestAlertBuilder()
    alert = alert_builder.build_alert(
        report, factory_state, subquest_mode=subquest_mode
    )

    highlight_targets: list[str] = []
    for target_id in (
        [node_id for node_id in report.isolated_power_nodes]
        + [gen_id for gen_id in report.disconnected_generators]
        + [machine_id for machine_id in report.unpowered_machines]
    ):
        if target_id not in highlight_targets:
            highlight_targets.append(target_id)

    if alert.target is not None and alert.target.id not in highlight_targets:
        highlight_targets.append(alert.target.id)

    response_payload = {
        "status": "success",
        "factoryRevision": revision,
        "goal": goal,
        "optimization_alert": alert.model_dump(),
        "ui_hints": {"highlight_targets": highlight_targets},
    }
    if snapshot_request_id:
        response_payload["snapshot_request_id"] = snapshot_request_id
    if is_subquest_snapshot and snapshot_request_id:
        subquest_request_store.consume(snapshot_request_id)
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
        # Sprint 5: snapshot_store에서 최신 스냅샷을 우선적으로 시도
        session_id = context.session_id or "default-session"
        client_id = context.client_id or "unreal"
        snapshot = process_optimizer_snapshot_store.get_latest(session_id, client_id)
        if snapshot and snapshot.factory_state:
            graph_payload["factory_state"] = snapshot.factory_state
            if graph_payload.get("factoryRevision") is None:
                graph_payload["factoryRevision"] = snapshot.factoryRevision
        else:
            # fallback to legacy session_memory
            remembered_state = process_optimizer_memory.get_state(context.session_id)
            if remembered_state:
                graph_payload["factory_state"] = remembered_state

    if graph_payload.get("factoryRevision") is None:
        remembered_revision = process_optimizer_memory.get_revision(context.session_id)
        if remembered_revision:
            graph_payload["factoryRevision"] = remembered_revision

    return graph_payload
