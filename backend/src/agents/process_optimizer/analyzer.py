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
        input_shortages_items: dict[str, str] = {}
        output_blocked: list[str] = []

        maintenance_required_machines: list[str] = []
        broken_machines: list[str] = []

        for machine in machines:
            # Sprint 7: 내구도 및 고장 상태 분석 (disabled 상태와 무관하게 모든 장비 검사)
            if machine.maintenance_required or (machine.durability and machine.durability.ratio <= 0.3):
                maintenance_required_machines.append(machine.id)
            if machine.condition == "broken":
                broken_machines.append(machine.id)
                if machine.id not in maintenance_required_machines:
                    maintenance_required_machines.append(machine.id)

            # 비활성화된 장비(disabled)는 병목 계산에서 제외
            if machine.status == "disabled":
                continue

            # 입력 inventory 중 수량이 0인 아이템이 있으면 input_shortage로 감지
            for inp in machine.inputs:
                if inp.amount <= 0:
                    input_shortages.append(machine.id)
                    input_shortages_items[machine.id] = inp.item_id
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

        # 5.1 전력망 그래프 연결성 분석 (Sprint 2)
        isolated_power_nodes: list[str] = []
        disconnected_generators: list[str] = []
        unpowered_machines: list[str] = []

        nodes = grid.nodes
        generators = grid.generators

        if nodes:
            # 인접 리스트 구축
            adj: dict[str, set[str]] = {node.id: set() for node in nodes}
            for node in nodes:
                for conn_id in node.connected_node_ids:
                    if conn_id in adj:
                        adj[node.id].add(conn_id)
                        adj[conn_id].add(node.id)

            # Connected Components (연결 요소) 계산 (BFS)
            visited = set()
            components: list[set[str]] = []
            for node_id in adj:
                if node_id not in visited:
                    component = set()
                    queue = [node_id]
                    visited.add(node_id)
                    while queue:
                        curr = queue.pop(0)
                        component.add(curr)
                        for neighbor in adj[curr]:
                            if neighbor not in visited:
                                visited.add(neighbor)
                                queue.append(neighbor)
                    components.append(component)

            # 발전기 연결 정보 파악 및 disconnected_generators 식별
            powered_nodes_by_generators = set()
            for gen in generators:
                if not gen.connected_power_node_ids:
                    disconnected_generators.append(gen.id)
                else:
                    for node_id in gen.connected_power_node_ids:
                        powered_nodes_by_generators.add(node_id)

            # 고립 송전탑 식별 (발전기가 연결되지 않은 component에 속한 노드)
            for comp in components:
                has_generator = False
                for node_id in comp:
                    if node_id in powered_nodes_by_generators:
                        has_generator = True
                        break
                if not has_generator:
                    isolated_power_nodes.extend(comp)

            # unpowered_machines 식별 (유효한 발전기가 있는 전력망에 연결되지 않은 기기)
            # 연결된 송전탑 중 isolated_power_nodes에 속하지 않는 유효 송전탑이 하나라도 없으면 unpowered
            for m in machines:
                if m.type == "generator":
                    continue
                if not m.connected_power_node_ids:
                    unpowered_machines.append(m.id)
                else:
                    has_powered_connection = False
                    for node_id in m.connected_power_node_ids:
                        if node_id in adj and node_id not in isolated_power_nodes:
                            has_powered_connection = True
                            break
                    if not has_powered_connection:
                        unpowered_machines.append(m.id)

            # 리스트 정렬 (일관된 테스트 검증 목적)
            isolated_power_nodes.sort()
            disconnected_generators.sort()
            unpowered_machines.sort()

        # 5.5 Sprint 8: need_more_state 판단 분석
        required_scopes = []
        include_hints = []
        reasons = []

        storage_amounts_by_item: dict[str, float] = {}
        for storage in state_obj.storages:
            for inv in storage.inventory:
                storage_amounts_by_item[inv.item_id] = (
                    storage_amounts_by_item.get(inv.item_id, 0.0) + inv.amount
                )

        # 1) 입력 부족 + storages 없음
        if input_shortages and (not getattr(state_obj, "storages", None) or len(state_obj.storages) == 0):
            required_scopes.append("storage_inventory")
            include_hints.append("storages")
            ITEM_NAME_MAP_LOCAL = {
                "iron_ore": "철광석",
                "iron_plate": "철판",
                "iron_gear": "철 톱니바퀴",
                "copper_ore": "구리광석",
                "copper_wire": "구리선",
                "cable": "케이블",
                "reinforced_plate": "보강된 철판",
                "modular_frame": "모듈러 프레임",
                "rotor": "회전자",
                "screw": "나사",
            }
            item_names = []
            for m_id in input_shortages:
                i_id = input_shortages_items.get(m_id, "원자재")
                ko_name = ITEM_NAME_MAP_LOCAL.get(i_id, i_id)
                if ko_name not in item_names:
                    item_names.append(ko_name)
            item_desc = ", ".join(item_names)
            reasons.append(f"{item_desc} 부족 원인이 창고 재고 부족인지 공급 라인 문제인지 판단하려면 storage 상태가 필요합니다.")

        # 2) 철광석 부족 + resource node 상태 없음
        has_iron_ore_shortage = False
        for m_id in input_shortages:
            if input_shortages_items.get(m_id) == "iron_ore":
                has_iron_ore_shortage = True
                break
        has_storage_snapshot = bool(getattr(state_obj, "storages", None))
        iron_ore_stock_amount = storage_amounts_by_item.get("iron_ore", 0.0)
        should_request_resource_nodes = (
            has_iron_ore_shortage
            and has_storage_snapshot
            and iron_ore_stock_amount <= 0.0
            and (
                not getattr(state_obj, "resource_nodes", None)
                or len(state_obj.resource_nodes) == 0
            )
        )
        if should_request_resource_nodes:
            required_scopes.append("resource_nodes")
            include_hints.append("resource_nodes")
            reasons.append("철광석 부족 문제를 분석하기 위해서는 자원 노드(resource_nodes) 정보가 필요합니다.")

        # 3) 기계 idle + 입력/출력/전력 문제가 불명확 + condition 없음
        has_unresolved_idle = False
        for machine in machines:
            if machine.status == "idle":
                is_input_shortage = machine.id in input_shortages
                is_output_blocked = machine.id in output_blocked
                is_unpowered = machine.id in unpowered_machines
                is_power_issue = power_summary.power_issue

                if not is_input_shortage and not is_output_blocked and not is_unpowered and not is_power_issue:
                    if not machine.condition and not machine.durability:
                        has_unresolved_idle = True
                        break
        if has_unresolved_idle:
            required_scopes.append("machine_condition")
            include_hints.append("machine_condition")
            reasons.append("장비가 대기(idle) 상태인 원인을 파악하려면 장비 내구도와 고장 상태(machine_condition) 정보가 필요합니다.")

        # 4) 전력 부족 + power_grid.nodes 없음
        if power_summary.power_issue and (not state_obj.power_grid.nodes or len(state_obj.power_grid.nodes) == 0):
            required_scopes.append("power_grid")
            include_hints.append("power_grid")
            reasons.append("전력 부족 문제를 분석하여 송전망 연결 상태를 파악하려면 전력망(power_grid) 정보가 필요합니다.")

        need_more_state_payload = None
        if required_scopes:
            need_more_state_payload = {
                "status": "need_more_state",
                "reason": " ".join(reasons),
                "required_state_scopes": required_scopes,
                "next_request_hint": {
                    "agent": "process_optimizer",
                    "operation": "state_update",
                    "include": include_hints
                }
            }

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
            isolated_power_nodes=isolated_power_nodes,
            disconnected_generators=disconnected_generators,
            unpowered_machines=unpowered_machines,
            storages=state_obj.storages,
            input_shortages_items=input_shortages_items,
            maintenance_required_machines=maintenance_required_machines,
            broken_machines=broken_machines,
            need_more_state=need_more_state_payload,
        )
