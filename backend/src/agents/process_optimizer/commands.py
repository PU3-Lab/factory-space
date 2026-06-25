"""Unreal command definitions and mapping for process optimizer v2.

초보자 설명:
이 모듈은 플레이어가 승인한 최적화 제안(OptimizationSuggestion)을 바탕으로
Unreal에 전송할 구조화된 명령 페이로드(payload)를 빌드하고 검증하는 역할을 맡습니다.
보안상 허용되지 않은 임의의 명령어가 생성되거나 전달되는 것을 막기 위해 화이트리스트 스키마를 고정하고 검증합니다.
"""

from __future__ import annotations

from typing import Any, Literal, Union
from pydantic import BaseModel, Field, ValidationError
from agents.process_optimizer.schemas import OptimizationSuggestion

# ==========================================
# 1. Unreal 실행 허용 명령 Pydantic 스키마 정의
# ==========================================

class BaseCommand(BaseModel):
    command: str

class SetRecipeCommand(BaseCommand):
    command: Literal["set_recipe"] = "set_recipe"
    machine_id: str
    recipe_id: str

class SetMachineEnabledCommand(BaseCommand):
    command: Literal["set_machine_enabled"] = "set_machine_enabled"
    machine_id: str
    enabled: bool

class ConnectConveyorCommand(BaseCommand):
    command: Literal["connect_conveyor"] = "connect_conveyor"
    conveyor_id: str
    before_target: str
    after_target: str

class DisconnectConveyorCommand(BaseCommand):
    command: Literal["disconnect_conveyor"] = "disconnect_conveyor"
    conveyor_id: str

class MoveMachineCommand(BaseCommand):
    command: Literal["move_machine"] = "move_machine"
    machine_id: str
    position: list[float]  # [x, y, z]

class PlaceMachineCommand(BaseCommand):
    command: Literal["place_machine"] = "place_machine"
    machine_type: str
    position: list[float]  # [x, y, z]

class RemoveMachineCommand(BaseCommand):
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
            "recipe_id": "iron_ingot"  # 기본 예시 레시피 ID
        }
    
    # 2. 기계 전원 활성화/비활성화 관련
    if "power" in action_text or "전원" in action_text or "가동" in action_text or "차단" in action_text:
        enabled = "차단" not in action_text and "off" not in action_text
        return {
            "command": "set_machine_enabled",
            "machine_id": target_id or "unknown_machine",
            "enabled": enabled
        }
        
    # 3. 컨베이어 연결 해제 관련
    if "disconnect" in action_text or "해제" in action_text:
        return {
            "command": "disconnect_conveyor",
            "conveyor_id": target_id or "unknown_conveyor"
        }

    # 4. 컨베이어 벨트 연결 관련 (가장 빈도가 높은 연결 권장안)
    if "connect" in action_text or "연결" in action_text or target_type == "conveyor" or "벨트" in action_text:
        return {
            "command": "connect_conveyor",
            "conveyor_id": target_id or "unknown_conveyor",
            "before_target": "storage_01",
            "after_target": target_id or "machine_01"
        }
        
    # 5. 장비 이동 관련
    if "move" in action_text or "이동" in action_text:
        return {
            "command": "move_machine",
            "machine_id": target_id or "unknown_machine",
            "position": [0.0, 0.0, 0.0]
        }
        
    # 6. 장비 배치/설치 관련
    if "place" in action_text or "건설" in action_text or "설치" in action_text:
        return {
            "command": "place_machine",
            "machine_type": "smelter",
            "position": [0.0, 0.0, 0.0]
        }
        
    # 7. 장비 철거 관련
    if "remove" in action_text or "철거" in action_text or "삭제" in action_text:
        return {
            "command": "remove_machine",
            "machine_id": target_id or "unknown_machine"
        }
        
    # 매칭되는 키워드가 전혀 없을 경우 기본 복구 행동으로 매핑 (컨베이어 연결)
    return {
        "command": "connect_conveyor",
        "conveyor_id": target_id or "conv_fallback",
        "before_target": "storage_01",
        "after_target": "smelter_01"
    }


def validate_command_payload(payload: dict[str, Any]) -> bool:
    """전달받은 명령 딕셔너리가 허용된 화이트리스트 명령 스키마에 완벽히 부합하는지 정밀 검증합니다."""
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
