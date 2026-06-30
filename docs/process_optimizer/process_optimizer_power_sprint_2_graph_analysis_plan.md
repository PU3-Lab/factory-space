# Process Optimizer Power Sprint 2: Graph Analysis

## 1. 목표

Sprint 2의 목표는 Sprint 1에서 받은 전력망 snapshot을 그래프로 분석하는 것이다.

백엔드는 송전탑 연결 정보를 기준으로 connected component를 계산하고, 고립 송전탑, 미연결 발전기, 전력 미공급 설비를 결정론적으로 찾는다.

## 2. 입력 기준

Sprint 2는 아래 구조가 schema validation을 통과한다는 전제에서 시작한다.

```text
factory_state.power_grid.nodes
factory_state.power_grid.generators
machine.connected_power_node_ids
generator.connected_power_node_ids
```

전력 연결 판단은 `connected_power_node_ids`를 우선한다.

```text
connected_power_node_ids가 빈 배열
-> 전력 노드에 연결되지 않은 것으로 판단

connected_power_node_ids에 ID가 있음
-> 해당 전력 노드에 연결된 것으로 판단
```

## 3. 분석 기준

```text
발전기가 하나 이상 연결된 component
-> 전력을 공급받을 수 있는 전력망으로 본다.

발전기가 없는 component
-> 고립 전력망 또는 전력 미공급 후보로 본다.

여러 개의 발전기 component가 존재
-> 각각 독립적인 유효 전력망으로 본다.
```

가장 큰 송전탑 그룹만 주 전력망으로 보지 않는다. 발전기가 연결되어 있는지가 더 중요한 기준이다.

## 4. 작업 범위

```text
- power_grid.nodes를 adjacency map으로 변환
- connected_node_ids 기반 BFS/DFS 탐색
- 전력망 connected component 계산
- 발전기가 연결된 component를 powered component로 판단
- 발전기가 없는 component를 isolated component 후보로 판단
- disconnected_generators 계산
- isolated_power_nodes 계산
- unpowered_machines 계산
- FactoryAnalysisReport에 전력망 분석 결과 추가
- 전력망 분석 단위 테스트 추가
```

## 5. 분석 예시

```text
송전탑 30개 중 pole_30만 connected_node_ids가 비어 있음
-> isolated_power_nodes = ["pole_30"]

발전기 5개 중 generator_5의 connected_power_node_ids가 빈 배열
-> disconnected_generators = ["generator_5"]

smelter_1이 pole_30에 연결되어 있고 pole_30 component에 발전기가 없음
-> unpowered_machines = ["smelter_1"]
```

## 6. 완료 기준

```text
- 30개 송전탑 중 1개 고립 케이스를 탐지한다.
- 5개 발전기 중 1개 미연결 케이스를 탐지한다.
- 고립 송전탑에 연결된 설비를 전력 미공급 설비로 계산한다.
- 발전기가 연결된 component는 powered component로 계산한다.
- produced/consumed 기반 기존 전력 부족 분석과 충돌하지 않는다.
```

## 7. 예상 테스트

```text
- nodes가 비어 있으면 기존 produced/consumed 분석만 수행한다.
- pole_30이 연결되지 않은 케이스에서 isolated_power_nodes에 포함된다.
- generator_5의 connected_power_node_ids가 빈 배열이면 disconnected_generators에 포함된다.
- smelter_1이 발전기 없는 component에 연결되어 있으면 unpowered_machines에 포함된다.
- 여러 발전기 component가 있을 때 각각 powered component로 계산된다.
```

## 8. 이번 Sprint에 포함하지 않는 것

```text
- preview 문구 생성
- LLM 설명 prompt 변경
- UI highlight 응답 확장
- suggested_subquest 생성
- 자동 전선 연결 명령 생성
```

Sprint 2는 계산까지만 담당한다.
