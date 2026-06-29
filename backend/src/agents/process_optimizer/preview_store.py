"""Process Optimizer의 미리보기 계획을 메모리에 저장합니다.

세션과 계획 식별자로 승인 대기 중인 계획을 다시 찾고, 적용 전에 필요한
만료 시간과 공장 revision 충돌 검사를 지원합니다.
"""

from __future__ import annotations

from datetime import UTC, datetime

from agents.process_optimizer.schemas import PreviewPlan


class PreviewPlanStore:
    """최적화 preview plan을 세션별로 임시 저장하는 메모리 저장소입니다.

    Sprint 3에서는 apply 단계로 넘어가기 전에 생성된 preview를 `session_id`와
    `plan_id` 조합으로 다시 찾을 수 있게 하는 것이 목표입니다. 실제 영속 DB는
    쓰지 않고, 프로세스 생명주기 동안만 유지되는 메모리 저장소로 시작합니다.
    """

    def __init__(self) -> None:
        """비어 있는 미리보기 계획 인덱스를 초기화합니다.

        계획은 세션 ID와 계획 ID 조합을 키로 사용하여 메모리에 보관됩니다.
        """

        self._store: dict[tuple[str, str], PreviewPlan] = {}

    def save(self, plan: PreviewPlan) -> None:
        """미리보기 계획을 세션과 계획 ID 조합으로 저장합니다.

        Args:
            plan: 분석 결과와 유효 기간을 담은 승인 대기 계획입니다.
        """

        self._store[(plan.session_id, plan.plan_id)] = plan

    def get(self, session_id: str, plan_id: str) -> PreviewPlan | None:
        """세션과 계획 식별자에 맞는 미리보기 계획을 조회합니다.

        Args:
            session_id: 플레이어 요청 세션 식별자입니다.
            plan_id: 분석 단계에서 발급한 계획 식별자입니다.

        Returns:
            저장된 계획이며, 찾지 못하면 ``None``입니다.
        """

        return self._store.get((session_id, plan_id))

    def is_expired(self, plan: PreviewPlan) -> bool:
        """미리보기 계획의 승인 가능 시간이 지났는지 확인합니다.

        Args:
            plan: 만료 여부를 검사할 미리보기 계획입니다.

        Returns:
            현재 시각이 만료 시각을 지났으면 ``True``를 반환합니다.
        """

        now = datetime.now(UTC) if plan.expires_at.tzinfo else datetime.now()
        return now > plan.expires_at

    def check_revision_conflict(self, plan: PreviewPlan, current_revision: int) -> bool:
        """계획 생성 시점과 현재 공장 revision이 다른지 확인합니다.

        Args:
            plan: 분석 시점의 revision을 가진 미리보기 계획입니다.
            current_revision: Unreal이 적용 요청과 함께 보낸 최신 revision입니다.

        Returns:
            두 revision이 달라 기존 계획을 적용할 수 없으면 ``True``입니다.
        """

        return plan.factoryRevision != current_revision

    def clear(self) -> None:
        """테스트 격리를 위해 저장된 미리보기 계획을 모두 제거합니다.

        Unreal 공장에는 영향을 주지 않고 승인 대기 중인 메모리 데이터만 비웁니다.
        """

        self._store.clear()


preview_plan_store = PreviewPlanStore()
