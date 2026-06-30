# Process Optimizer Power Sprint 7: Machine Condition And Durability

## 1. 목적

Sprint 7의 목표는 기계의 내구도와 상태 정보를 받아 고장 또는 정비 필요 상태를 최적화 제안에 반영하는 것이다.

입력, 출력, 전력 문제가 없어도 기계 내구도가 낮으면 생산 효율이 떨어지거나 곧 멈출 수 있다. 이 정보는 `durability`, `condition`, `maintenance_required`로 전달받는 것을 권장한다.

## 2. 권장 Snapshot 필드

```json
{
  "id": "smelter_1",
  "type": "smelter",
  "status": "idle",
  "operating_rate": 0,
  "power_consumption": 15,
  "durability": {
    "current": 42,
    "max": 100,
    "ratio": 0.42
  },
  "condition": "damaged",
  "maintenance_required": true
}
```

## 3. 필드 의미

| 필드 | 의미 | 사용 목적 |
| --- | --- | --- |
| `durability.current` | 현재 내구도 | 정비 필요 여부 판단 |
| `durability.max` | 최대 내구도 | 내구도 비율 계산 |
| `durability.ratio` | 현재/최대 비율 | threshold 기반 판단 |
| `condition` | 정상/손상/고장 등 상태 | 플레이어용 설명 |
| `maintenance_required` | 정비 필요 여부 | 서브퀘스트 생성 기준 |

## 4. 분석 기준

```text
maintenance_required == true
-> 정비 필요 설비로 판단한다.

durability.ratio <= 0.3
-> 심각한 정비 필요로 판단한다.

condition == "broken"
-> 고장 설비로 판단한다.
```

단, 내구도 정보가 없으면 임의로 고장이라고 추측하지 않는다.

## 5. 응답 방향

```json
{
  "id": "inspect_machine_condition_smelter_1",
  "target": {
    "type": "machine",
    "id": "smelter_1"
  },
  "problem": "smelter_1의 내구도가 낮아 정비가 필요합니다.",
  "recommended_action": "smelter_1을 점검하고 필요한 수리 자원을 사용하십시오.",
  "expected_effect": "정비 후 생산 중단 가능성이 줄어듭니다.",
  "risk": "low"
}
```

## 6. 완료 기준

```text
- machine durability schema가 validation을 통과한다.
- 내구도 낮은 설비를 분석 결과에 포함한다.
- condition과 maintenance_required를 플레이어용 설명에 반영한다.
- 정비 필요 설비를 ui_hints.highlight_targets에 포함한다.
- 자동 수리 명령을 만들지 않는다.
```

## 7. 테스트 시나리오

```text
1. durability.ratio가 0.2인 설비
-> 정비 필요 제안이 생성된다.

2. maintenance_required=true인 설비
-> 정비 서브퀘스트 후보가 생성된다.

3. condition="broken"인 설비
-> 고장 설비로 설명된다.

4. durability 정보가 없는 설비
-> 기존 분석 흐름이 유지된다.
```

## 8. 이번 Sprint에 포함하지 않는 것

```text
- 자동 수리 실행
- 수리 자원 소모 계산
- 부품 교체 시뮬레이션
- 장기 고장 확률 예측
```

