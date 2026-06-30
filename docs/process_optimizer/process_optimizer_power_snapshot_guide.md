# Process Optimizer 전력망 Snapshot 가이드

## 1. 목적

이 문서는 Unreal이 `process_optimizer` Agent에 전력망 상태를 전달할 때 어떤 `factory_state` 구조를 보내면 좋은지 정리한다.

현재 목표는 자동으로 전선을 연결하는 것이 아니다. 백엔드는 전력망 문제를 계산하고, GPT-5.4-nano는 그 결과를 플레이어가 이해하기 쉬운 말로 설명한다. 실제 전선 연결이나 발전기 연결은 플레이어가 직접 수행한다.

```text
전력망 상태 수신
-> 백엔드가 고립 송전탑, 미연결 발전기, 전력 미공급 설비 계산
-> process_optimizer가 개선 방향을 설명
-> Unreal UI가 대상 설비/송전탑/발전기를 하이라이트
-> 플레이어가 직접 전선을 연결하거나 발전기를 송전탑에 연결
```

## 2. 현재 구현 기준

현재 `process_optimizer`가 기본적으로 계산할 수 있는 전력 정보는 `power_grid.produced`와 `power_grid.consumed`다.

```json
{
  "power_grid": {
    "produced": 120,
    "consumed": 150
  }
}
```

이 값만 있으면 백엔드는 다음 정도까지 판단할 수 있다.

```text
consumed > produced
-> 전력 부족 문제가 있다.
```

하지만 아래 원인은 이 값만으로는 알 수 없다.

```text
- 송전탑 30개 중 1개가 전력망에서 고립됨
- 발전기 5개 중 1개가 송전탑에 연결되지 않음
- 특정 설비가 고립된 송전탑에 연결되어 전력을 받지 못함
- 전선 연결이 끊겨 전력망이 여러 그룹으로 분리됨
```

이 문제까지 판단하려면 Unreal이 전력망 연결 snapshot을 추가로 보내야 한다.

## 3. connected_power_node_ids 규칙

Unreal 시스템상 설비나 발전기가 여러 송전탑 또는 전력 노드에 연결될 수 있으므로, 단일 값인 `connected_power_node_id`가 아니라 리스트 값인 `connected_power_node_ids`를 사용한다.

```text
connected_power_node_ids: []
-> 연결된 전력 노드가 없음

connected_power_node_ids: ["pole_1"]
-> pole_1에 연결됨

connected_power_node_ids: ["pole_12", "pole_13"]
-> pole_12와 pole_13에 연결됨
```

예시:

```json
{
  "id": "smelter_1",
  "type": "smelter",
  "status": "idle",
  "operating_rate": 0,
  "power_consumption": 15,
  "connected_power_node_ids": ["pole_12", "pole_13"]
}
```

위 설비는 `pole_12`, `pole_13`에 연결되어 있다는 뜻이다. 둘 중 하나라도 정상 전력망에 연결되어 있다면 전력 공급 가능성이 있다고 볼 수 있다.

발전기 예시:

```json
{
  "id": "generator_1",
  "produced": 30,
  "connected": true,
  "connected_power_node_ids": ["pole_1"]
}
```

연결되지 않은 발전기는 `null`보다 빈 배열을 권장한다.

```json
{
  "id": "generator_5",
  "produced": 30,
  "connected": false,
  "connected_power_node_ids": []
}
```

## 4. 권장 factory_state 구조

전력망 문제를 분석하려면 `factory_state.power_grid` 안에 `nodes`와 `generators` 정보를 함께 보내는 것을 권장한다.

```json
{
  "factory_state": {
    "machines": [
      {
        "id": "generator_1",
        "type": "generator",
        "status": "operating",
        "power_output": 30,
        "connected_power_node_ids": ["pole_1"]
      },
      {
        "id": "generator_5",
        "type": "generator",
        "status": "operating",
        "power_output": 30,
        "connected_power_node_ids": []
      },
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
          "id": "pole_2",
          "type": "power_pole",
          "connected_node_ids": ["pole_1", "pole_3"],
          "connected_machine_ids": []
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

현재 Pydantic schema는 extra field를 허용하므로 위와 같은 추가 필드가 들어와도 snapshot 자체는 받을 수 있다. 다만 현재 analyzer는 아직 전력망 그래프를 직접 순회해서 고립 송전탑이나 미연결 발전기를 계산하지 않는다. 이 문서는 다음 구현을 위한 권장 계약이다.

### 4.1 전력망 분석 기준

전력망 분석에서는 같은 정보가 여러 위치에 중복될 수 있으므로, 아래 기준을 따른다.

```text
발전기 전력망 연결 분석 기준
-> power_grid.generators를 우선 사용한다.

machines 안의 type="generator"
-> 일반 설비 상태나 UI 표시용으로 참고할 수 있다.

power_grid.generators와 machines의 발전기 정보가 모두 있을 때
-> 전력망 연결 분석은 power_grid.generators를 기준으로 한다.
```

`connected`와 `connected_power_node_ids`가 함께 들어온 경우에는 `connected_power_node_ids`를 더 신뢰한다.

```text
connected_power_node_ids가 빈 배열
-> 전력 노드에 연결되지 않은 것으로 판단한다.

connected_power_node_ids에 하나 이상의 ID가 있음
-> 해당 전력 노드에 연결된 것으로 판단한다.

connected 값
-> Unreal UI나 디버깅용 보조 정보로만 사용한다.
```

예를 들어 아래처럼 `connected`가 `true`여도 `connected_power_node_ids`가 비어 있으면, 백엔드는 미연결 발전기로 판단한다.

```json
{
  "id": "generator_5",
  "produced": 30,
  "connected": true,
  "connected_power_node_ids": []
}
```

### 4.2 TargetDescriptor 확장 필요

현재 구현의 `TargetDescriptor`는 주로 설비와 컨베이어를 대상으로 사용한다. 전력망 최적화에서는 송전탑과 발전기를 직접 가리켜야 하므로 Sprint 1에서 target type을 확장한다.

```text
추가할 target type:
- power_pole
- generator
```

전력망 관련 preview, alert, subquest는 아래처럼 대상을 명확히 보낸다.

```json
{
  "target": {
    "type": "power_pole",
    "id": "pole_30"
  }
}
```

```json
{
  "target": {
    "type": "generator",
    "id": "generator_5"
  }
}
```

### 4.3 전력망 component 판단 기준

전력망 그래프 분석에서는 송전탑 연결을 connected component로 나눈다. 각 component는 아래처럼 판단한다.

```text
발전기가 하나 이상 연결된 component
-> 전력을 공급받을 수 있는 전력망으로 본다.

발전기가 없는 component
-> 고립 전력망 또는 전력 미공급 후보로 본다.

여러 개의 발전기 component가 존재
-> 각각 독립적인 유효 전력망으로 본다.
```

따라서 단순히 가장 큰 송전탑 그룹만 주 전력망으로 보지 않는다. 발전기가 연결되어 있는지가 더 중요한 기준이다.

## 5. 필드 설명

| 필드 | 의미 | 사용 목적 |
| --- | --- | --- |
| `power_grid.produced` | 전체 생산 전력 | 전력 부족 여부 계산 |
| `power_grid.consumed` | 전체 소비 전력 | 전력 부족 여부 계산 |
| `power_grid.nodes` | 송전탑 또는 전력 노드 목록 | 전력망 연결 그래프 구성 |
| `nodes[].id` | 송전탑 ID | 문제 대상 식별 |
| `nodes[].connected_node_ids` | 직접 연결된 다른 송전탑 ID 목록 | 고립 노드 탐색 |
| `nodes[].connected_machine_ids` | 해당 송전탑에 연결된 설비 ID 목록 | 전력을 받는 설비 확인 |
| `power_grid.generators` | 발전기 목록 | 미연결 발전기 탐지 |
| `generators[].connected` | 발전기가 전력망에 연결되었는지 여부 | 전력 생산 누락 원인 판단 |
| `connected_power_node_ids` | 설비 또는 발전기가 연결된 송전탑 ID 목록 | 설비와 전력망 연결 관계 확인 |

## 6. 분석 가능해지는 문제

이 구조가 들어오면 백엔드는 이후 다음 문제를 계산할 수 있다.

```text
1. 전력망에서 고립된 송전탑 탐지
2. 전력망에 연결되지 않은 발전기 탐지
3. 전력을 받지 못하는 설비 탐지
4. 전력 생산량은 충분하지만 연결 문제로 설비가 멈춘 상황 탐지
5. 어떤 송전탑 또는 발전기를 확인해야 하는지 개선안 생성
```

예를 들어 송전탑이 30개 있는데 29개만 연결되어 있고 `pole_30`이 고립되어 있다면, 백엔드는 다음과 같은 개선안을 만들 수 있다.

```json
{
  "id": "inspect_power_pole_30",
  "target": {
    "type": "power_pole",
    "id": "pole_30"
  },
  "problem": "pole_30이 주 전력망과 연결되어 있지 않습니다.",
  "recommended_action": "pole_30 주변의 송전탑 연결을 확인하고, 플레이어가 직접 전선을 연결하십시오.",
  "expected_effect": "pole_30에 연결된 설비가 전력을 공급받을 수 있습니다.",
  "risk": "medium"
}
```

발전기가 5개 있고 그중 `generator_5`만 송전탑에 연결되지 않았다면 다음과 같은 개선안이 가능하다.

```json
{
  "id": "inspect_generator_5_power_connection",
  "target": {
    "type": "generator",
    "id": "generator_5"
  },
  "problem": "generator_5가 전력망에 연결되어 있지 않습니다.",
  "recommended_action": "generator_5와 가까운 송전탑의 연결 상태를 확인하고, 플레이어가 직접 연결하십시오.",
  "expected_effect": "generator_5의 생산 전력이 전력망에 반영될 수 있습니다.",
  "risk": "medium"
}
```

## 7. 자동 실행 제외 원칙

전력망 최적화는 자동 실행하지 않는다.

```text
하지 않는 것:
- AI가 전선을 자동으로 연결하지 않는다.
- 백엔드가 connect_power_line 명령을 만들지 않는다.
- 플레이어 승인만으로 송전탑/발전기를 자동 연결하지 않는다.
```

현재 목표는 제안형 최적화다.

```text
하는 것:
- 전력망 문제를 계산한다.
- 문제 대상 송전탑, 발전기, 설비를 알려준다.
- Unreal UI가 해당 대상을 하이라이트할 수 있게 한다.
- GPT-5.4-nano가 플레이어용 설명을 자연스럽게 만든다.
- 서브퀘스트 목표로 연결한다.
- 플레이어가 직접 전선을 연결하거나 발전기를 연결한다.
```

## 8. 전력망 그래프 분석 Sprint 계획

전력망 연결 최적화는 단순히 `analyzer.py`에 조건문을 추가하는 작업이 아니다. Unreal snapshot 계약, 백엔드 schema, 분석 지표, 제안 생성, UI highlight, 서브퀘스트 연결이 함께 맞아야 한다.

따라서 아래처럼 4개 sprint로 나누어 진행한다.

각 Sprint의 상세 구현 계획은 아래 문서를 기준으로 한다.

```text
Sprint 1: docs/process_optimizer/process_optimizer_power_sprint_1_snapshot_schema_plan.md
Sprint 2: docs/process_optimizer/process_optimizer_power_sprint_2_graph_analysis_plan.md
Sprint 3: docs/process_optimizer/process_optimizer_power_sprint_3_suggestion_highlight_plan.md
Sprint 4: docs/process_optimizer/process_optimizer_power_sprint_4_subquest_plan.md
```

### Sprint 1. 전력망 Snapshot 계약과 Schema 확장

목표는 Unreal이 보내는 전력망 연결 정보를 백엔드가 안정적으로 받을 수 있게 만드는 것이다.

작업 범위:

```text
- power_grid.nodes 구조 확정
- power_grid.generators 구조 확정
- machine.connected_power_node_ids 사용 확정
- generator.connected_power_node_ids 사용 확정
- 발전기 분석 기준은 power_grid.generators로 확정
- connected와 connected_power_node_ids가 충돌할 때 connected_power_node_ids 우선
- TargetDescriptor.type에 power_pole, generator 추가
- schemas.py에 PowerNodeState 모델 추가
- schemas.py에 GeneratorPowerState 모델 추가
- PowerGridState에 nodes, generators 필드 추가
- agent-test용 전력망 snapshot JSON 예시 추가
- schema validation 테스트 추가
```

완료 기준:

```text
- 송전탑/발전기 연결 정보가 포함된 factory_state가 validation을 통과한다.
- target.type이 power_pole 또는 generator인 preview/alert/subquest가 validation을 통과한다.
- 기존 analyze/state_update/apply/undo/measure 요청이 깨지지 않는다.
- Unreal이 어떤 필드를 보내야 하는지 문서와 예시 JSON으로 확인할 수 있다.
```

예상 테스트:

```text
- power_grid.nodes가 비어 있어도 기존 전력 총량 분석이 동작한다.
- power_grid.nodes와 generators가 들어와도 FactoryState validation이 통과한다.
- connected_power_node_ids가 빈 배열인 발전기 snapshot을 받을 수 있다.
- connected=true이지만 connected_power_node_ids=[]인 발전기는 미연결로 판단할 준비가 되어 있다.
- TargetDescriptor(type="power_pole")와 TargetDescriptor(type="generator")가 validation을 통과한다.
```

### Sprint 2. analyzer.py 전력망 그래프 분석 추가

목표는 송전탑 연결 상태를 그래프로 분석해 고립 송전탑, 미연결 발전기, 전력 미공급 설비를 계산하는 것이다.

작업 범위:

```text
- power_grid.nodes를 adjacency map으로 변환
- connected_node_ids 기반 BFS/DFS 탐색
- 전력망 connected component 계산
- 발전기가 연결된 component를 powered component로 판단
- 발전기가 없는 component를 고립 또는 전력 미공급 후보로 판단
- disconnected_generators 계산
- isolated_power_nodes 계산
- unpowered_machines 계산
- FactoryAnalysisReport에 전력망 분석 결과 추가
- 전력망 분석 단위 테스트 추가
```

분석 예시:

```text
송전탑 30개 중 pole_30만 connected_node_ids가 비어 있음
-> isolated_power_nodes = ["pole_30"]

발전기 5개 중 generator_5의 connected_power_node_ids가 빈 배열
-> disconnected_generators = ["generator_5"]

smelter_1이 pole_30에 연결되어 있고 pole_30이 고립 상태
-> unpowered_machines = ["smelter_1"]
```

완료 기준:

```text
- 30개 송전탑 중 1개 고립 케이스를 탐지한다.
- 5개 발전기 중 1개 미연결 케이스를 탐지한다.
- 고립 송전탑에 연결된 설비를 전력 미공급 설비로 계산한다.
- produced/consumed 기반 기존 전력 부족 분석과 충돌하지 않는다.
```

### Sprint 3. 개선안과 UI Highlight 추가

목표는 전력망 분석 결과를 플레이어가 이해할 수 있는 최적화 제안으로 변환하는 것이다.

작업 범위:

```text
- suggestion.py에서 isolated_power_nodes 개선안 생성
- suggestion.py에서 disconnected_generators 개선안 생성
- suggestion.py에서 unpowered_machines 안내 생성
- ui_hints.highlight_targets에 송전탑/발전기/영향 설비 ID 포함
- LLM 설명 prompt에 전력망 분석 결과 전달
- agent-test 예시 JSON과 예상 응답 추가
```

완료 기준:

```text
- 고립 송전탑 문제가 preview changes에 표시된다.
- 미연결 발전기 문제가 preview changes에 표시된다.
- Unreal UI가 문제 송전탑/발전기를 highlight할 수 있다.
- GPT-5.4-nano는 계산 결과를 설명만 보강하고, 임의의 전력 연결 명령을 만들지 않는다.
```

### Sprint 4. 전력망 제안형 최적화의 서브퀘스트 연결

목표는 전력망 최적화 제안을 자동 실행 명령으로 처리하지 않고, 플레이어가 직접 해결할 수 있는 서브퀘스트로 연결하는 것이다.

이 sprint까지 완료하면 전력망 최적화는 1차 완성으로 본다. 즉, AI가 전력선을 직접 연결하지 않고 문제 위치와 해결 목표를 제시하며, Unreal UI가 이를 서브퀘스트와 하이라이트로 보여주는 구조다.

작업 범위:

```text
- state_update에서 전력망 문제가 감지되면 optimization_alert 생성
- isolated_power_nodes를 suggested_subquest로 변환
- disconnected_generators를 suggested_subquest로 변환
- unpowered_machines를 objective 문구에 포함
- suggested_subquest.next_request에 request_source="subquest" 포함
- target에 power_pole / generator / machine 식별자 포함
- ui_hints.highlight_targets에 문제 송전탑, 발전기, 영향받는 설비 포함
- agent-test용 state_update -> subquest -> analyze 예시 추가
- subquest flow 테스트 추가
```

서브퀘스트 응답 예시:

```json
{
  "optimization_alert": {
    "needed": true,
    "severity": "medium",
    "reason": "pole_30이 주 전력망과 연결되어 있지 않아 smelter_1이 전력을 받지 못하고 있습니다.",
    "target": {
      "type": "power_pole",
      "id": "pole_30"
    },
    "suggested_subquest": {
      "title": "고립된 송전탑 확인",
      "objective": "pole_30 주변의 전력 연결을 확인하고 smelter_1에 전력이 공급되도록 직접 연결하세요.",
      "target": {
        "type": "power_pole",
        "id": "pole_30"
      },
      "severity": "medium",
      "next_request": {
        "agent": "process_optimizer",
        "operation": "analyze",
        "goal": "power_saving",
        "request_source": "subquest",
        "target": {
          "type": "power_pole",
          "id": "pole_30"
        }
      }
    }
  }
}
```

완료 기준:

```text
- 주기 state_update만으로 전력망 문제 alert가 생성된다.
- 고립 송전탑 문제가 서브퀘스트 제목과 목표로 변환된다.
- 미연결 발전기 문제가 서브퀘스트 제목과 목표로 변환된다.
- 서브퀘스트에서 넘어온 analyze 요청이 target 중심으로 preview를 반환한다.
- Unreal UI가 target과 highlight_targets를 사용해 문제 위치를 표시할 수 있다.
- 전력망 문제는 자동 실행되지 않고, 플레이어가 직접 해결하는 제안형 최적화로 유지된다.
```

Sprint 4 이후 기본 운영 흐름:

```text
1. Unreal이 주기적으로 factory_state를 보낸다.
2. 백엔드가 전력망 그래프를 분석한다.
3. 고립 송전탑 또는 미연결 발전기가 있으면 optimization_alert를 반환한다.
4. Unreal이 이를 서브퀘스트 UI로 표시한다.
5. 플레이어가 직접 전선을 연결하거나 발전기를 송전탑에 연결한다.
6. 다음 state_update 또는 analyze에서 문제가 해결되었는지 다시 확인한다.
```

## 9. 구현 우선순위

우선순위는 다음과 같다.

```text
1. Snapshot 계약 확정
2. Schema와 테스트 추가
3. analyzer.py 전력망 그래프 분석
4. suggestion.py 개선안 생성
5. UI highlight 연동
6. 서브퀘스트 연결
```

제안형 최적화 기준으로는 Sprint 4에서 마무리한다.

```text
Sprint 1: Snapshot 계약
Sprint 2: 전력망 그래프 분석
Sprint 3: 개선안과 highlight
Sprint 4: 서브퀘스트 연결
-> 1차 완성
```

자동 실행형 최적화는 현재 범위에 포함하지 않는다.

## 10. Sprint 5~8 운영형 확장 계획

Sprint 1~4가 전력망 문제를 제안형 최적화로 1차 완성하는 범위라면, Sprint 5~8은 공장 상태를 더 오래 기억하고 더 넓은 원인을 판단하기 위한 운영형 확장이다.

이 확장은 자동 최적화가 아니라, 주기 snapshot을 바탕으로 더 정확한 서브퀘스트와 추가 상태 요청을 만들기 위한 작업이다.

각 Sprint의 상세 구현 계획은 아래 문서를 기준으로 한다.

```text
Sprint 5: docs/process_optimizer/process_optimizer_power_sprint_5_snapshot_store_plan.md
Sprint 6: docs/process_optimizer/process_optimizer_power_sprint_6_inventory_analysis_plan.md
Sprint 7: docs/process_optimizer/process_optimizer_power_sprint_7_machine_condition_plan.md
Sprint 8: docs/process_optimizer/process_optimizer_power_sprint_8_missing_state_request_plan.md
```

### Sprint 5. Snapshot Store

목표는 Unreal이 주기적으로 보내는 `state_update`를 백엔드가 session 단위로 기억하는 것이다.

```text
Unreal state_update
-> 최신 factory_state 저장
-> factoryRevision과 updated_at 저장
-> 다음 analyze 또는 subquest 판단에서 최신 snapshot 참조
```

### Sprint 6. Storage Inventory 분석

목표는 기계 입력 부족의 원인을 창고 재고와 함께 판단하는 것이다.

```text
기계 입력 부족 + 창고에 재료 있음
-> 공급 라인 문제

기계 입력 부족 + 창고에도 재료 없음
-> 자원 생산/채굴 부족 문제
```

### Sprint 7. Machine Condition / Durability 분석

목표는 기계 내구도와 정비 필요 상태를 최적화 제안에 반영하는 것이다.

```text
durability.ratio 낮음
-> 정비 필요 제안

maintenance_required=true
-> 정비 서브퀘스트 후보
```

### Sprint 8. Missing State Request

목표는 상태 정보가 부족할 때 백엔드가 원인을 추측하지 않고 Unreal에 필요한 snapshot 범위를 요청하는 것이다.

Sprint 6의 창고 재고 분석에서 `storages`가 없으면 공급 라인 문제인지 생산/채굴 부족인지 단정하지 않고 `storage_inventory`를 요청한다.

```json
{
  "status": "need_more_state",
  "required_state_scopes": [
    "storage_inventory",
    "machine_condition"
  ]
}
```

### Sprint 5~8 완료 후 기대 흐름

```text
1. Unreal이 주기적으로 factory_state를 보낸다.
2. 백엔드가 최신 snapshot을 기억한다.
3. 입력 부족, 전력 문제, 창고 재고 부족, 내구도 문제를 함께 분석한다.
4. 정보가 부족하면 need_more_state로 추가 snapshot을 요청한다.
5. 문제가 명확하면 서브퀘스트 또는 preview 제안으로 반환한다.
6. 플레이어가 직접 해결하고, 다음 state_update에서 개선 여부를 다시 확인한다.
```
