# Process Optimizer 서브퀘스트 연동 기획안

## 1. 배경

Unreal 쪽 요구사항은 `process_optimizer`를 단순한 자동 실행 버튼으로 만들지 않는 것이다.
플레이어가 직접 요청하면 현재 공장 상태를 분석해 최적화 제안을 보여 주고, 플레이어가 검토한 뒤 승인해야 실제 실행 명령을 만든다.

동시에 Unreal은 공장 상태를 주기적으로 백엔드에 전달할 수 있다. 이 주기 상태 업데이트를 활용해 AI가 병목이나 비효율을 감지하면, 해당 내용을 바로 공장에 적용하지 않고 서브퀘스트 후보로 넘기는 흐름이 필요하다.

## 2. 최종 목표

`process_optimizer`는 두 가지 진입 경로를 가진다.

```text
1. 플레이어 요청형 최적화
   특정 기계 상호작용, 공장 UI 버튼, 대화창 요청 등을 통해 플레이어가 최적화 분석을 요청한다.

2. 주기 상태 기반 서브퀘스트 후보
   Unreal이 주기적으로 보내는 factory_state를 보고 백엔드가 최적화 필요 여부를 감지한다.
   감지된 문제는 공장 변경이 아니라 서브퀘스트 후보나 UI 알림으로 전달한다.
```

이 구조의 핵심은 자동 실행이 아니라 “감지 → 제안 → 플레이어 선택 → 승인 후 실행”이다.

## 3. 현재 구현 상태

현재 백엔드 기준으로 이미 가능한 흐름은 다음과 같다.

```text
Unreal /ws/agent
-> agent: "process_optimizer"
-> operation: "analyze"
-> v2 LangGraph 분석
-> preview 계획 반환
-> 플레이어 승인 시 operation: "apply"
-> execute_ready commands 반환
```

현재 구현된 주요 기능:

| 항목 | 현재 상태 |
| --- | --- |
| 플레이어 요청 기반 `analyze` | 구현됨 |
| preview 계획 생성 | 구현됨 |
| 승인 없는 `apply` 차단 | 구현됨 |
| 승인 후 command payload 생성 | 구현됨 |
| `factoryRevision` 충돌 검증 | 구현됨 |
| 실행 기록 기반 undo 준비 | 구현됨 |
| 효과 측정 `measure` | 구현됨 |
| 주기 `state_update` 수신 및 저장 | 구현됨 |
| `state_update` 기반 최적화 필요 감지 | 추가 구현 필요 |
| 감지 결과를 서브퀘스트 후보로 반환 | 추가 구현 필요 |
| `quest_generator`와 직접 연동 | 추가 구현 필요 |

현재 `state_update`는 최신 공장 상태를 session memory에 저장하는 역할까지 수행한다. 아직 이 단계에서 병목을 감지해 `subquest_candidate`를 반환하지는 않는다.

## 4. 권장 UX 흐름

### 4.1 특정 기계 상호작용 기반 최적화 요청

플레이어가 특정 기계에 다가가거나 기계 UI를 열었을 때 “최적화 분석” 버튼을 제공한다.

```text
플레이어가 smelter_1과 상호작용
-> Unreal이 해당 기계 중심의 최신 factory_state 수집
-> process_optimizer analyze 요청
-> 백엔드가 smelter_1 관련 문제를 우선 분석
-> preview 계획 반환
-> Unreal이 관련 기계/컨베이어 highlight
-> 플레이어가 승인하면 apply 요청
```

이 방식은 기본 HUD의 AI 캐릭터를 직접 클릭하기 어렵다는 Unreal 제약과도 잘 맞는다.

### 4.2 공장 UI 버튼 기반 최적화 요청

공장 관리 UI나 생산 라인 UI에 “공장 최적화 분석” 버튼을 제공한다.

```text
플레이어가 공장 UI에서 최적화 분석 버튼 클릭
-> Unreal이 전체 또는 주요 라인 factory_state 전송
-> process_optimizer analyze 요청
-> 전체 공장 기준 최대 3개 개선안 preview 반환
```

이 방식은 특정 기계가 아니라 전체 공장 상태를 보고 싶을 때 적합하다.

### 4.3 주기 상태 기반 서브퀘스트 후보

Unreal이 주기적으로 `state_update`를 보내면 백엔드는 상태를 저장하고, 가벼운 규칙 기반 분석을 통해 최적화 필요 여부를 감지한다.

```text
Unreal이 주기적으로 state_update 전송
-> 백엔드가 최신 factory_state 저장
-> 입력 부족, 출력 적체, 전력 부족, 컨베이어 혼잡 감지
-> 문제가 기준치를 넘으면 optimization_alert 생성
-> Unreal이 alert를 서브퀘스트 후보로 표시
-> 플레이어가 서브퀘스트를 클릭하면 analyze 요청
```

이때 `state_update`는 실제 공장 변경 명령을 만들지 않는다.

## 5. 요청/응답 계약 초안

### 5.1 플레이어가 특정 기계에서 최적화 요청

현재 `ProcessOptimizerPayload`는 `extra="allow"`를 사용하므로 아래 필드를 받아도 즉시 검증 오류가 나지는 않는다. 다만 실제 코드에서 의미 있게 사용하려면 `request_source`, `target` 또는 `target_machine_id`를 명시 필드로 추가하는 것이 좋다.

```json
{
  "type": "agent.request",
  "request_id": "optimizer-machine-analyze-001",
  "session_id": "player-session-001",
  "client_id": "unreal-client",
  "agent": "process_optimizer",
  "payload": {
    "operation": "analyze",
    "goal": "balance",
    "request_source": "machine_interaction",
    "target": {
      "type": "machine",
      "id": "smelter_1"
    },
    "factoryRevision": 12,
    "factory_state": {
      "machines": [],
      "conveyors": [],
      "power_grid": {
        "produced": 120.0,
        "consumed": 90.0
      }
    }
  },
  "context": {
    "language": "ko",
    "mode": "gameplay"
  }
}
```

응답은 기존 `preview` 형식을 유지한다.

```json
{
  "type": "agent.response",
  "request_id": "optimizer-machine-analyze-001",
  "session_id": "player-session-001",
  "client_id": "unreal-client",
  "agent": "process_optimizer",
  "payload": {
    "status": "preview",
    "plan_id": "plan-example-001",
    "factoryRevision": 12,
    "goal": "balance",
    "summary": "smelter_1의 입력 부족을 먼저 해결하는 최적화 제안입니다.",
    "changes": [],
    "ui_hints": {
      "highlight_targets": ["smelter_1"]
    }
  },
  "streams": []
}
```

### 5.2 주기 상태 업데이트에서 서브퀘스트 후보 반환

추가 구현 후 `state_update` 응답은 기존 `status: "success"`를 유지하면서 선택적으로 `optimization_alert`를 포함한다.

```json
{
  "type": "agent.response",
  "request_id": "optimizer-state-update-001",
  "session_id": "player-session-001",
  "client_id": "unreal-client",
  "agent": "process_optimizer",
  "payload": {
    "status": "success",
    "factoryRevision": 12,
    "goal": "balance",
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
        "next_request": {
          "agent": "process_optimizer",
          "operation": "analyze",
          "target": {
            "type": "machine",
            "id": "smelter_1"
          }
        }
      }
    }
  },
  "streams": []
}
```

문제가 없으면 `optimization_alert`를 생략하거나 아래처럼 반환한다.

```json
{
  "optimization_alert": {
    "needed": false
  }
}
```

## 6. 백엔드 수정 범위

### 6.1 Payload schema 확장

`backend/src/agents/process_optimizer/schemas.py`의 `ProcessOptimizerPayload`에 선택 필드를 추가한다.

권장 필드:

```text
request_source: "factory_ui" | "machine_interaction" | "dialog" | "subquest" | "periodic_state"
target: TargetDescriptor | None
subquest_mode: bool
```

목적:

```text
request_source
-> Unreal에서 어떤 UI/이벤트로 요청했는지 기록

target
-> 특정 기계나 컨베이어 중심 분석 요청을 표현

subquest_mode
-> state_update에서 감지된 문제를 서브퀘스트 후보로 만들지 여부 표현
```

### 6.2 state_update 분석 확장

`backend/src/agents/process_optimizer/middleware.py`의 `build_state_update_response`를 확장한다.

현재:

```text
factory_state 저장
factoryRevision 저장
status: success 반환
```

수정 후:

```text
factory_state 저장
factoryRevision 저장
FactoryStateAnalyzerTool로 경량 분석
심각한 문제 감지 시 optimization_alert 생성
status: success와 함께 반환
```

이 단계에서는 preview plan을 저장하거나 command를 만들지 않는다.

### 6.3 alert 생성 모듈 추가

새 모듈을 만든다면 다음 이름이 적합하다.

```text
backend/src/agents/process_optimizer/subquest_alert.py
```

역할:

```text
FactoryAnalysisReport
-> severity 계산
-> target 선정
-> player-facing reason 생성
-> suggested_subquest 생성
```

초기 기준:

| 감지 조건 | severity | 서브퀘스트 예시 |
| --- | --- | --- |
| 입력 부족 장비 1개 이상 | medium | 입력 라인 복구 |
| 출력 적체 장비 1개 이상 | medium | 출력 저장소 비우기 |
| 컨베이어 혼잡 0.8 이상 | low/medium | 컨베이어 병목 해소 |
| 소비 전력 > 생산 전력 | high | 전력 공급 안정화 |

### 6.4 analyze에서 target 우선순위 반영

특정 기계 상호작용으로 들어온 요청은 전체 분석 결과 중 해당 target과 관련된 suggestion을 우선 보여 주는 것이 자연스럽다.

수정 후보:

```text
OptimizationSuggestionTool.generate_suggestions()
-> optional target을 받아 우선 정렬
```

또는 graph node에서 suggestions 생성 후 target 관련 항목을 앞으로 재정렬한다.

### 6.5 테스트 추가

추가할 테스트:

```text
test_process_optimizer_state_update_alert.py
```

검증 항목:

```text
1. 정상 상태 state_update는 optimization_alert.needed=false 또는 alert 없음
2. 입력 부족 상태는 medium alert와 suggested_subquest 반환
3. 전력 부족 상태는 high alert 반환
4. state_update는 commands를 절대 반환하지 않음
5. target analyze 요청은 target 관련 suggestion을 우선 반환
```

## 7. Unreal 수정 범위

Unreal 쪽에서 필요한 작업은 백엔드 Agent 구조를 바꾸는 일이 아니라, 백엔드 계약에 맞는 입력과 UI 연결을 만드는 것이다.

### 7.1 특정 기계 상호작용 UI

```text
기계 상호작용 메뉴
-> "이 기계 최적화 분석" 버튼
-> analyze 요청 전송
-> 응답 preview 표시
```

필요한 데이터:

```text
target machine id
최신 factoryRevision
관련 machine/conveyor/power snapshot
```

### 7.2 공장 UI 최적화 버튼

```text
공장 관리 UI
-> "공장 최적화 분석" 버튼
-> 전체 factory_state 기반 analyze 요청
```

### 7.3 서브퀘스트 후보 UI

```text
state_update 응답에 optimization_alert.needed=true
-> Unreal이 suggested_subquest를 서브퀘스트 후보로 표시
-> 플레이어가 클릭하면 next_request 기준으로 analyze 요청
```

서브퀘스트는 자동 공장 변경이 아니라 분석 진입점이다.

## 8. 실행 안전 원칙

이 기획에서 반드시 유지해야 하는 원칙은 다음과 같다.

```text
1. state_update는 공장 상태 저장과 알림 후보 생성만 한다.
2. state_update는 commands를 반환하지 않는다.
3. analyze는 preview만 반환하고 공장을 바꾸지 않는다.
4. apply는 approval=true일 때만 commands를 만든다.
5. Unreal은 commands를 실행하기 전에 월드 규칙으로 최종 검증한다.
6. 서브퀘스트 후보는 플레이어가 선택할 수 있는 제안이지 자동 실행이 아니다.
```

## 9. 구현 우선순위

### 1단계: 계약 정리

```text
- request_source, target, optimization_alert 응답 구조 문서화
- Unreal과 필드 이름 확정
```

### 2단계: state_update alert 구현

```text
- FactoryStateAnalyzerTool 재사용
- alert 생성 함수 추가
- commands 미생성 테스트 추가
```

### 3단계: 특정 기계 analyze 개선

```text
- target 관련 suggestion 우선 정렬
- ui_hints.highlight_targets에 target 포함 보장
```

### 4단계: Unreal UI 연결

```text
- 특정 기계 상호작용 버튼
- 공장 UI 최적화 버튼
- 서브퀘스트 후보 표시
```

### 5단계: smoke test

```text
- 주기 state_update -> alert 반환
- alert 클릭 -> analyze preview
- preview 승인 -> apply execute_ready
- Unreal 월드 검증 후 실행
```

## 10. 발표용 설명

```text
Process Optimizer는 플레이어가 요청하면 현재 공장 상태를 분석해 최적화 preview를 제안하고,
플레이어가 승인한 경우에만 실행 가능한 command를 생성합니다.

추가로 Unreal이 주기적으로 보내는 공장 상태를 이용해 입력 부족, 출력 적체, 전력 부족 같은 문제를 감지하고,
이를 자동 실행하지 않고 서브퀘스트 후보로 전달합니다.

따라서 AI가 공장을 마음대로 바꾸는 구조가 아니라,
문제를 발견하고 플레이어에게 선택 가능한 목표를 제안하는 보조 Agent 구조입니다.
```
