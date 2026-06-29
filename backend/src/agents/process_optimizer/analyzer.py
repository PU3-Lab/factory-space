"""Unreal이 전달한 공장 상태를 결정론적으로 분석하는 모듈입니다.

장비 가동률, 입출력 병목, 컨베이어 정체, 전력 상태를 계산하여
최적화 제안 단계에서 사용할 분석 리포트로 변환합니다.
"""

from __future__ import annotations

from typing import Any

from agents.process_optimizer.schemas import (
    FactoryAnalysisReport,
    FactoryState,
    PowerSummary,
)


class FactoryStateAnalyzerTool:
    """공장의 현재 상태 snapshot을 분석하여 가동률, 병목(입력 부족/출력 적체), 컨베이어 정체, 전력 부족 등의 지표를 계산하는 분석 도구 클래스입니다.

    LLM이 직접 호출하거나, 그래프 실행 노드 내에서 결정론적(deterministic) 계산을 수행할 때 사용됩니다.
    """

    def analyze(
        self,
        factory_state: FactoryState | dict[str, Any] | None,
        factory_revision: int = 0,
        goal: str = "balance",
    ) -> FactoryAnalysisReport:
        """주어진 공장 상태 데이터를 바탕으로 성능 지표를 분석하고 정규화된 분석 리포트를 반환합니다.

        Args:
            factory_state: 분석할 공장 상태 정보 (Pydantic 모델 혹은 dictionary 형태).
            factory_revision: 공장 상태 스냅샷의 버전 번호.
            goal: 사용자가 지정한 최적화 목표 ("balance", "throughput", "power_saving", "congestion_relief").

        Returns:
            FactoryAnalysisReport: 계산된 지표 및 이슈가 담긴 리포트 객체.
        """
        # 1. 입력 형식이 딕셔너리일 경우 Pydantic 모델로 변환하여 유연하게 처리
        if isinstance(factory_state, dict):
            state_obj = FactoryState.model_validate(factory_state)
        elif isinstance(factory_state, FactoryState):
            state_obj = factory_state
        else:
            state_obj = FactoryState()

        # 2. 장비 가동률 계산
        machines = state_obj.machines
        if not machines:
            avg_operating_rate = 0.0
        else:
            total_op_rate = sum(m.operating_rate for m in machines)
            avg_operating_rate = total_op_rate / len(machines)

        # 3. 입력 부족(input_shortage) 및 출력 적체(output_blocked) 감지
        input_shortages: list[str] = []
        output_blocked: list[str] = []

        for machine in machines:
            # 비활성화된 장비(disabled)는 병목 계산에서 제외
            if machine.status == "disabled":
                continue

            # 입력 inventory 중 수량이 0인 아이템이 있으면 input_shortage로 감지
            for inp in machine.inputs:
                if inp.amount <= 0:
                    input_shortages.append(machine.id)
                    break

            # 출력 inventory 중 수량이 최대 용량(max_amount)에 도달했거나 가득 차면 output_blocked로 감지
            for out in machine.outputs:
                if out.amount >= out.max_amount:
                    output_blocked.append(machine.id)
                    break

        # 4. 컨베이어 혼잡 지표 계산
        conveyors = state_obj.conveyors
        congested_conveyors: list[str] = []
        if not conveyors:
            avg_conveyor_congestion = 0.0
        else:
            total_congestion = 0.0
            for conv in conveyors:
                total_congestion += conv.congestion_rate
                # 혼잡도가 0.8(80%) 이상일 경우 정체(congested) 상태로 분류
                if conv.congestion_rate >= 0.8:
                    congested_conveyors.append(conv.id)
            avg_conveyor_congestion = total_congestion / len(conveyors)

        # 5. 전력 상태 요약
        grid = state_obj.power_grid
        # 소비 전력이 생산 전력보다 클 경우 power_issue가 발생한 것으로 판단
        power_issue = grid.consumed > grid.produced

        power_summary = PowerSummary(
            produced=grid.produced,
            consumed=grid.consumed,
            power_issue=power_issue,
        )

        # 6. 정규화된 분석 결과 반환
        return FactoryAnalysisReport(
            factoryRevision=factory_revision,
            goal=goal,
            average_operating_rate=avg_operating_rate,
            input_shortages=input_shortages,
            output_blocked=output_blocked,
            congested_conveyors=congested_conveyors,
            average_conveyor_congestion=avg_conveyor_congestion,
            power_summary=power_summary,
        )
