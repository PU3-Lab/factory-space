# Process Optimizer Power Sprint 1: Snapshot Schema

## 1. 목표

전력망 최적화의 첫 단계는 Unreal이 보내는 전력망 snapshot을 백엔드가 안정적으로 받을 수 있게 만드는 것이다.

이 Sprint에서는 분석 로직을 깊게 만들지 않는다. 먼저 `factory_state` 안에 송전탑, 발전기, 설비의 전력 연결 정보를 일관된 구조로 받을 수 있도록 schema와 예시를 정리한다.

## 2. 핵심 원칙

```text
- 전력 연결 정보는 connected_power_node_ids 리스트를 기준으로 판단한다.
- 단일 connected_power_node_id는 사용하지 않는다.
- connected 값은 보조 정보로만 사용한다.
- connected와 connected_power_node_ids가 충돌하면 connected_power_node_ids를 우선한다.
- 발전기 전력망 분석 기준은 power_grid.generators로 둔다.
- machines 안의 type="generator" 정보는 일반 설비 상태나 UI 참고용으로만 사용한다.
```

## 3. 권장 Snapshot 구조

```json
{
  "factory_state": {
    "machines": [
      {
        "id": "smelter_1",
        "type": "smelter",
        "status": "idle",
        "operating_rate": 0,
        "power_consumption": 15,
        "connected_power_node_ids": ["pole_30"]
      }
    ],
    "conveyors": [],
    "power_grid": {
      "produced": 120,
      "consumed": 150,
      "nodes": [
        {
          "id": "pole_1",
          "type": "power_pole",
          "connected_node_ids": ["pole_2"],
          "connected_machine_ids": ["generator_1"]
        },
        {
          "id": "pole_30",
          "type": "power_pole",
          "connected_node_ids": [],
          "connected_machine_ids": ["smelter_1"]
        }
      ],
      "generators": [
        {
          "id": "generator_1",
          "produced": 30,
          "connected": true,
          "connected_power_node_ids": ["pole_1"]
        },
        {
          "id": "generator_5",
          "produced": 30,
          "connected": false,
          "connected_power_node_ids": []
        }
      ]
    }
  }
}
```

## 4. 작업 범위

```text
- schemas.py에 PowerNodeState 모델 추가
- schemas.py에 GeneratorPowerState 모델 추가
- PowerGridState에 nodes 필드 추가
- PowerGridState에 generators 필드 추가
- MachineState가 connected_power_node_ids를 받을 수 있는지 확인
- TargetDescriptor.type에 power_pole, generator 추가
- agent-test용 전력망 snapshot JSON 예시 추가
- schema validation 테스트 추가
```

## 5. 완료 기준

```text
- power_grid.nodes가 포함된 factory_state가 validation을 통과한다.
- power_grid.generators가 포함된 factory_state가 validation을 통과한다.
- connected_power_node_ids가 빈 배열인 발전기 snapshot을 받을 수 있다.
- target.type이 power_pole인 요청/응답 모델이 validation을 통과한다.
- target.type이 generator인 요청/응답 모델이 validation을 통과한다.
- 기존 analyze/state_update/apply/undo/measure 요청이 깨지지 않는다.
```

## 6. 예상 테스트

```text
- nodes와 generators가 없는 기존 factory_state도 통과한다.
- nodes와 generators가 있는 전력망 factory_state도 통과한다.
- connected=true이지만 connected_power_node_ids=[]인 발전기는 schema에서 받을 수 있다.
- TargetDescriptor(type="power_pole", id="pole_30")가 통과한다.
- TargetDescriptor(type="generator", id="generator_5")가 통과한다.
```

## 7. 이번 Sprint에 포함하지 않는 것

```text
- 전력망 그래프 BFS/DFS 분석
- 고립 송전탑 탐지
- 미연결 발전기 탐지
- 전력 미공급 설비 계산
- preview 제안 생성
- subquest 생성
- 자동 전선 연결 명령 생성
```

자동 전선 연결은 전체 범위에서도 제외한다.
