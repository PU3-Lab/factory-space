"""Execution record definitions and storage for process optimizer v2.

초보자 설명:
이 모듈은 플레이어가 승인한 최적화 명령을 Unreal에 전달하여 실행하기 직전에,
해당 변경 사항(계획 ID, 제안 ID, 변경 전후 속성 및 공장 버전)을 기록(ExecutionRecord)하는 저장소입니다.
이를 통해 차후 되돌리기(Undo) 처리 시 충돌을 검증하고, 동일한 최적화 변경이 여러 번 중복으로 실행되지 않도록 멱등성을 지키는 안전장치 역할을 합니다.
"""

from __future__ import annotations

from datetime import datetime
from typing import Any, Optional
from pydantic import BaseModel, Field


class ExecutionRecord(BaseModel):
    """최적화 계획 실행 변경 이력을 정의하는 Pydantic 모델입니다."""
    
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
    """메모리 기반으로 최적화 실행 기록(ExecutionRecord)을 관리하고 중복 방지 조회를 담당하는 저장소 클래스입니다."""

    def __init__(self) -> None:
        # 키: (plan_id, change_id), 값: ExecutionRecord
        self._records: dict[tuple[str, str], ExecutionRecord] = {}

    def save(self, record: ExecutionRecord) -> None:
        """실행 기록을 저장소에 저장합니다."""
        key = (record.plan_id, record.change_id)
        self._records[key] = record

    def get_record(self, plan_id: str, change_id: str) -> Optional[ExecutionRecord]:
        """계획 ID와 변경 ID를 기반으로 저장된 실행 기록을 조회합니다."""
        key = (plan_id, change_id)
        return self._records.get(key)

    def has_record(self, plan_id: str, change_id: str) -> bool:
        """해당 계획 및 변경 항목에 대한 실행 기록이 이미 존재하는지 여부를 판단합니다."""
        key = (plan_id, change_id)
        return key in self._records

    def get_records_by_plan(self, plan_id: str) -> list[ExecutionRecord]:
        """특정 계획 ID에 속한 모든 실행 기록 목록을 조회합니다."""
        return [r for (pid, cid), r in self._records.items() if pid == plan_id]

    def clear(self) -> None:
        """저장소의 모든 실행 기록 데이터를 삭제합니다 (테스트용)."""
        self._records.clear()


# 싱글톤 전역 인스턴스 선언
execution_record_store = ExecutionRecordStore()
