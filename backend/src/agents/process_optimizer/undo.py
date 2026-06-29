"""Process Optimizer 변경을 안전하게 되돌리기 위한 도우미입니다.

실행 기록의 적용 후 상태와 현재 공장 상태를 비교하여 충돌을 찾고,
기록된 적용 전 상태가 신뢰할 수 있을 때만 역방향 명령을 생성합니다.
"""

from typing import Any

from agents.process_optimizer.execution_record import ExecutionRecord
from agents.process_optimizer.schemas import FactoryState


def get_component_state(
    factory_state: Any, target_type: str, target_id: str
) -> dict[str, Any] | None:
    """공장 snapshot에서 지정한 장비 또는 컨베이어의 현재 상태를 찾습니다.

    Args:
        factory_state: Pydantic 모델 또는 딕셔너리 형태의 공장 snapshot입니다.
        target_type: 조회할 대상 종류로 ``machine`` 또는 ``conveyor``를 사용합니다.
        target_id: Unreal에서 사용하는 대상 식별자입니다.

    Returns:
        정규화된 대상 상태이며, 입력이나 대상을 확인할 수 없으면 ``None``입니다.
    """
    if isinstance(factory_state, FactoryState):
        state_dict = factory_state.model_dump()
    elif isinstance(factory_state, dict):
        state_dict = factory_state
    else:
        try:
            state_dict = FactoryState.model_validate(factory_state).model_dump()
        except Exception:
            return None

    if target_type == "machine":
        for m in state_dict.get("machines", []):
            if m.get("id") == target_id:
                status = m.get("status", "idle")
                enabled = status != "disabled"
                return {
                    "id": target_id,
                    "type": m.get("type", "unknown"),
                    "status": status,
                    "enabled": m.get("enabled", enabled),
                    "recipe_id": m.get("recipe_id"),
                    **{
                        k: v
                        for k, v in m.items()
                        if k not in ["id", "type", "status", "enabled", "recipe_id"]
                    },
                }
    elif target_type == "conveyor":
        for c in state_dict.get("conveyors", []):
            if c.get("id") == target_id:
                return {
                    "id": target_id,
                    "congestion_rate": c.get("congestion_rate", 0.0),
                    **{
                        k: v for k, v in c.items() if k not in ["id", "congestion_rate"]
                    },
                }
    return None


def check_undo_conflict(record: ExecutionRecord, factory_state: Any) -> bool:
    """기록된 적용 후 상태와 현재 공장 상태가 충돌하는지 확인합니다.

    Args:
        record: 적용 전후 상태를 담은 실행 기록입니다.
        factory_state: Unreal이 되돌리기 요청과 함께 보낸 최신 공장 상태입니다.

    Returns:
        플레이어 수정이나 대상 누락으로 안전하게 되돌릴 수 없으면 ``True``입니다.
    """
    target_type = None
    target_id = None

    # Resolve target metadata from planned commands or recorded target hints.
    if isinstance(record.after, dict) and "planned_command" in record.after:
        cmd = record.after["planned_command"]
        target_id = cmd.get("machine_id") or cmd.get("conveyor_id")
        if "machine_id" in cmd:
            target_type = "machine"
        elif "conveyor_id" in cmd:
            target_type = "conveyor"
    elif (
        isinstance(record.before, dict)
        and "target" in record.before
        and record.before["target"]
    ):
        target_type = record.before["target"].get("type")
        target_id = record.before["target"].get("id")

    if not target_type or not target_id:
        return True

    current = get_component_state(factory_state, target_type, target_id)
    if not current:
        # Missing current target state is treated as an undo conflict.
        return True

    after_state = record.after

    # Compare authoritative after-state fields when Unreal provided them.
    if isinstance(after_state, dict) and after_state.get("state_known") is True:
        ignored_fields = {
            "state_known",
            "source",
            "requires_unreal_confirmation",
            "planned_command",
            "target",
        }
        for key, expected_value in after_state.items():
            if key in ignored_fields:
                continue
            if current.get(key) != expected_value:
                return True
        return False

    # Resolve target metadata from planned commands or recorded target hints.
    if isinstance(after_state, dict) and "planned_command" in after_state:
        cmd = after_state["planned_command"]
        cmd_type = cmd.get("command")

        if cmd_type == "set_recipe":
            expected_recipe = cmd.get("recipe_id")
            actual_recipe = current.get("recipe_id")
            return actual_recipe != expected_recipe

        elif cmd_type == "set_machine_enabled":
            expected_enabled = cmd.get("enabled")
            actual_enabled = current.get("enabled")
            return actual_enabled != expected_enabled

        elif cmd_type == "connect_conveyor":
            expected_before = cmd.get("before_target")
            expected_after = cmd.get("after_target")
            actual_before = current.get("before_target")
            actual_after = current.get("after_target")
            if expected_before and actual_before and actual_before != expected_before:
                return True
            if expected_after and actual_after and actual_after != expected_after:
                return True
            return False

        elif cmd_type == "disconnect_conveyor":
            return False

        elif cmd_type == "move_machine":
            expected_pos = cmd.get("position")
            actual_pos = current.get("position")
            if expected_pos and actual_pos:
                return any(abs(e - a) > 1e-4 for e, a in zip(expected_pos, actual_pos))
            return False

        elif cmd_type == "place_machine":
            return False

        elif cmd_type == "remove_machine":
            return True

    # Compare direct after-state fields when available.
    elif isinstance(after_state, dict):
        for k, v in after_state.items():
            if k in ["state_known", "source", "requires_unreal_confirmation", "target"]:
                continue
            if current.get(k) != v:
                return True

    return False


def build_inverse_command(record: ExecutionRecord) -> dict[str, Any] | None:
    """실행 기록의 적용 전 상태를 복원하는 역방향 명령을 만듭니다.

    Args:
        record: 원래 명령과 신뢰 가능한 적용 전 상태를 담은 실행 기록입니다.

    Returns:
        Unreal에 전달할 역방향 명령이며, 상태를 알 수 없으면 ``None``입니다.
    """
    before_state = record.before
    if isinstance(before_state, dict) and before_state.get("state_known") is False:
        return None

    # Reuse an already stored inverse command when present.
    if isinstance(before_state, dict) and "command" in before_state:
        return before_state

    # Resolve target metadata from planned commands or recorded target hints.
    after_state = record.after
    if not isinstance(after_state, dict) or "planned_command" not in after_state:
        return None

    cmd = after_state["planned_command"]
    cmd_type = cmd.get("command")
    target_id = cmd.get("machine_id") or cmd.get("conveyor_id") or "unknown"

    if cmd_type == "set_recipe":
        orig_recipe = "copper_ingot"
        if isinstance(before_state, dict):
            orig_recipe = (
                before_state.get("recipe_id")
                or before_state.get("original_recipe_id")
                or orig_recipe
            )
        return {
            "command": "set_recipe",
            "machine_id": target_id,
            "recipe_id": orig_recipe,
        }

    elif cmd_type == "set_machine_enabled":
        orig_enabled = True
        if isinstance(before_state, dict):
            if "enabled" in before_state:
                orig_enabled = before_state["enabled"]
            elif "original_enabled" in before_state:
                orig_enabled = before_state["original_enabled"]
        return {
            "command": "set_machine_enabled",
            "machine_id": target_id,
            "enabled": orig_enabled,
        }

    elif cmd_type == "connect_conveyor":
        orig_before = None
        orig_after = None
        if isinstance(before_state, dict):
            orig_before = before_state.get("before_target")
            orig_after = before_state.get("after_target")

        if orig_before or orig_after:
            return {
                "command": "connect_conveyor",
                "conveyor_id": target_id,
                "before_target": orig_before or "storage_01",
                "after_target": orig_after or "smelter_01",
            }
        else:
            return {"command": "disconnect_conveyor", "conveyor_id": target_id}

    elif cmd_type == "disconnect_conveyor":
        orig_before = "storage_01"
        orig_after = "smelter_01"
        if isinstance(before_state, dict):
            orig_before = before_state.get("before_target") or orig_before
            orig_after = before_state.get("after_target") or orig_after
        return {
            "command": "connect_conveyor",
            "conveyor_id": target_id,
            "before_target": orig_before,
            "after_target": orig_after,
        }

    elif cmd_type == "move_machine":
        orig_pos = [0.0, 0.0, 0.0]
        if isinstance(before_state, dict) and "position" in before_state:
            orig_pos = before_state["position"]
        return {
            "command": "move_machine",
            "machine_id": target_id,
            "position": orig_pos,
        }

    elif cmd_type == "place_machine":
        return {"command": "remove_machine", "machine_id": target_id}

    elif cmd_type == "remove_machine":
        mach_type = "smelter"
        orig_pos = [0.0, 0.0, 0.0]
        if isinstance(before_state, dict):
            mach_type = before_state.get("machine_type") or mach_type
            orig_pos = before_state.get("position") or orig_pos
        return {
            "command": "place_machine",
            "machine_type": mach_type,
            "position": orig_pos,
        }

    return None
