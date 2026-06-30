"""서브퀘스트 판단을 위해 발급한 스냅샷 요청을 잠시 기억합니다.

Unreal이 후속 ``state_update``에 돌려준 요청 ID가 실제로 백엔드에서
발급된 값인지 세션과 클라이언트 단위로 확인합니다.
"""

from __future__ import annotations

from dataclasses import dataclass
from datetime import UTC, datetime, timedelta
from uuid import uuid4


@dataclass(frozen=True)
class PendingSubquestRequest:
    """아직 완료되지 않은 서브퀘스트 스냅샷 요청입니다."""

    snapshot_request_id: str
    session_id: str
    client_id: str
    expires_at: datetime


class SubquestRequestStore:
    """발급한 스냅샷 요청 ID를 만료 시각까지 보관하는 인메모리 저장소입니다."""

    def __init__(self) -> None:
        """빈 요청 저장소를 준비합니다."""
        self._requests: dict[str, PendingSubquestRequest] = {}

    def create(
        self,
        session_id: str,
        client_id: str,
        *,
        ttl: timedelta = timedelta(minutes=5),
    ) -> PendingSubquestRequest:
        """세션에 연결된 새 스냅샷 요청을 발급합니다."""
        request = PendingSubquestRequest(
            snapshot_request_id=f"snapshot-{uuid4().hex[:12]}",
            session_id=session_id,
            client_id=client_id,
            expires_at=datetime.now(UTC) + ttl,
        )
        self._requests[request.snapshot_request_id] = request
        return request

    def matches(
        self,
        snapshot_request_id: str,
        session_id: str,
        client_id: str,
    ) -> bool:
        """요청 ID가 유효하고 같은 세션과 클라이언트에서 왔는지 확인합니다."""
        request = self._requests.get(snapshot_request_id)
        if request is None:
            return False
        if request.expires_at <= datetime.now(UTC):
            self._requests.pop(snapshot_request_id, None)
            return False
        return request.session_id == session_id and request.client_id == client_id

    def consume(self, snapshot_request_id: str) -> None:
        """판단이 끝난 요청 ID를 다시 사용할 수 없도록 제거합니다."""
        self._requests.pop(snapshot_request_id, None)


subquest_request_store = SubquestRequestStore()
