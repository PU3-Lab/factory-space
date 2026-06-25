"""In-memory preview plan store for Process Optimizer v2."""

from __future__ import annotations

from datetime import datetime, timezone

from agents.process_optimizer.schemas import PreviewPlan


class PreviewPlanStore:
    """최적화 preview plan을 세션별로 임시 저장하는 메모리 저장소입니다.

    Sprint 3에서는 apply 단계로 넘어가기 전에 생성된 preview를 `session_id`와
    `plan_id` 조합으로 다시 찾을 수 있게 하는 것이 목표입니다. 실제 영속 DB는
    쓰지 않고, 프로세스 생명주기 동안만 유지되는 메모리 저장소로 시작합니다.
    """

    def __init__(self) -> None:
        self._store: dict[tuple[str, str], PreviewPlan] = {}

    def save(self, plan: PreviewPlan) -> None:
        """preview plan을 `(session_id, plan_id)` 키로 저장합니다."""

        self._store[(plan.session_id, plan.plan_id)] = plan

    def get(self, session_id: str, plan_id: str) -> PreviewPlan | None:
        """세션 ID와 plan ID로 저장된 preview plan을 조회합니다."""

        return self._store.get((session_id, plan_id))

    def is_expired(self, plan: PreviewPlan) -> bool:
        """preview plan의 만료 시간이 현재 시각보다 과거인지 확인합니다."""

        now = datetime.now(timezone.utc) if plan.expires_at.tzinfo else datetime.now()
        return now > plan.expires_at

    def check_revision_conflict(self, plan: PreviewPlan, current_revision: int) -> bool:
        """저장 당시 factoryRevision과 현재 revision이 다른지 확인합니다."""

        return plan.factoryRevision != current_revision

    def clear(self) -> None:
        """테스트 격리를 위해 저장된 preview plan을 모두 제거합니다."""

        self._store.clear()


preview_plan_store = PreviewPlanStore()
