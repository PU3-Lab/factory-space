"""Unit tests for Process Optimizer execution logging storage."""

import pytest
from agents.process_optimizer.execution_record import ExecutionRecord, ExecutionRecordStore

def test_execution_record_store_lifecycle():
    """ExecutionRecordStore의 기록 저장, 중복 감지 및 조회 수명 주기를 테스트합니다."""
    store = ExecutionRecordStore()
    
    plan_id = "test-plan-001"
    change_id = "change-001"
    
    record = ExecutionRecord(
        plan_id=plan_id,
        change_id=change_id,
        before={"state_known": False, "source": "unreal_runtime_required"},
        after={
            "state_known": False,
            "source": "planned_command",
            "planned_command": {"command": "set_recipe"},
        },
        revision=12
    )
    
    # 1. 초기 상태 확인
    assert store.has_record(plan_id, change_id) is False
    assert store.get_record(plan_id, change_id) is None
    
    # 2. 저장소 기록 보관
    store.save(record)
    assert store.has_record(plan_id, change_id) is True
    
    retrieved = store.get_record(plan_id, change_id)
    assert retrieved is not None
    assert retrieved.plan_id == plan_id
    assert retrieved.change_id == change_id
    assert retrieved.before["state_known"] is False
    assert retrieved.before["source"] == "unreal_runtime_required"
    assert retrieved.after["source"] == "planned_command"
    assert retrieved.after["planned_command"]["command"] == "set_recipe"
    assert retrieved.revision == 12
    
    # 3. 다른 키 검증
    assert store.has_record(plan_id, "different-change") is False
    assert store.has_record("different-plan", change_id) is False
