"""Unit tests for OptimizationSuggestionTool and SuggestionValidationTool."""

from __future__ import annotations

import pytest

from agents.process_optimizer.schemas import (
    FactoryAnalysisReport,
    OptimizationSuggestion,
    PowerSummary,
)
from agents.process_optimizer.suggestion import (
    OptimizationSuggestionTool,
    SuggestionValidationTool,
)


def test_suggestion_tool_basic_mapping() -> None:
    """입력 부족 및 출력 적체 등의 분석 결과가 제안 객체로 알맞게 매핑되는지 검증합니다."""
    tool = OptimizationSuggestionTool()

    report = FactoryAnalysisReport(
        factoryRevision=1,
        goal="balance",
        average_operating_rate=0.5,
        input_shortages=["machine_a"],
        output_blocked=["machine_b"],
        congested_conveyors=["conveyor_c"],
        average_conveyor_congestion=0.3,
        power_summary=PowerSummary(produced=100.0, consumed=50.0, power_issue=False),
    )

    suggestions, ui_hints = tool.generate_suggestions(report)

    # 총 3개 제안이 생성되어야 함 (input_shortage, output_blocked, conveyor_congestion)
    assert len(suggestions) == 3

    # 각 제안의 속성 검증
    # 1. machine_a 입력 부족 제안
    s1 = next(s for s in suggestions if "machine_a" in s.id)
    assert s1.target is not None
    assert s1.target.type == "machine"
    assert s1.target.id == "machine_a"
    assert "원자재 입력 재고" in s1.problem
    assert s1.risk == "low"
    assert s1.confidence == 1.0

    # 2. machine_b 출력 적체 제안
    s2 = next(s for s in suggestions if "machine_b" in s.id)
    assert s2.target is not None
    assert s2.target.type == "machine"
    assert s2.target.id == "machine_b"
    assert "생산품 출력 공간" in s2.problem
    assert s2.risk == "low"
    assert s2.confidence == pytest.approx(0.9)

    # 3. conveyor_c 혼잡 제안
    s3 = next(s for s in suggestions if "conveyor_c" in s.id)
    assert s3.target is not None
    assert s3.target.type == "conveyor"
    assert s3.target.id == "conveyor_c"
    assert "혼잡 상태" in s3.problem
    assert s3.risk == "low"
    assert s3.confidence == pytest.approx(0.8)

    # UI 하이라이트 타겟 힌트 검증
    assert "machine_a" in ui_hints.highlight_targets
    assert "machine_b" in ui_hints.highlight_targets
    assert "conveyor_c" in ui_hints.highlight_targets
    assert len(ui_hints.highlight_targets) == 3


def test_suggestion_priority_by_goals() -> None:
    """최적화 목표(goal)에 따라 생성되는 제안 후보들의 우선순위 정렬 및 최대 3개 필터링 동작을 검증합니다."""
    tool = OptimizationSuggestionTool()

    # 4가지 병목(입력 부족 1개, 출력 적체 1개, 전력 부족 1개, 컨베이어 정체 1개)이 모두 발생한 리포트
    report = FactoryAnalysisReport(
        factoryRevision=100,
        goal="balance",  # 목표는 가변적으로 테스트
        average_operating_rate=0.2,
        input_shortages=["smelter_1"],
        output_blocked=["constructor_1"],
        congested_conveyors=["conv_99"],
        average_conveyor_congestion=0.9,
        power_summary=PowerSummary(produced=100.0, consumed=150.0, power_issue=True),
    )

    # 1. throughput (생산량 극대화): input_shortage > output_blocked > congestion > power_issue
    # 따라서 smelter_1 (input), constructor_1 (output), conv_99 (congestion)이 출력되고 power_issue는 제외되어야 함
    report.goal = "throughput"
    suggestions, _ = tool.generate_suggestions(report)
    assert len(suggestions) == 3
    assert any(s.id == "suggest_input_smelter_1" for s in suggestions)
    assert any(s.id == "suggest_output_constructor_1" for s in suggestions)
    assert any(s.id == "suggest_conveyor_conv_99" for s in suggestions)
    assert not any(s.id == "suggest_power_issue" for s in suggestions)

    # 2. power_saving (전력 절약): power_issue > input_shortage > output_blocked > congestion
    # 따라서 power_issue, smelter_1 (input), constructor_1 (output)이 출력되고 conv_99 (congestion)은 제외
    report.goal = "power_saving"
    suggestions, _ = tool.generate_suggestions(report)
    assert len(suggestions) == 3
    assert any(s.id == "suggest_power_issue" for s in suggestions)
    assert any(s.id == "suggest_input_smelter_1" for s in suggestions)
    assert any(s.id == "suggest_output_constructor_1" for s in suggestions)
    assert not any(s.id == "suggest_conveyor_conv_99" for s in suggestions)

    # 3. congestion_relief (정체 해소): congestion > output_blocked > input_shortage > power_issue
    # 따라서 conv_99 (congestion), constructor_1 (output), smelter_1 (input)이 출력되고 power_issue 제외
    report.goal = "congestion_relief"
    suggestions, _ = tool.generate_suggestions(report)
    assert len(suggestions) == 3
    assert any(s.id == "suggest_conveyor_conv_99" for s in suggestions)
    assert any(s.id == "suggest_output_constructor_1" for s in suggestions)
    assert any(s.id == "suggest_input_smelter_1" for s in suggestions)
    assert not any(s.id == "suggest_power_issue" for s in suggestions)


def test_suggestion_validation_tool() -> None:
    """제안 검증 도구(SuggestionValidationTool)의 개수 제한 및 명령어 주입 시도 차단 성능을 검증합니다."""
    validator = SuggestionValidationTool()

    # 1. 정상 제안 (통과해야 함)
    valid_suggestions = [
        OptimizationSuggestion(
            id="s1",
            problem="공장의 전력 공급이 부족합니다.",
            recommended_action="발전기를 증설하여 전력 생산량을 증가시키세요.",
            expected_effect="전력 불안정 해소",
        ),
        OptimizationSuggestion(
            id="s2",
            problem="일부 컨베이어가 정체 상태입니다.",
            recommended_action="컨베이어 등급을 한 단계 올려 속도를 보완하십시오.",
            expected_effect="정체 완화",
        ),
    ]
    assert validator.validate_suggestions(valid_suggestions) is True

    # 2. 제안 개수 초과 (4개인 경우 실패해야 함)
    invalid_count = [
        OptimizationSuggestion(
            id="s1", problem="p", recommended_action="a", expected_effect="e"
        ),
        OptimizationSuggestion(
            id="s2", problem="p", recommended_action="a", expected_effect="e"
        ),
        OptimizationSuggestion(
            id="s3", problem="p", recommended_action="a", expected_effect="e"
        ),
        OptimizationSuggestion(
            id="s4", problem="p", recommended_action="a", expected_effect="e"
        ),
    ]
    assert validator.validate_suggestions(invalid_count) is False

    # 3. 원시 실행 명령어 주입 시도 차단 검증
    # 3.1 set_recipe 명령어 단어 단독 주입
    injection_cmd_1 = [
        OptimizationSuggestion(
            id="s1",
            problem="p",
            recommended_action="플레이어님, 콘솔에 set_recipe 명령을 입력해 주세요.",
            expected_effect="e",
        )
    ]
    assert validator.validate_suggestions(injection_cmd_1) is False

    # 3.2 connect_conveyor 명령어 주입
    injection_cmd_2 = [
        OptimizationSuggestion(
            id="s1",
            problem="p",
            recommended_action="connect_conveyor: conv_1 to machine_a",
            expected_effect="e",
        )
    ]
    assert validator.validate_suggestions(injection_cmd_2) is False

    # 4. JSON 형태 주입 시도 차단 검증
    injection_json = [
        OptimizationSuggestion(
            id="s1",
            problem="p",
            recommended_action='자동 실행 구문 {"command": "move_machine", "id": "m1"} 을 전송합니다.',
            expected_effect="e",
        )
    ]
    assert validator.validate_suggestions(injection_json) is False


def test_suggestion_tool_empty_report() -> None:
    """Validate that a report with no issues returns empty lists."""
    tool = OptimizationSuggestionTool()
    report = FactoryAnalysisReport(
        factoryRevision=42,
        goal="balance",
        average_operating_rate=1.0,
        input_shortages=[],
        output_blocked=[],
        congested_conveyors=[],
        average_conveyor_congestion=0.0,
        power_summary=PowerSummary(produced=100.0, consumed=50.0, power_issue=False),
    )
    suggestions, ui_hints = tool.generate_suggestions(report)
    assert len(suggestions) == 0
    assert len(ui_hints.highlight_targets) == 0


def test_suggestion_validation_case_insensitive_and_punctuation() -> None:
    """Validate that case variations and punctuation wrap of forbidden commands are blocked."""
    validator = SuggestionValidationTool()

    # Case 1: Case-mixed command 'SeT_mAcHiNe_EnAbLeD'
    injection_case = [
        OptimizationSuggestion(
            id="s1",
            problem="p",
            recommended_action="시스템 SeT_mAcHiNe_EnAbLeD 를 실행해라.",
            expected_effect="e",
        )
    ]
    assert validator.validate_suggestions(injection_case) is False

    # Case 2: Punctuation-wrapped command '[set_recipe]'
    injection_punct = [
        OptimizationSuggestion(
            id="s1",
            problem="p",
            recommended_action="반드시 [set_recipe] 하도록 설정하세요.",
            expected_effect="e",
        )
    ]
    assert validator.validate_suggestions(injection_punct) is False

    # Case 3: Punctuation suffix 'move_machine:'
    injection_suffix = [
        OptimizationSuggestion(
            id="s1",
            problem="p",
            recommended_action="move_machine: smelter_1 to (x, y)",
            expected_effect="e",
        )
    ]
    assert validator.validate_suggestions(injection_suffix) is False


def test_suggestion_priority_balance_goal() -> None:
    """Validate that balance goal prioritizes input_shortage and output_blocked equally above other issues."""
    tool = OptimizationSuggestionTool()
    report = FactoryAnalysisReport(
        factoryRevision=1,
        goal="balance",
        average_operating_rate=0.5,
        input_shortages=["machine_a"],
        output_blocked=["machine_b"],
        congested_conveyors=["conveyor_c"],
        power_summary=PowerSummary(produced=10.0, consumed=20.0, power_issue=True),
    )

    suggestions, _ = tool.generate_suggestions(report)

    # balance goal priority mapping has:
    # input_shortage: 1, output_blocked: 1
    # power_issue: 2, conveyor_congestion: 3
    # Therefore, the returned 3 suggestions must be:
    # machine_a (input_shortage), machine_b (output_blocked), and power_issue (weight 2).
    # conveyor_c (weight 3) should be filtered out.
    assert len(suggestions) == 3
    assert any(s.target is not None and s.target.id == "machine_a" for s in suggestions)
    assert any(s.target is not None and s.target.id == "machine_b" for s in suggestions)
    assert any(s.id == "suggest_power_issue" for s in suggestions)
    assert not any(
        s.target is not None and s.target.id == "conveyor_c" for s in suggestions
    )
