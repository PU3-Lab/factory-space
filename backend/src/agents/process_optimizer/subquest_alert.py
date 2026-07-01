"""주기적인 공장 상태 업데이트(state_update) 중 최적화가 필요한 문제를 감지하여 알림을 빌드합니다.

이 모듈은 FactoryAnalysisReport의 규칙 기반 분석 결과를 바탕으로,
가장 심각하고 시급한 단 하나의 문제를 선택하여 플레이어에게 서브퀘스트(Subquest)로 제안합니다.
"""

from __future__ import annotations

from typing import Any

from agents.process_optimizer.schemas import (
    FactoryAnalysisReport,
    FactoryState,
    OptimizationAlert,
    SuggestedSubquest,
    SuggestedSubquestNextRequest,
    TargetDescriptor,
)

# 장비 종류별 한글 표기 매핑
MACHINE_TYPE_MAP = {
    "smelter": "제련기",
    "constructor": "제작기",
    "assembler": "조립기",
    "manufacturer": "제조기",
}

# 주요 아이템 고유 ID별 한글 표기 매핑
ITEM_NAME_MAP = {
    "iron_ore": "철광석",
    "copper_ore": "구리광석",
    "coal": "석탄",
    "caterium_ore": "카테리움 광석",
    "limestone": "석회암",
    "iron_ingot": "철 주괴",
    "copper_ingot": "구리 주괴",
    "steel_ingot": "강철 주괴",
    "iron_plate": "철판",
    "iron_rod": "철봉",
    "wire": "전선",
    "cable": "케이블",
    "screw": "나사",
    "reinforced_iron_plate": "보강된 철판",
    "rotor": "로터",
    "stator": "스테이터",
    "modular_frame": "모듈러 프레임",
    "smart_plating": "스마트 도금판",
}


class SubquestAlertBuilder:
    """공장 분석 보고서(FactoryAnalysisReport) 및 현재 공장 상태(FactoryState)를 분석하여

    가장 우선순위가 높은 이슈를 탐지하고 최적화 알림(OptimizationAlert)을 생성하는 빌더 클래스입니다.
    """

    def build_alert(
        self,
        report: FactoryAnalysisReport,
        factory_state: FactoryState | dict[str, Any] | None,
        subquest_mode: bool = True,
    ) -> OptimizationAlert:
        """분석 보고서에서 전력 부족 -> 입력 부족 -> 출력 적체 -> 컨베이어 혼잡 순으로

        심각도가 높은 문제를 단 하나만 감지하여 최적화 알림을 빌드합니다.

        Args:
            report: FactoryStateAnalyzerTool에서 반환한 지표 분석 보고서.
            factory_state: 장비 및 컨베이어 상세 상태가 포함된 현재 공장 상태.
            subquest_mode: 서브퀘스트 생성을 활성화할지 여부. False인 경우 needed가 False인 알림을 반환합니다.

        Returns:
            OptimizationAlert: needed 상태 및 감지된 이슈에 대한 상세 제안서가 포함된 객체.
        """
        # 1. 서브퀘스트 모드가 꺼져있으면 감지를 수행하지 않고 필요한 상태가 아님으로 반환
        if not subquest_mode:
            return OptimizationAlert(needed=False)

        # Sprint 8: need_more_state 가 있으면 서브퀘스트 탐지를 무시하고 needed=False 반환
        if getattr(report, "need_more_state", None) is not None:
            return OptimizationAlert(needed=False)

        # 2. 공장 상태를 Pydantic 객체로 일관되게 정규화
        if isinstance(factory_state, dict):
            state_obj = FactoryState.model_validate(factory_state)
        elif isinstance(factory_state, FactoryState):
            state_obj = factory_state
        else:
            state_obj = FactoryState()

        # 3. 장비 조회를 위해 ID 기반 매핑을 구축
        machines_map = {m.id: m for m in state_obj.machines}

        # --- A. 전력 부족 이슈 탐지 (우선순위 1 - severity: high) ---
        if report.power_summary.power_issue:
            return OptimizationAlert(
                needed=True,
                severity="high",
                reason="공장 전력 공급이 부족합니다.",
                target=None,
                suggested_subquest=SuggestedSubquest(
                    title="전력 공급망 복구",
                    objective="공장의 전력 공급이 부족합니다. 발전 설비를 추가하거나 비필수 설비의 가동을 중단하여 전력망을 안정화하세요.",
                    target=None,
                    severity="high",
                    next_request=SuggestedSubquestNextRequest(
                        agent="process_optimizer",
                        operation="analyze",
                        goal=report.goal,
                        request_source="subquest",
                        target=None,
                    ),
                ),
            )

        # --- A2. 고립 송전탑 이슈 탐지 (Power Sprint 4) ---
        if report.isolated_power_nodes:
            pole_id = report.isolated_power_nodes[0]
            target_desc = TargetDescriptor(type="power_pole", id=pole_id)

            unpowered_machines_str = ""
            if report.unpowered_machines:
                unpowered_machines_str = ", ".join(report.unpowered_machines)

            if unpowered_machines_str:
                reason = f"{pole_id}이 주 전력망과 연결되어 있지 않아 {unpowered_machines_str}이 전력을 받지 못하고 있습니다."
                objective = f"{pole_id} 주변의 전력 연결을 확인하고 {unpowered_machines_str}에 전력이 공급되도록 직접 연결하세요."
            else:
                reason = f"{pole_id}이 주 전력망과 연결되어 있지 않습니다."
                objective = f"{pole_id} 주변의 전력 연결 상태를 확인하고, 플레이어가 직접 전선을 연결하십시오."

            return OptimizationAlert(
                needed=True,
                severity="medium",
                reason=reason,
                target=target_desc,
                suggested_subquest=SuggestedSubquest(
                    title="고립된 송전탑 확인",
                    objective=objective,
                    target=target_desc,
                    severity="medium",
                    next_request=SuggestedSubquestNextRequest(
                        agent="process_optimizer",
                        operation="analyze",
                        goal=report.goal,
                        request_source="subquest",
                        target=target_desc,
                    ),
                ),
            )

        # --- A3. 미연결 발전기 이슈 탐지 (Power Sprint 4) ---
        if report.disconnected_generators:
            gen_id = report.disconnected_generators[0]
            target_desc = TargetDescriptor(type="generator", id=gen_id)

            reason = f"{gen_id}가 전력망에 연결되어 있지 않아 생산 전력이 누락되고 있습니다."
            objective = f"{gen_id}와 가까운 송전탑의 연결 상태를 확인하고, 플레이어가 직접 연결하십시오."

            return OptimizationAlert(
                needed=True,
                severity="medium",
                reason=reason,
                target=target_desc,
                suggested_subquest=SuggestedSubquest(
                    title="미연결 발전기 확인",
                    objective=objective,
                    target=target_desc,
                    severity="medium",
                    next_request=SuggestedSubquestNextRequest(
                        agent="process_optimizer",
                        operation="analyze",
                        goal=report.goal,
                        request_source="subquest",
                        target=target_desc,
                    ),
                ),
            )

        # --- A4. 설비 정비 및 고장 이슈 탐지 (Power Sprint 7) ---
        if report.maintenance_required_machines:
            machine_id = report.maintenance_required_machines[0]
            is_broken = machine_id in report.broken_machines

            target_desc = TargetDescriptor(type="machine", id=machine_id)

            if is_broken:
                title = "고장 설비 점검"
                objective = f"{machine_id} 설비가 고장(broken) 상태입니다. 현장으로 가 장비를 점검하고 수리하십시오."
                reason = f"{machine_id} 설비가 고장 상태로 멈춰 있습니다."
            else:
                title = "설비 정비 수행"
                objective = f"{machine_id}의 내구도가 기준치 이하로 떨어졌습니다. 더 큰 손상을 방지하기 위해 정비를 수행하세요."
                reason = f"{machine_id}의 내구도가 낮아 정비가 필요합니다."

            return OptimizationAlert(
                needed=True,
                severity="medium",
                reason=reason,
                target=target_desc,
                suggested_subquest=SuggestedSubquest(
                    title=title,
                    objective=objective,
                    target=target_desc,
                    severity="medium",
                    next_request=SuggestedSubquestNextRequest(
                        agent="process_optimizer",
                        operation="analyze",
                        goal=report.goal,
                        request_source="subquest",
                        target=target_desc,
                    ),
                ),
            )

        # --- B. 입력 부족 이슈 탐지 (우선순위 2 - severity: medium) ---
        if report.input_shortages:
            machine_id = report.input_shortages[0]
            machine_obj = machines_map.get(machine_id)

            machine_type_ko = "장비"
            if machine_obj:
                machine_type_ko = MACHINE_TYPE_MAP.get(machine_obj.type, "장비")

            item_id = report.input_shortages_items.get(machine_id, "원자재")
            item_name_ko = ITEM_NAME_MAP.get(item_id, item_id)

            target_desc = TargetDescriptor(type="machine", id=machine_id)

            if not report.storages:
                title = f"{machine_type_ko} 입력 라인 복구"
                objective = f"{machine_id}에 {item_name_ko} 공급이 다시 들어오도록 컨베이어와 상류 설비를 확인하세요."
                reason = f"{machine_id}의 입력 재고가 부족합니다."
            else:
                total_amount = 0.0
                for storage in report.storages:
                    for inv in storage.inventory:
                        if inv.item_id == item_id:
                            total_amount += inv.amount

                if total_amount > 0.0:
                    title = f"{item_name_ko} 공급 라인 점검"
                    objective = f"{machine_id} 공급 창고에 {item_name_ko} 재고가 존재합니다. 창고와 {machine_id} 사이의 컨베이어 연결을 확인하여 공급을 복구하세요."
                    reason = f"{machine_id}에 {item_name_ko}가 공급되지 않고 있습니다. (창고 재고 있음)"
                else:
                    title = f"{item_name_ko} 생산량 확충"
                    objective = f"{machine_id}의 공급 창고에도 {item_name_ko}가 부족합니다. {item_name_ko} 채굴기나 생산 시설을 확충하여 절대적인 공급량을 늘리세요."
                    reason = f"{machine_id}의 공급 창고에도 {item_name_ko}가 부족합니다."

            return OptimizationAlert(
                needed=True,
                severity="medium",
                reason=reason,
                target=target_desc,
                suggested_subquest=SuggestedSubquest(
                    title=title,
                    objective=objective,
                    target=target_desc,
                    severity="medium",
                    next_request=SuggestedSubquestNextRequest(
                        agent="process_optimizer",
                        operation="analyze",
                        goal=report.goal,
                        request_source="subquest",
                        target=target_desc,
                    ),
                ),
            )

        # --- C. 출력 적체 이슈 탐지 (우선순위 3 - severity: medium) ---
        if report.output_blocked:
            machine_id = report.output_blocked[0]
            machine_obj = machines_map.get(machine_id)
            machine_type_ko = "장비"
            if machine_obj:
                machine_type_ko = MACHINE_TYPE_MAP.get(machine_obj.type, "장비")

            target_desc = TargetDescriptor(type="machine", id=machine_id)
            return OptimizationAlert(
                needed=True,
                severity="medium",
                reason=f"{machine_id}의 출력 공간이 가득 찼습니다.",
                target=target_desc,
                suggested_subquest=SuggestedSubquest(
                    title=f"{machine_type_ko} 출력 라인 적체 해소",
                    objective=f"{machine_id}의 생산품 출력 공간이 가득 차 공정이 멈췄습니다. 출력 벨트나 하류 적체를 해소하세요.",
                    target=target_desc,
                    severity="medium",
                    next_request=SuggestedSubquestNextRequest(
                        agent="process_optimizer",
                        operation="analyze",
                        goal=report.goal,
                        request_source="subquest",
                        target=target_desc,
                    ),
                ),
            )

        # --- D. 컨베이어 혼잡 이슈 탐지 (우선순위 4 - severity: low 또는 medium) ---
        if report.congested_conveyors:
            conveyor_id = report.congested_conveyors[0]
            # 최적화 목표가 congestion_relief 인 경우 병목 해소 최우선이므로 severity를 medium으로 격상
            severity = "medium" if report.goal == "congestion_relief" else "low"

            target_desc = TargetDescriptor(type="conveyor", id=conveyor_id)
            return OptimizationAlert(
                needed=True,
                severity=severity,
                reason=f"{conveyor_id} 컨베이어 병목이 감지되었습니다.",
                target=target_desc,
                suggested_subquest=SuggestedSubquest(
                    title="컨베이어 이송 정체 해소",
                    objective=f"{conveyor_id} 컨베이어의 혼잡도를 완화하기 위해 벨트를 업그레이드하거나 병목 라인을 분산시키세요.",
                    target=target_desc,
                    severity=severity,
                    next_request=SuggestedSubquestNextRequest(
                        agent="process_optimizer",
                        operation="analyze",
                        goal=report.goal,
                        request_source="subquest",
                        target=target_desc,
                    ),
                ),
            )


        # 4. 아무런 이슈도 감지되지 않은 정상 동작 상태
        return OptimizationAlert(needed=False)
