"""Effect measurement logic for process optimizer v2.

초보자 설명:
이 모듈은 최적화 계획이 실제로 적용된 이후, 일정 시간과 생산 주기가 경과했는지 관찰하고,
적용 전과 후의 공장 분석 지표(Metrics)를 비교하여 실제 개선 효과가 있었는지 검증합니다.
효과가 좋으면 success, 예상보다 낮으면 failed, 오히려 나빠졌다면 degraded(악화)로 평가합니다.
"""

import json
from datetime import datetime, timezone
from typing import Any, Optional, Literal
from pydantic import BaseModel
from agents.process_optimizer.execution_record import ExecutionRecord
from agents.process_optimizer.schemas import FactoryState
from agents.process_optimizer.analyzer import FactoryStateAnalyzerTool

def check_observation_window(records: list[ExecutionRecord], current_time: Optional[datetime] = None) -> bool:
    """적용 후 최소 30초가 경과했는지 확인합니다.

    설명:
    실행 기록(ExecutionRecord) 중 가장 먼저 생성된 시점과 현재 검증 시점의 차이를 구하여
    30초 미만으로 지났다면 관찰 시간이 부족하므로 False를 반환합니다.
    """
    if not records:
        return False
        
    if current_time is None:
        current_time = datetime.now(timezone.utc)
        
    # datetime에 timezone이 없을 경우 utc로 강제 설정하여 비교 오류 방지
    if current_time.tzinfo is None:
        current_time = current_time.replace(tzinfo=timezone.utc)
        
    for rec in records:
        rec_time = rec.created_at
        if rec_time.tzinfo is None:
            rec_time = rec_time.replace(tzinfo=timezone.utc)
            
        diff = (current_time - rec_time).total_seconds()
        if diff < 30.0:
            return False
            
    return True

def check_production_cycles(payload_cycles: int) -> bool:
    """최소 3 생산 주기(production cycle)가 경과했는지 검증합니다."""
    return payload_cycles >= 3

def recreate_before_state(current_state: Any, records: list[ExecutionRecord]) -> dict[str, Any]:
    """현재 공장 상태에 실행 기록의 before 정보를 덮어씌워, 적용 전의 가상 공장 상태를 복원합니다."""
    if isinstance(current_state, FactoryState):
        state_dict = current_state.model_dump()
    elif isinstance(current_state, dict):
        state_dict = json.loads(json.dumps(current_state))
    else:
        try:
            state_dict = FactoryState.model_validate(current_state).model_dump()
        except Exception:
            return {}

    for rec in records:
        target_id = None
        target_type = None

        if isinstance(rec.after, dict) and "planned_command" in rec.after:
            cmd = rec.after["planned_command"]
            target_id = cmd.get("machine_id") or cmd.get("conveyor_id")
            target_type = "machine" if "machine_id" in cmd else "conveyor"
        elif isinstance(rec.before, dict) and "target" in rec.before and rec.before["target"]:
            target_id = rec.before["target"].get("id")
            target_type = rec.before["target"].get("type")

        if not target_id or not target_type:
            continue

        before_state = rec.before
        if not isinstance(before_state, dict):
            continue

        if target_type == "machine":
            for m in state_dict.get("machines", []):
                if m.get("id") == target_id:
                    # before_state에 포함된 모든 유효 필드들을 머지
                    for k, v in before_state.items():
                        if k not in ["state_known", "source", "reason", "target"]:
                            m[k] = v

                    if "enabled" in before_state:
                        enabled = before_state["enabled"]
                        m["status"] = "idle" if enabled else "disabled"
                        if not enabled:
                            m["operating_rate"] = 0.0

        elif target_type == "conveyor":
            for c in state_dict.get("conveyors", []):
                if c.get("id") == target_id:
                    for k, v in before_state.items():
                        if k not in ["state_known", "source", "reason", "target"]:
                            c[k] = v

    return state_dict

def evaluate_effects(
    expected_effect: dict[str, Any],
    before_metrics: Any,
    after_metrics: Any
) -> dict[str, Any]:
    """예상 효과 지표와 실제 효과 지표를 비교하여 성공, 미달(failed), 악화(degraded) 상태를 판별합니다.

    설명:
    - degraded (악화): 평균 가동률이 떨어졌거나, 병목(input shortages 등) 건수가 늘어난 경우.
    - success (성공): 예상치만큼 병목 지표들이 실제로 모두 해소된 경우.
    - failed (미달): 이전보단 나아졌으나 목표 예상치에는 못 미치는 상태.
    """
    # 1. 예상치 수집
    expected_input_resolved = expected_effect.get("resolved_input_shortages_count", 0)
    expected_output_resolved = expected_effect.get("resolved_output_blocks_count", 0)
    expected_conveyor_resolved = expected_effect.get("resolved_conveyor_congestions_count", 0)
    
    # 2. 실제 성과 지표 계산
    actual_input_resolved = max(0, len(before_metrics.input_shortages) - len(after_metrics.input_shortages))
    actual_output_resolved = max(0, len(before_metrics.output_blocked) - len(after_metrics.output_blocked))
    actual_conveyor_resolved = max(0, len(before_metrics.congested_conveyors) - len(after_metrics.congested_conveyors))
    
    before_op_rate = before_metrics.average_operating_rate
    after_op_rate = after_metrics.average_operating_rate
    
    actual_effect = {
        "resolved_input_shortages_count": actual_input_resolved,
        "resolved_output_blocks_count": actual_output_resolved,
        "resolved_conveyor_congestions_count": actual_conveyor_resolved,
        "average_operating_rate_before": before_op_rate,
        "average_operating_rate_after": after_op_rate
    }
    
    # 3. 상태 분류 및 추천 차기 액션 판별
    # 가동률이 하락했거나 병목이 늘어났다면 악화(degraded)
    is_degraded = (
        after_op_rate < before_op_rate - 1e-4 or
        len(after_metrics.input_shortages) > len(before_metrics.input_shortages) or
        len(after_metrics.output_blocked) > len(before_metrics.output_blocked) or
        len(after_metrics.congested_conveyors) > len(before_metrics.congested_conveyors)
    )
    
    if is_degraded:
        return {
            "status": "degraded",
            "next_action": "reanalyze",
            "actual_effect": actual_effect
        }
        
    # 예상 해결 건수를 모두 달성했는지 체크
    met_expected = (
        actual_input_resolved >= expected_input_resolved and
        actual_output_resolved >= expected_output_resolved and
        actual_conveyor_resolved >= expected_conveyor_resolved
    )
    
    if met_expected:
        return {
            "status": "success",
            "next_action": "monitor",
            "actual_effect": actual_effect
        }
    else:
        return {
            "status": "failed",
            "next_action": "reanalyze",
            "actual_effect": actual_effect
        }
