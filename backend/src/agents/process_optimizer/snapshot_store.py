"""주기적인 공장 상태 업데이트(state_update)에서 전달된 최신 공장 스냅샷을 세션별로 기억하는 인메모리 저장소입니다.

플레이어가 직접 분석 버튼을 누르지 않아도, 가장 최근에 수신한 공장 배치 상태를 기준으로
최적화 이상 징후를 판별하고 제안을 즉각적으로 구성할 수 있도록 지원합니다.
"""

from __future__ import annotations

from datetime import datetime, UTC
from typing import Any
from pydantic import BaseModel, Field


class PowerGridSnapshot(BaseModel):
    """주기적인 상태 업데이트 시점에 저장되는 공장 전력망 스냅샷 데이터 모델입니다.

    Unreal 클라이언트 정보, 수신 시각, 리비전 및 공장의 원본 스냅샷 딕셔너리를 포함합니다.
    """

    session_id: str = Field(description="플레이어의 고유 게임 세션 식별자")
    client_id: str = Field(default="unreal", description="요청을 보낸 클라이언트 식별자")
    factoryRevision: int = Field(default=0, description="공장 배치 설계의 리비전 번호")
    updated_at: datetime = Field(
        default_factory=lambda: datetime.now(UTC),
        description="스냅샷이 저장 및 갱신된 시각 (UTC 기준)"
    )
    source: str = Field(default="state_update", description="스냅샷 유입 경로 또는 출처")
    factory_state: dict[str, Any] = Field(
        default_factory=dict,
        description="설비 및 전력망 상태 정보가 포함된 공장 원본 스냅샷 데이터"
    )


class ProcessOptimizerSnapshotStore:
    """세션(session_id) 및 클라이언트(client_id) 단위로 최신 공장 스냅샷을 유지 관리하는 인메모리 저장소 클래스입니다.

    이 클래스는 주기적으로 수신한 공장의 물리적 기기 상태를 메모리에 임시 보관해두고,
    후속 요청이 상태 데이터를 생략하여 전송하더라도 가장 최근 데이터를 조회하여 분석을 수행할 수 있게 보완해 줍니다.
    """

    def __init__(self) -> None:
        """스냅샷 저장 공간을 딕셔너리로 초기화합니다."""
        # Key: (session_id, client_id), Value: PowerGridSnapshot
        self._store: dict[tuple[str, str], PowerGridSnapshot] = {}

    def save(
        self,
        session_id: str,
        client_id: str,
        factory_state: dict[str, Any],
        revision: int,
        source: str = "state_update"
    ) -> PowerGridSnapshot:
        """최신 공장 스냅샷을 생성하거나 기존 정보를 덮어씌워 갱신(Upsert)합니다.

        이미 더 높은 factoryRevision의 스냅샷이 저장되어 있으면, 늦게 도착한
        오래된 상태 업데이트가 최신 상태를 덮지 않도록 기존 스냅샷을 유지합니다.

        Args:
            session_id: 세션 식별자
            client_id: 클라이언트 식별자
            factory_state: 수신된 공장 상태 snapshot 딕셔너리
            revision: 공장 리비전 번호
            source: 데이터 유입 출처 (기본값: 'state_update')

        Returns:
            PowerGridSnapshot: 새로 저장 및 갱신 완료된 스냅샷 객체
        """
        key = (session_id, client_id)
        existing = self._store.get(key)
        if existing and revision < existing.factoryRevision:
            return existing

        snapshot = PowerGridSnapshot(
            session_id=session_id,
            client_id=client_id,
            factoryRevision=revision,
            updated_at=datetime.now(UTC),
            source=source,
            factory_state=factory_state
        )
        self._store[key] = snapshot
        return snapshot

    def get_latest(self, session_id: str, client_id: str = "unreal") -> PowerGridSnapshot | None:
        """세션 ID와 클라이언트 ID를 기준으로 가장 최근에 저장된 스냅샷을 조회합니다.

        지정된 클라이언트 ID로 일치하는 스냅샷이 없을 경우, 해당 세션 ID에 해당하는 스냅샷 중
        가장 최근 갱신 시각(updated_at)을 가진 데이터를 찾아 유연하게 반환(Fallback)합니다.

        Args:
            session_id: 조회할 세션 식별자
            client_id: 조회할 클라이언트 식별자 (기본값: 'unreal')

        Returns:
            PowerGridSnapshot | None: 저장된 스냅샷 객체 또는 스냅샷이 존재하지 않는 경우 None
        """
        # 1. client_id까지 완전히 일치하는 스냅샷 우선 검색
        snapshot = self._store.get((session_id, client_id))
        if snapshot:
            return snapshot

        # 2. 일치 항목이 없는 경우 해당 session_id를 공유하는 모든 스냅샷을 시간 역순 정렬하여 최신본 반환
        candidates = [s for (s_id, c_id), s in self._store.items() if s_id == session_id]
        if not candidates:
            return None

        candidates.sort(key=lambda s: s.updated_at, reverse=True)
        return candidates[0]

    def clear(self, session_id: str) -> None:
        """특정 세션 ID와 매핑된 모든 저장 스냅샷 데이터를 깨끗하게 삭제합니다.

        Args:
            session_id: 삭제할 세션 식별자
        """
        keys_to_remove = [k for k in self._store.keys() if k[0] == session_id]
        for k in keys_to_remove:
            self._store.pop(k, None)


# 전역에서 단일 데이터 엔트리로 사용하기 위한 싱글톤 객체
process_optimizer_snapshot_store = ProcessOptimizerSnapshotStore()
