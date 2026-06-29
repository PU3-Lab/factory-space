"""Process Optimizer 세션별 최신 공장 상태를 메모리에 보관합니다.

주기적인 상태 업데이트로 공장 snapshot과 revision을 저장하고, 분석 요청이
상태를 직접 포함하지 않았을 때 가장 최근 데이터를 대신 사용하게 합니다.
"""

from __future__ import annotations

from typing import Any


class ProcessOptimizerSessionMemory:
    """세션별 최신 공장 상태와 revision을 관리하는 메모리 저장소입니다.

    Unreal의 주기 상태 업데이트를 세션 단위로 보관하여, 이후 최적화 요청이
    공장 상태를 생략해도 같은 세션의 최신 snapshot을 참조할 수 있게 합니다.
    """

    def __init__(self) -> None:
        """세션 ID를 키로 사용하는 상태와 revision 저장소를 초기화합니다.

        공장 snapshot과 revision은 서로 분리된 메모리 딕셔너리에 보관됩니다.
        """

        self._states: dict[str, dict[str, Any]] = {}
        self._revisions: dict[str, int] = {}

    def get_state(self, session_id: str | None) -> dict[str, Any]:
        """세션에 저장된 가장 최근 공장 상태를 조회합니다.

        Args:
            session_id: 조회할 플레이어 세션 식별자입니다.

        Returns:
            저장된 공장 상태이며, 세션이나 상태가 없으면 빈 딕셔너리입니다.
        """
        if not session_id:
            return {}
        return self._states.get(session_id, {})

    def get_revision(self, session_id: str | None) -> int:
        """세션에 저장된 가장 최근 공장 revision을 조회합니다.

        Args:
            session_id: 조회할 플레이어 세션 식별자입니다.

        Returns:
            저장된 revision이며, 세션 정보가 없으면 ``0``입니다.
        """
        if not session_id:
            return 0
        return self._revisions.get(session_id, 0)

    def update(
        self,
        session_id: str | None,
        factory_state: dict[str, Any],
        revision: int,
    ) -> None:
        """세션의 최신 공장 상태와 revision을 함께 갱신합니다.

        Args:
            session_id: 상태를 보관할 플레이어 세션 식별자입니다.
            factory_state: Unreal이 보낸 최신 공장 snapshot입니다.
            revision: snapshot에 대응하는 공장 revision입니다.
        """
        if not session_id:
            return
        self._states[session_id] = factory_state
        self._revisions[session_id] = revision

    def clear(self, session_id: str | None) -> None:
        """특정 세션에 저장된 공장 상태와 revision을 제거합니다.

        Args:
            session_id: 메모리에서 제거할 플레이어 세션 식별자입니다.
        """
        if not session_id:
            return
        self._states.pop(session_id, None)
        self._revisions.pop(session_id, None)


# Global process optimizer memory singleton
process_optimizer_memory = ProcessOptimizerSessionMemory()
