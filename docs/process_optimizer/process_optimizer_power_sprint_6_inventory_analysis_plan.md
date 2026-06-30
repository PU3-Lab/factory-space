# Process Optimizer Power Sprint 6: Storage Inventory Analysis

## 1. 목적

Sprint 6의 목표는 기계 입력 부족의 원인을 창고 재고와 함께 분석하는 것이다.

현재 입력 부족은 주로 machine input buffer를 보고 판단한다. 하지만 실제 게임에서는 제련기에 철광석이 없어도 창고에는 철광석이 충분할 수 있다. 이 경우 문제는 자원 부족이 아니라 공급 라인 문제다.

```text
기계 입력 부족
-> 창고 재고 확인
-> 창고에 재료 있음: 공급 라인/컨베이어 문제
-> 창고에도 재료 없음: 생산/채굴 부족 문제
```

## 2. 권장 Snapshot 필드

`factory_state`에 storage 정보를 추가한다.

```json
{
  "storages": [
    {
      "id": "storage_iron_ore_1",
      "type": "storage",
      "inventory": [
        {
          "item_id": "iron_ore",
          "amount": 0,
          "max_amount": 500
        }
      ],
      "connected_machine_ids": ["smelter_1"],
      "connected_conveyor_ids": ["conv_12"]
    }
  ]
}
```

## 3. 분석 기준

```text
machine.inputs[].amount == 0
-> 필요한 item_id를 찾는다.

storages inventory에 같은 item_id가 충분함
-> 입력 부족 원인은 공급 라인 문제로 분류한다.

storages inventory에도 같은 item_id가 부족함
-> 입력 부족 원인은 자원 생산/채굴 부족으로 분류한다.
```

예시:

```text
smelter_1 입력 iron_ore = 0
storage iron_ore = 300
-> smelter_1 공급 라인 점검 제안
```

```text
smelter_1 입력 iron_ore = 0
storage iron_ore = 0
-> iron_ore 채굴/생산량 확충 제안
```

## 4. 응답 방향

자동 실행이 아니라 제안형 최적화로 유지한다.

```json
{
  "id": "inspect_iron_ore_supply_smelter_1",
  "target": {
    "type": "machine",
    "id": "smelter_1"
  },
  "problem": "smelter_1에 iron_ore가 공급되지 않고 있습니다.",
  "recommended_action": "iron_ore 창고와 smelter_1 사이의 컨베이어 연결을 확인하십시오.",
  "expected_effect": "창고에 있는 iron_ore가 smelter_1로 공급될 수 있습니다.",
  "risk": "low"
}
```

## 5. 완료 기준

```text
- factory_state.storages schema가 validation을 통과한다.
- 입력 부족 item_id와 storage inventory를 비교한다.
- 창고 재고 있음/없음에 따라 문제 원인을 다르게 설명한다.
- 철광석 부족 상황을 서브퀘스트 objective로 만들 수 있다.
- 자동으로 자원 생산 시설을 설치하거나 연결하지 않는다.
```

## 6. 테스트 시나리오

```text
1. 창고에 철광석 있음
-> 공급 라인 문제 제안이 나온다.

2. 창고에도 철광석 없음
-> 철광석 생산/채굴 부족 제안이 나온다.

3. storage 정보 없음
-> 원인을 단정하지 않고 추가 상태 요청 후보로 처리한다.

4. 여러 창고에 재고가 분산됨
-> 같은 item_id의 총량을 합산해 판단한다.
```

## 7. 이번 Sprint에 포함하지 않는 것

```text
- 컨베이어 path finding
- 자동 컨베이어 연결
- 채굴기 자동 설치
- 장기 소비량 예측
```

