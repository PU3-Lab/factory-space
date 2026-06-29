"""Process Optimizer가 Unreal에 전달할 명령을 정의하고 검증합니다.

검증된 최적화 제안을 Unreal 명령 스키마로 변환하며, 허용 목록에 없거나
필수 값이 부족한 명령이 실행 단계로 넘어가지 않도록 차단합니다.
"""

from __future__ import annotations

from typing import Any, Literal, Union

from pydantic import BaseModel, ValidationError

from agents.process_optimizer.schemas import OptimizationSuggestion

# ==========================================
# 1. Unreal 실행 허용 명령 Pydantic 스키마 정의
# ==========================================


class BaseCommand(BaseModel):
    """모든 Unreal 명령이 공통으로 사용하는 기본 스키마입니다.

    Attributes:
        command: Unreal에서 실행할 구체적인 작업을 구분하는 명령 이름입니다.
    """

    command: str


class SetRecipeCommand(BaseCommand):
    """특정 장비의 생산 레시피를 변경하는 명령입니다.

    Attributes:
        machine_id: 레시피를 변경할 장비의 식별자입니다.
        recipe_id: 장비에 새로 지정할 레시피 식별자입니다.
    """

    command: Literal["set_recipe"] = "set_recipe"
    machine_id: str
    recipe_id: str


class SetMachineEnabledCommand(BaseCommand):
    """특정 장비의 활성화 상태를 변경하는 명령입니다.

    Attributes:
        machine_id: 활성화 상태를 변경할 장비의 식별자입니다.
        enabled: 장비에 적용할 활성화 여부입니다.
    """

    command: Literal["set_machine_enabled"] = "set_machine_enabled"
    machine_id: str
    enabled: bool


class ConnectConveyorCommand(BaseCommand):
    """컨베이어의 연결 대상을 변경하는 명령입니다.

    Attributes:
        conveyor_id: 다시 연결할 컨베이어의 식별자입니다.
        before_target: 변경 전에 연결되어 있던 대상입니다.
        after_target: 변경 후 컨베이어가 연결될 대상입니다.
    """

    command: Literal["connect_conveyor"] = "connect_conveyor"
    conveyor_id: str
    before_target: str
    after_target: str


class DisconnectConveyorCommand(BaseCommand):
    """특정 컨베이어의 연결을 해제하는 명령입니다.

    Attributes:
        conveyor_id: 연결을 해제할 컨베이어의 식별자입니다.
    """

    command: Literal["disconnect_conveyor"] = "disconnect_conveyor"
    conveyor_id: str


class MoveMachineCommand(BaseCommand):
    """이미 배치된 장비를 다른 위치로 이동하는 명령입니다.

    Attributes:
        machine_id: 이동할 장비의 식별자입니다.
        position: ``[x, y, z]`` 순서로 표현한 목적지 좌표입니다.
    """

    command: Literal["move_machine"] = "move_machine"
    machine_id: str
    position: list[float]  # [x, y, z]


class PlaceMachineCommand(BaseCommand):
    """새 장비를 지정된 위치에 배치하는 명령입니다.

    Attributes:
        machine_type: 새로 생성할 장비 종류입니다.
        position: ``[x, y, z]`` 순서로 표현한 배치 좌표입니다.
    """

    command: Literal["place_machine"] = "place_machine"
    machine_type: str
    position: list[float]  # [x, y, z]


class RemoveMachineCommand(BaseCommand):
    """이미 배치된 장비를 제거하는 명령입니다.

    Attributes:
        machine_id: 제거할 장비의 식별자입니다.
    """

    command: Literal["remove_machine"] = "remove_machine"
    machine_id: str


# 합집합 타입 정의
UnrealCommandPayload = Union[
    SetRecipeCommand,
    SetMachineEnabledCommand,
    ConnectConveyorCommand,
    DisconnectConveyorCommand,
    MoveMachineCommand,
    PlaceMachineCommand,
    RemoveMachineCommand,
]

# 허용되는 명령의 화이트리스트 집합
ALLOWED_COMMANDS = {
    "set_recipe",
    "set_machine_enabled",
    "connect_conveyor",
    "disconnect_conveyor",
    "move_machine",
    "place_machine",
    "remove_machine",
}

# ==========================================
# 2. 결정론적 명령 변환 및 검증 함수
# ==========================================


def build_command_payload(suggestion: OptimizationSuggestion) -> dict[str, Any]:
    """최적화 제안 내용을 분석하여 그에 매칭되는 화이트리스트 명령 딕셔너리를 결정론적으로 조립합니다.

    설명:
    제안의 ID 및 추천 수단(recommended_action)을 텍스트 기반으로 대조 분석하여, 레시피 설정이나 벨트 연결 등으로 매핑합니다.
    """
    s_id = suggestion.id
    target_id = suggestion.target.id if suggestion.target else ""
    target_type = suggestion.target.type if suggestion.target else ""
    action_text = suggestion.recommended_action.lower()

    # 1. 레시피 설정 관련
    if "recipe" in action_text or "레시피" in action_text:
        return {
            "command": "set_recipe",
            "machine_id": target_id or "unknown_machine",
            "recipe_id": "iron_ingot",  # 기본 예시 레시피 ID
        }

    # 2. 기계 전원 활성화/비활성화 관련
    if (
        "power" in action_text
        or "전원" in action_text
        or "가동" in action_text
        or "차단" in action_text
    ):
        enabled = "차단" not in action_text and "off" not in action_text
        return {
            "command": "set_machine_enabled",
            "machine_id": target_id or "unknown_machine",
            "enabled": enabled,
        }

    # 3. 컨베이어 연결 해제 관련
    if "disconnect" in action_text or "해제" in action_text:
        return {
            "command": "disconnect_conveyor",
            "conveyor_id": target_id or "unknown_conveyor",
        }

    # 4. 컨베이어 벨트 연결 관련 (가장 빈도가 높은 연결 권장안)
    if (
        "connect" in action_text
        or "연결" in action_text
        or target_type == "conveyor"
        or "벨트" in action_text
    ):
        return {
            "command": "connect_conveyor",
            "conveyor_id": target_id or "unknown_conveyor",
            "before_target": "storage_01",
            "after_target": target_id or "machine_01",
        }

    # 5. 장비 이동 관련
    if "move" in action_text or "이동" in action_text:
        return {
            "command": "move_machine",
            "machine_id": target_id or "unknown_machine",
            "position": [0.0, 0.0, 0.0],
        }

    # 6. 장비 배치/설치 관련
    if "place" in action_text or "건설" in action_text or "설치" in action_text:
        return {
            "command": "place_machine",
            "machine_type": "smelter",
            "position": [0.0, 0.0, 0.0],
        }

    # 7. 장비 철거 관련
    if "remove" in action_text or "철거" in action_text or "삭제" in action_text:
        return {
            "command": "remove_machine",
            "machine_id": target_id or "unknown_machine",
        }

    # 매칭되는 키워드가 전혀 없을 경우 기본 복구 행동으로 매핑 (컨베이어 연결)
    return {
        "command": "connect_conveyor",
        "conveyor_id": target_id or "conv_fallback",
        "before_target": "storage_01",
        "after_target": "smelter_01",
    }


def validate_command_payload(payload: dict[str, Any]) -> bool:
    """Unreal 명령이 허용 목록과 해당 Pydantic 스키마에 맞는지 검사합니다.

    Args:
        payload: 명령 이름과 실행에 필요한 대상 정보를 담은 딕셔너리입니다.

    Returns:
        허용된 명령이며 필수 필드 검증까지 통과하면 ``True``를 반환합니다.
    """
    command = payload.get("command")

    # 1. 화이트리스트 명령어인지 1차 필터링
    if command not in ALLOWED_COMMANDS:
        return False

    # 2. 각 명령어별 구체적 Pydantic 스키마 모델 검증
    try:
        if command == "set_recipe":
            SetRecipeCommand.model_validate(payload)
        elif command == "set_machine_enabled":
            SetMachineEnabledCommand.model_validate(payload)
        elif command == "connect_conveyor":
            ConnectConveyorCommand.model_validate(payload)
        elif command == "disconnect_conveyor":
            DisconnectConveyorCommand.model_validate(payload)
        elif command == "move_machine":
            MoveMachineCommand.model_validate(payload)
        elif command == "place_machine":
            PlaceMachineCommand.model_validate(payload)
        elif command == "remove_machine":
            RemoveMachineCommand.model_validate(payload)
        return True
    except ValidationError:
        return False
