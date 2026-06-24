"""Tools for generating and validating optimization suggestions based on analyzed factory state."""

from __future__ import annotations

import re
from typing import Any

from agents.process_optimizer.schemas import (
    FactoryAnalysisReport,
    OptimizationSuggestion,
    TargetDescriptor,
    UiHints,
)


class OptimizationSuggestionTool:
    """공장 상태 분석 리포트(FactoryAnalysisReport)를 바탕으로 규칙 기반의 최적화 제안 목록을 생성하는 도구입니다.

    최적화 목표(goal)에 따른 우선순위가 반영되며, 최대 3개의 제안과 Unreal UI용 하이라이트 대상을 반환합니다.
    """

    def generate_suggestions(
        self, report: FactoryAnalysisReport
    ) -> tuple[list[OptimizationSuggestion], UiHints]:
        """분석 리포트를 바탕으로 우선순위에 맞춰 최대 3개의 최적화 제안 후보를 생성합니다.

        Args:
            report: FactoryStateAnalyzerTool을 통해 계산된 지표 및 이슈 리포트.

        Returns:
            tuple[list[OptimizationSuggestion], UiHints]: 제안 목록 및 하이라이트 대상 힌트.
        """
        raw_candidates: list[dict[str, Any]] = []

        # 1. 입력 부족 이슈 후보 수집
        for machine_id in report.input_shortages:
            raw_candidates.append(
                {
                    "type": "input_shortage",
                    "id": f"suggest_input_{machine_id}",
                    "target": TargetDescriptor(type="machine", id=machine_id),
                    "problem": f"{machine_id} 설비의 원자재 입력 재고가 고갈되었습니다.",
                    "recommended_action": "공급 라인의 컨베이어 벨트 연결과 상류 설비의 생산 상태를 점검하십시오.",
                    "expected_effect": "설비 가동율이 복구되어 정상 공정이 가동됩니다.",
                    "risk": "low",
                    "confidence": 1.0,
                    "priority_key": "input_shortage",
                }
            )

        # 2. 출력 적체 이슈 후보 수집
        for machine_id in report.output_blocked:
            raw_candidates.append(
                {
                    "type": "output_blocked",
                    "id": f"suggest_output_{machine_id}",
                    "target": TargetDescriptor(type="machine", id=machine_id),
                    "problem": f"{machine_id} 설비의 생산품 출력 공간이 가득 차 공정이 멈췄습니다.",
                    "recommended_action": "출력 보관함을 정리하거나 하류 설비로 이송하는 컨베이어 벨트 속도를 향상시키십시오.",
                    "expected_effect": "적체 현상이 해소되어 정체되었던 자원의 흐름이 재개됩니다.",
                    "risk": "low",
                    "confidence": 0.9,
                    "priority_key": "output_blocked",
                }
            )

        # 3. 전력 부족 이슈 후보 수집
        if report.power_summary.power_issue:
            produced = report.power_summary.produced
            consumed = report.power_summary.consumed
            raw_candidates.append(
                {
                    "type": "power_issue",
                    "id": "suggest_power_issue",
                    "target": None,
                    "problem": f"공장 소비 전력({consumed}MW)이 공급 전력({produced}MW)을 초과했습니다.",
                    "recommended_action": "추가 발전 설비를 건설하거나 비필수 유휴 장비의 전원을 차단하십시오.",
                    "expected_effect": "전력 부하가 감소하고 전력망이 안정화되어 정전을 예방합니다.",
                    "risk": "medium",
                    "confidence": 1.0,
                    "priority_key": "power_issue",
                }
            )

        # 4. 컨베이어 혼잡 이슈 후보 수집
        for conveyor_id in report.congested_conveyors:
            raw_candidates.append(
                {
                    "type": "congestion",
                    "id": f"suggest_conveyor_{conveyor_id}",
                    "target": TargetDescriptor(type="conveyor", id=conveyor_id),
                    "problem": f"{conveyor_id} 컨베이어 벨트가 혼잡 상태입니다.",
                    "recommended_action": "이송 경로를 다각화하거나 더 빠른 등급의 컨베이어 벨트로 업그레이드하십시오.",
                    "expected_effect": "이송 병목이 해소되어 원자재 유입 속도가 향상됩니다.",
                    "risk": "low",
                    "confidence": 0.8,
                    "priority_key": "congestion",
                }
            )

        # 5. 최적화 목표(goal)에 따른 정렬 우선순위 정의
        # 우선순위가 높은 이슈 타입 순서대로 정렬하기 위해, 각 타입별 가중치 매핑
        goal = report.goal
        priority_map: dict[str, dict[str, int]] = {
            "throughput": {
                "input_shortage": 1,
                "output_blocked": 2,
                "congestion": 3,
                "power_issue": 4,
            },
            "power_saving": {
                "power_issue": 1,
                "input_shortage": 2,
                "output_blocked": 3,
                "congestion": 4,
            },
            "congestion_relief": {
                "congestion": 1,
                "output_blocked": 2,
                "input_shortage": 3,
                "power_issue": 4,
            },
            # balance (기본값)
            "balance": {
                "input_shortage": 1,
                "output_blocked": 1,  # 동일 우선순위
                "power_issue": 2,
                "congestion": 3,
            },
        }

        # 기본 맵 선택
        goal_priority = priority_map.get(goal, priority_map["balance"])

        def get_priority_weight(candidate: dict[str, Any]) -> int:
            return goal_priority.get(candidate["priority_key"], 99)

        # 우선순위 가중치 순서(오름차순)로 정렬
        sorted_candidates = sorted(raw_candidates, key=get_priority_weight)

        # 6. 최대 3개 선정 및 Pydantic 모델로 변환
        suggestions: list[OptimizationSuggestion] = []
        highlight_targets: list[str] = []

        for item in sorted_candidates[:3]:
            suggestion = OptimizationSuggestion(
                id=item["id"],
                target=item["target"],
                problem=item["problem"],
                recommended_action=item["recommended_action"],
                expected_effect=item["expected_effect"],
                risk=item["risk"],
                confidence=item["confidence"],
            )
            suggestions.append(suggestion)

            # 타겟 정보가 있고, 해당 타겟 ID가 하이라이트 대상에 누락되었으면 추가
            if item["target"] is not None:
                highlight_targets.append(item["target"].id)

        ui_hints = UiHints(highlight_targets=highlight_targets)
        return suggestions, ui_hints


class SuggestionValidationTool:
    """생성된 최적화 제안들이 정해진 비즈니스 규칙 및 보안 계약을 준수하는지 검증하는 도구입니다.

    제안의 개수 제한(최대 3개) 및 부적절한 자동 실행 원시 명령어 주입 시도(프롬프트 인젝션 방어)를 차단합니다.
    """

    # 백엔드가 실행하도록 허용하는 원시 변경 명령어 화이트리스트
    FORBIDDEN_COMMANDS = {
        "set_recipe",
        "set_machine_enabled",
        "connect_conveyor",
        "disconnect_conveyor",
        "move_machine",
        "place_machine",
        "remove_machine",
    }

    def validate_suggestions(self, suggestions: list[OptimizationSuggestion]) -> bool:
        """제안 목록에 부적절한 자동 실행 구문이나 보안 위배 사항이 포함되었는지 검증합니다.

        Args:
            suggestions: 검증할 최적화 제안 리스트.

        Returns:
            bool: 검증 통과 시 True, 규칙 위배 시 False.
        """
        # 1. 제안 개수는 최대 3개여야 함
        if len(suggestions) > 3:
            return False

        # 2. 각 제안의 텍스트 영역을 돌며 검증 수행
        for s in suggestions:
            # 검사할 필드 수집
            fields_to_check = [s.problem, s.recommended_action, s.expected_effect]

            for text in fields_to_check:
                if not text:
                    continue

                lower_text = text.lower()

                # 2.1 원시 명령어 단어가 텍스트 내에 단독 단어 혹은 명령 조작 구문 형태로 포함되었는지 검사
                for cmd in self.FORBIDDEN_COMMANDS:
                    # e.g., "set_recipe", "set_recipe:...", "set_recipe(...)" 형태 차단
                    pattern = rf"\b{re.escape(cmd)}\b"
                    if re.search(pattern, lower_text):
                        return False

                # 2.2 JSON 구조 주입 시도 차단 (e.g. { "command": ... } )
                # 중괄호 내부에 큰따옴표나 콜론이 들어간 경우 JSON 주입으로 간주
                if "{" in text and "}" in text:
                    # 중괄호 내부에 "command" 또는 콜론(:) 등이 발견되는지 확인
                    inner_content = re.findall(r"\{([^}]+)\}", text)
                    for content in inner_content:
                        if (
                            ":" in content
                            or '"' in content
                            or "command" in content.lower()
                        ):
                            return False

        return True
