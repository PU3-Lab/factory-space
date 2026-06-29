# Process Optimizer Subquest Sprint 3: Unreal Quest Integration

## 1. 목적

Sprint 1의 `optimization_alert`와 Sprint 2의 target analyze를 Unreal의 서브퀘스트 UX와 연결한다.

이 Sprint의 목표는 “AI가 자동으로 공장을 고치는 것”이 아니라, 주기 상태에서 발견한 문제를 플레이어가 선택할 수 있는 서브퀘스트 후보로 보여 주고, 플레이어가 선택하면 `process_optimizer` 분석 preview로 이어지게 만드는 것이다.

## 2. 최종 흐름

```text
Unreal이 주기적으로 state_update 전송
-> backend가 optimization_alert 반환
-> Unreal이 suggested_subquest를 UI에 후보로 표시
-> 플레이어가 서브퀘스트 후보 클릭
-> Unreal이 next_request 기반 analyze 요청 전송
-> backend가 preview 반환
-> 플레이어가 검토 후 승인
-> apply 요청 후 execute_ready commands 반환
-> Unreal이 최종 월드 검증 후 실행
```

## 3. 구현 범위

### 3.1 Unreal 표시 계약 확정

`optimization_alert.suggested_subquest`의 필드를 확정한다.

권장 필드:

```text
title
objective
target
severity
next_request
```

응답 예시:

```json
{
  "optimization_alert": {
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
      "target": {
        "type": "machine",
        "id": "smelter_1"
      },
      "severity": "medium",
      "next_request": {
        "agent": "process_optimizer",
        "operation": "analyze",
        "goal": "balance",
        "request_source": "subquest",
        "target": {
          "type": "machine",
          "id": "smelter_1"
        }
      }
    }
  }
}
```

### 3.2 서브퀘스트 클릭 시 analyze 요청

Unreal은 `next_request`를 그대로 신뢰해서 실행하지 않고, 최신 상태를 다시 붙여 `analyze` 요청을 보낸다.

```text
suggested_subquest.next_request
+ 최신 factoryRevision
+ 최신 factory_state
-> process_optimizer analyze
```

권장 요청:

```json
{
  "type": "agent.request",
  "request_id": "optimizer-subquest-analyze-001",
  "session_id": "player-session-001",
  "client_id": "unreal-client",
  "agent": "process_optimizer",
  "payload": {
    "operation": "analyze",
    "goal": "balance",
    "request_source": "subquest",
    "target": {
      "type": "machine",
      "id": "smelter_1"
    },
    "factoryRevision": 13,
    "factory_state": {}
  },
  "context": {
    "language": "ko",
    "mode": "gameplay"
  }
}
```

### 3.3 Quest Generator 연동 여부 결정

두 가지 방식 중 하나를 선택한다.

#### Option A: process_optimizer가 서브퀘스트 문구까지 직접 반환

```text
state_update
-> optimization_alert.suggested_subquest
-> Unreal UI 표시
```

장점:

```text
- 구현이 단순하다.
- agent-test에서 바로 검증 가능하다.
- process_optimizer 분석 근거와 서브퀘스트가 직접 연결된다.
```

단점:

```text
- quest_generator의 퀘스트 문체/보상/분류 로직을 재사용하지 않는다.
```

#### Option B: process_optimizer alert를 quest_generator에 넘김

```text
state_update
-> optimization_alert
-> quest_generator production_quest 요청
-> 서브퀘스트 문구 생성
```

장점:

```text
- 퀘스트 문체와 형식이 통일된다.
- 장기적으로 보상/난이도/퀘스트 타입 관리가 쉽다.
```

단점:

```text
- Agent 간 연동 계약이 추가된다.
- 테스트 범위가 커진다.
```

권장: Sprint 3에서는 Option A로 시작하고, 이후 별도 Sprint에서 Option B를 확장한다.

### 3.4 문서와 예시 업데이트

업데이트 대상:

```text
backend/docs/unreal_agent_json_examples.md
backend/docs/agent_test_operator_process_examples.md
docs/process_optimizer/unreal_websocket_contract.md
```

추가할 예시:

```text
- state_update에서 optimization_alert 반환
- suggested_subquest 클릭 후 analyze 요청
- analyze preview 승인 후 apply 요청
```

## 4. 제외 범위

이번 Sprint에 포함하지 않는다.

```text
- 자동 승인 모드
- 자동 apply
- 보상 지급 로직
- 장기 퀘스트 체인 생성
- 플레이어별 최적화 성향 학습
```

## 5. 테스트 계획

백엔드 smoke test:

```text
1. state_update 입력 부족 상태 전송
2. optimization_alert.needed=true 확인
3. suggested_subquest.next_request 기반 analyze 전송
4. preview 응답 확인
5. approval=true apply 전송
6. execute_ready commands 확인
```

Unreal 연동 확인:

```text
1. 주기 상태 업데이트에서 alert 수신
2. 서브퀘스트 후보 UI 표시
3. 후보 클릭 시 analyze 요청 전송
4. preview 카드 표시
5. 월드 highlight 표시
6. 승인 후 commands 최종 검증 및 실행
```

## 6. 완료 기준

```text
- state_update alert가 Unreal UI에서 서브퀘스트 후보로 표시된다.
- 플레이어가 후보를 클릭하면 최신 factory_state로 analyze 요청이 전송된다.
- analyze preview와 apply 승인 흐름이 기존 안전 원칙을 유지한다.
- state_update 자체는 command를 생성하지 않는다.
- 문서와 agent-test 예시가 최신 계약을 반영한다.
```

## 7. 발표용 설명

```text
Process Optimizer는 주기적으로 받은 공장 상태에서 병목 징후를 감지하더라도 자동으로 공장을 수정하지 않습니다.
대신 최적화가 필요한 상황을 서브퀘스트 후보로 만들고, 플레이어가 그 후보를 선택했을 때만 상세 분석 preview를 제공합니다.
이후 실제 변경은 플레이어 승인과 Unreal의 최종 월드 검증을 모두 통과한 경우에만 실행됩니다.
```
