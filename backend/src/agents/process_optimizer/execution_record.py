"""Process Optimizer의 변경 실행 기록을 정의하고 저장합니다.

적용된 변경의 전후 상태와 공장 revision을 보관하여 중복 실행을 방지하고,
이후 되돌리기 요청에서 현재 상태와 충돌하는지 판단할 근거를 제공합니다.
"""

from __future__ import annotations

from datetime import datetime
from typing import Any

from pydantic import BaseModel, Field


class ExecutionRecord(BaseModel):
    """최적화 계획에서 실행된 변경 한 건을 표현하는 모델입니다.

    계획과 변경 식별자, 적용 전후 상태, 적용 시점의 공장 revision을 묶어
    중복 실행 검사와 안전한 되돌리기에 사용합니다.
    """

    plan_id: str
    """최적화 계획 고유 식별자"""

    change_id: str
    """세부 변경 항목 고유 식별자"""

    before: Any
    """변경 적용 전의 기계/컨베이어 정보 속성 또는 레시피 설정 상태"""

    after: Any
    """변경 적용 후의 기계/컨베이어 정보 속성 또는 레시피 설정 상태"""

    revision: int
    """변경이 적용될 시점의 공장 고유 편집 버전 번호"""

    created_at: datetime = Field(default_factory=datetime.now)
    """기록 생성(최적화 적용) 시점 시각"""


class ExecutionRecordStore:
    """실행 기록을 계획과 변경 항목별로 관리하는 메모리 저장소입니다.

    ``(plan_id, change_id)``를 키로 사용하여 이미 처리된 변경을 찾고,
    되돌리기와 성과 측정에 필요한 기록을 계획 단위로 제공합니다.
    """

    def __init__(self) -> None:
        """비어 있는 실행 기록 인덱스를 초기화합니다.

        기록은 계획 ID와 변경 ID 조합을 키로 사용하는 메모리 딕셔너리에 보관됩니다.
        """

        # 키: (plan_id, change_id), 값: ExecutionRecord
        self._records: dict[tuple[str, str], ExecutionRecord] = {}

    def save(self, record: ExecutionRecord) -> None:
        """실행 기록을 계획 ID와 변경 ID 조합으로 저장합니다.

        Args:
            record: 적용 전후 상태와 revision을 담은 실행 기록입니다.
        """
        key = (record.plan_id, record.change_id)
        self._records[key] = record

    def get_record(self, plan_id: str, change_id: str) -> ExecutionRecord | None:
        """계획과 변경 식별자에 맞는 실행 기록 한 건을 조회합니다.

        Args:
            plan_id: 최적화 계획 식별자입니다.
            change_id: 계획 안의 변경 항목 식별자입니다.

        Returns:
            저장된 실행 기록이며, 찾지 못하면 ``None``입니다.
        """
        key = (plan_id, change_id)
        return self._records.get(key)

    def has_record(self, plan_id: str, change_id: str) -> bool:
        """같은 계획과 변경 항목이 이미 기록되었는지 확인합니다.

        Args:
            plan_id: 최적화 계획 식별자입니다.
            change_id: 계획 안의 변경 항목 식별자입니다.

        Returns:
            실행 기록이 이미 존재하면 ``True``를 반환합니다.
        """
        key = (plan_id, change_id)
        return key in self._records

    def get_records_by_plan(self, plan_id: str) -> list[ExecutionRecord]:
        """특정 최적화 계획에 속한 실행 기록을 모두 조회합니다.

        Args:
            plan_id: 조회할 최적화 계획 식별자입니다.

        Returns:
            해당 계획에 저장된 실행 기록 목록입니다.
        """
        return [r for (pid, cid), r in self._records.items() if pid == plan_id]

    def clear(self) -> None:
        """테스트 격리를 위해 저장된 모든 실행 기록을 제거합니다.

        실행 중인 공장 상태에는 영향을 주지 않고 메모리 저장소만 비웁니다.
        """
        self._records.clear()


# 싱글톤 전역 인스턴스 선언
execution_record_store = ExecutionRecordStore()
