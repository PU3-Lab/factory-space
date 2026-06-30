# Process Optimizer Power Sprint 8: Missing State Request

## 1. 목적

Sprint 8의 목표는 분석에 필요한 상태가 부족할 때 백엔드가 추측하지 않고 Unreal에 추가 snapshot을 요청할 수 있게 하는 것이다.

예를 들어 제련기에 철광석이 없는데 창고 정보가 없으면, 백엔드는 철광석 생산 부족인지 공급 라인 문제인지 단정할 수 없다. 이때 `need_more_state` 응답으로 필요한 상태 범위를 요청한다.

Sprint 6에서는 창고 재고가 포함된 경우 입력 부족 원인을 다음처럼 구분한다.

```text
입력 부족 + 창고에 재고 있음
-> 공급 라인/컨베이어 문제

입력 부족 + 창고에도 재고 없음
-> 자원 생산/채굴 부족
```

하지만 `storages` 자체가 없으면 위 둘 중 어느 쪽인지 판단할 수 없다. Sprint 8에서는 이 경우 기존 입력 부족 제안으로 단정하지 않고, Unreal에 `storage_inventory` 범위의 snapshot을 다시 요청하는 흐름을 추가한다.

## 2. 응답 형태

```json
{
  "status": "need_more_state",
  "reason": "철광석 부족 원인이 창고 재고 부족인지 공급 라인 문제인지 판단하려면 storage 상태가 필요합니다.",
  "required_state_scopes": [
    "storage_inventory",
    "resource_nodes"
  ],
  "next_request_hint": {
    "agent": "process_optimizer",
    "operation": "state_update",
    "include": [
      "storages",
      "resource_nodes"
    ]
  }
}
```

## 3. 요청 가능한 상태 범위

```text
storage_inventory
-> 창고/컨테이너 재고 정보

resource_nodes
-> 채굴 가능한 자원 노드와 채굴기 연결 상태

machine_condition
-> 내구도, 고장, 정비 필요 상태

power_grid
-> 송전탑, 발전기, 전력 노드 연결 상태

conveyor_links
-> 컨베이어 연결과 방향 정보
```

## 4. 판단 기준

```text
입력 부족 + storages 없음
-> storage_inventory 요청

철광석 부족 + resource node 상태 없음
-> resource_nodes 요청

기계 idle + 입력/출력/전력 문제가 불명확 + condition 없음
-> machine_condition 요청

전력 부족 + power_grid.nodes 없음
-> power_grid 요청
```

### 4.1 Sprint 6 보완 기준

Sprint 6에서 남긴 보완점은 다음 기준으로 처리한다.

```text
machine.inputs[].amount <= 0
AND 부족 item_id를 알 수 있음
AND factory_state.storages 필드가 없음 또는 비어 있음
-> status=need_more_state
-> required_state_scopes=["storage_inventory"]
-> next_request_hint.include=["storages"]
```

이때 백엔드는 다음 내용을 말하지 않는다.

```text
- 창고에도 재고가 부족하다고 단정하지 않는다.
- 공급 라인이 끊겼다고 단정하지 않는다.
- 채굴기나 생산 시설을 늘리라고 단정하지 않는다.
- 자동으로 컨베이어나 채굴기를 설치하는 명령을 만들지 않는다.
```

`storages`가 들어온 이후에는 Sprint 6 로직을 다시 사용한다.

```text
storage 총량 > 0
-> 공급 라인 점검 preview 또는 subquest

storage 총량 <= 0
-> 생산/채굴 부족 preview 또는 subquest
```

## 5. 설계 원칙

```text
- 정보가 없으면 원인을 단정하지 않는다.
- need_more_state는 실행 명령이 아니다.
- Unreal은 required_state_scopes를 보고 다음 snapshot 범위를 결정한다.
- 필요한 상태가 들어오면 다시 analyze 또는 state_update로 분석한다.
```

## 6. 완료 기준

```text
- 정보 부족 상황에서 need_more_state 응답을 반환한다.
- required_state_scopes가 문제 원인에 맞게 채워진다.
- next_request_hint가 Unreal이 다시 보낼 operation과 include 목록을 제공한다.
- 입력 부족인데 storages가 없으면 기존 입력 부족 제안 대신 storage_inventory 요청을 우선한다.
- storage snapshot을 다시 받은 뒤에는 Sprint 6의 재고 있음/없음 분기 제안으로 돌아간다.
- 기존 preview/apply/undo/measure 흐름과 충돌하지 않는다.
- LLM은 부족한 정보를 추측해서 설명하지 않는다.
```

## 7. 테스트 시나리오

```text
1. 입력 부족인데 storages 없음
-> required_state_scopes에 storage_inventory가 포함된다.
-> preview changes를 만들지 않는다.

2. 전력 문제인데 power_grid.nodes 없음
-> required_state_scopes에 power_grid가 포함된다.

3. 기계 idle인데 원인이 불명확하고 durability 없음
-> required_state_scopes에 machine_condition이 포함된다.

4. 필요한 상태가 이미 모두 있음
-> need_more_state가 아니라 preview 또는 alert를 반환한다.

5. Sprint 6 연계 재분석
-> need_more_state 응답 이후 Unreal이 storages를 포함한 state_update/analyze를 보내면 공급 라인 또는 생산/채굴 부족 제안이 반환된다.
```

## 8. 이번 Sprint에 포함하지 않는 것

```text
- Unreal에 직접 RPC 호출
- 실시간 양방향 상태 스트리밍 제어
- DB 기반 장기 상태 분석
- 자동 최적화 실행
```
