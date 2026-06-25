"""Unit tests for Process Optimizer PreviewPlanStore."""

from datetime import datetime, timedelta, timezone

from agents.process_optimizer.preview_store import PreviewPlanStore
from agents.process_optimizer.schemas import PreviewPlan, UiHints


def _make_preview_plan(
    *,
    plan_id: str = "test-plan-123",
    session_id: str = "test-session-456",
    revision: int = 10,
    expires_delta: timedelta = timedelta(minutes=5),
) -> PreviewPlan:
    now = datetime.now(timezone.utc)
    return PreviewPlan(
        plan_id=plan_id,
        session_id=session_id,
        factoryRevision=revision,
        goal="balance",
        changes=[],
        expected_effect={"estimated": False},
        ui_hints=UiHints(),
        created_at=now,
        expires_at=now + expires_delta,
    )


def test_preview_plan_store_saves_and_reads_by_session_and_plan_id():
    """PreviewPlanStore는 session_id와 plan_id 조합으로 preview 계획을 저장합니다."""

    store = PreviewPlanStore()
    plan = _make_preview_plan()

    store.save(plan)

    retrieved = store.get(plan.session_id, plan.plan_id)
    assert retrieved is not None
    assert retrieved.plan_id == plan.plan_id
    assert retrieved.session_id == plan.session_id
    assert retrieved.factoryRevision == plan.factoryRevision


def test_preview_plan_store_returns_none_for_unknown_keys():
    """존재하지 않는 session_id나 plan_id 조회는 None을 반환합니다."""

    store = PreviewPlanStore()
    plan = _make_preview_plan()
    store.save(plan)

    assert store.get(plan.session_id, "unknown-plan") is None
    assert store.get("unknown-session", plan.plan_id) is None


def test_preview_plan_store_detects_expiration():
    """expires_at이 현재 시각보다 과거이면 만료된 계획으로 판단합니다."""

    store = PreviewPlanStore()
    active_plan = _make_preview_plan(expires_delta=timedelta(minutes=5))
    expired_plan = _make_preview_plan(
        plan_id="expired-plan",
        expires_delta=timedelta(minutes=-5),
    )

    assert store.is_expired(active_plan) is False
    assert store.is_expired(expired_plan) is True


def test_preview_plan_store_detects_revision_conflict():
    """현재 factoryRevision이 preview 생성 시점과 다르면 충돌로 판단합니다."""

    store = PreviewPlanStore()
    plan = _make_preview_plan(revision=10)

    assert store.check_revision_conflict(plan, 10) is False
    assert store.check_revision_conflict(plan, 11) is True


def test_preview_plan_store_clear_removes_all_plans():
    """테스트 격리를 위해 clear가 모든 preview 계획을 제거하는지 검증합니다."""

    store = PreviewPlanStore()
    plan = _make_preview_plan()
    store.save(plan)

    store.clear()

    assert store.get(plan.session_id, plan.plan_id) is None
