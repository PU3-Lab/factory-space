"""Unit tests for Process Optimizer Unreal command payloads."""

import pytest
from agents.process_optimizer.schemas import OptimizationSuggestion, TargetDescriptor
from agents.process_optimizer.commands import (
    build_command_payload,
    validate_command_payload,
    SetRecipeCommand,
    SetMachineEnabledCommand
)

def test_command_build_and_validation():
    """OptimizationSuggestion을 기반으로 Unreal 명령 페이로드가 정상 생성 및 검증되는지 테스트합니다."""
    # 1. set_recipe 추천 제안 빌드 및 검증
    sug_recipe = OptimizationSuggestion(
        id="suggest_recipe_smelter_01",
        target=TargetDescriptor(type="machine", id="smelter_01"),
        problem="Incorrect recipe configuration.",
        recommended_action="Set the recipe to iron ingot.",
        expected_effect="Produce iron ingot."
    )
    
    payload = build_command_payload(sug_recipe)
    assert payload["command"] == "set_recipe"
    assert payload["machine_id"] == "smelter_01"
    assert payload["recipe_id"] == "iron_ingot"
    assert validate_command_payload(payload) is True

    # 2. set_machine_enabled 추천 제안 빌드 및 검증
    sug_power = OptimizationSuggestion(
        id="suggest_power_smelter_01",
        target=TargetDescriptor(type="machine", id="smelter_01"),
        problem="High power load.",
        recommended_action="Turn off machine smelter_01 to save power.",
        expected_effect="Reduce power consumption."
    )
    
    payload_power = build_command_payload(sug_power)
    assert payload_power["command"] == "set_machine_enabled"
    assert payload_power["machine_id"] == "smelter_01"
    assert payload_power["enabled"] is False
    assert validate_command_payload(payload_power) is True

def test_forbidden_command_validation_fails():
    """허용 목록에 없는 부적절한 명령어가 차단되는지 검증합니다."""
    # 화이트리스트 외부의 명령 주입 시도
    invalid_payload = {
        "command": "delete_all_machines",
        "machine_id": "all"
    }
    assert validate_command_payload(invalid_payload) is False

def test_malformed_command_payload_fails():
    """필수 필드가 누락되는 등 형식이 망가진 명령이 스키마 검증에서 차단되는지 검증합니다."""
    # set_recipe에서 recipe_id 누락
    malformed_recipe = {
        "command": "set_recipe",
        "machine_id": "smelter_01"
        # recipe_id 누락
    }
    assert validate_command_payload(malformed_recipe) is False
