# Process Optimizer Subquest Sprint 1: State Update Alert

## 1. 목적

주기적으로 들어오는 `state_update`를 단순 저장에서 한 단계 확장해, 공장에 최적화가 필요한 징후가 있는지 감지한다.

이 Sprint의 목표는 서브퀘스트를 실제로 등록하는 것이 아니다. 백엔드가 `factory_state`를 보고 “최적화 분석을 제안할 만한 문제”를 `optimization_alert` 형태로 반환할 수 있게 만드는 것이다.

## 2. 현재 상태

현재 `state_update` 흐름은 다음과 같다.

```text
Unreal state_update
-> validate_process_optimizer_payload
-> build_state_update_response
-> process_optimizer_memory.update
-> status: success 반환
```

현재 구현은 공장 상태 저장까지만 수행한다.

## 3. 구현 범위

### 3.1 Payload schema 보강

`ProcessOptimizerPayload`에 선택 필드를 추가한다.

```text
request_source
target
subquest_mode
```

권장 의미:

| 필드 | 의미 |
| --- | --- |
| `request_source` | Unreal에서 어떤 UI 또는 이벤트로 요청했는지 표시 |
| `target` | 특정 기계/컨베이어 중심 요청일 때 대상 표시 |
| `subquest_mode` | 주기 상태에서 서브퀘스트 후보 생성을 허용하는지 표시 |

### 3.2 Alert 생성 모델 추가

`state_update` 응답에 들어갈 alert 구조를 코드 모델로 정의한다.

권장 구조:

```json
{
  "needed": true,
  "severity": "medium",
  "reason": "smelter_1의 입력 재고가 부족합니다.",
  "target": {
    "type": "machine",
    "id": "smelter_1"
  },
  "suggested_subquest": {
    "title": "제련기 입력 라인 복구",
    "objective": "smelter_1에 철광석 공급이 다시 들어오도록 컨베이어와 상류 설비를 확인하세요.",
    "next_request": {
      "agent": "process_optimizer",
      "operation": "analyze"
    }
  }
}
```

### 3.3 Alert 생성 로직 추가

새 파일 후보:

```text
backend/src/agents/process_optimizer/subquest_alert.py
```

역할:

```text
FactoryAnalysisReport
-> 가장 중요한 문제 선택
-> severity 계산
-> player-facing reason 생성
-> suggested_subquest 생성
```

초기 감지 기준:

| 조건 | severity | reason 예시 |
| --- | --- | --- |
| 전력 소비량이 생산량 초과 | high | 공장 전력 공급이 부족합니다. |
| 입력 부족 장비 존재 | medium | 특정 장비의 입력 재고가 부족합니다. |
| 출력 적체 장비 존재 | medium | 특정 장비의 출력 공간이 가득 찼습니다. |
| 컨베이어 혼잡 0.8 이상 | low 또는 medium | 컨베이어 병목이 감지되었습니다. |

### 3.4 `build_state_update_response` 확장

현재 `middleware.py`의 `build_state_update_response`가 상태 저장만 한다면, Sprint 1에서는 아래 흐름으로 확장한다.

```text
factory_state 저장
factoryRevision 저장
FactoryStateAnalyzerTool.analyze 실행
SubquestAlertBuilder로 optimization_alert 생성
status: success 응답에 alert 선택 포함
```

중요: 이 단계에서는 `commands`, `plan_id`, `changes`를 만들지 않는다.

## 4. 제외 범위

이번 Sprint에 포함하지 않는다.

```text
- Unreal 서브퀘스트 UI 등록
- quest_generator 직접 호출
- 특정 기계 target 우선 분석
- apply/undo/measure 변경
- 실제 공장 변경 command 생성
```

## 5. 테스트 계획

추가 테스트 파일 후보:

```text
backend/tests/test_process_optimizer_state_update_alert.py
```

테스트 케이스:

| 테스트 | 기대 결과 |
| --- | --- |
| 정상 공장 상태 | `optimization_alert.needed=false` 또는 alert 없음 |
| 입력 부족 장비 포함 | medium alert 반환 |
| 출력 적체 장비 포함 | medium alert 반환 |
| 전력 부족 상태 | high alert 반환 |
| 컨베이어 혼잡 상태 | conveyor target alert 반환 |
| `state_update` 응답 | `commands`가 없어야 함 |

## 6. 완료 기준

```text
- state_update에서 최신 상태 저장이 기존처럼 동작한다.
- 문제 있는 factory_state에서 optimization_alert가 반환된다.
- 문제 없는 factory_state에서는 불필요한 alert가 생성되지 않는다.
- state_update는 여전히 공장 변경 command를 만들지 않는다.
- 기존 analyze/apply/undo/measure 테스트가 깨지지 않는다.
```

## 7. Unreal 공유 포인트

Sprint 1 완료 후 Unreal에 공유할 내용:

```text
- state_update 응답에 optimization_alert가 선택적으로 추가된다.
- optimization_alert는 자동 실행 명령이 아니다.
- Unreal은 이 alert를 서브퀘스트 후보나 UI 알림으로 표시할 수 있다.
- 플레이어가 alert를 클릭하면 별도 analyze 요청을 보내야 한다.
```
