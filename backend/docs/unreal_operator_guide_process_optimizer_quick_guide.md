# Unreal용 Operator Guide / Process Optimizer 빠른 연동 가이드

이 문서는 Unreal 개발자가 `operator_guide`와 `process_optimizer`의 역할을
빠르게 이해하고, `/ws/agent` WebSocket에 요청 JSON을 보낼 수 있도록 정리한
실전 연동 가이드다.

## 1. 두 에이전트 한눈에 보기

| Agent | 한 줄 설명 | Unreal에서 호출하는 시점 |
| --- | --- | --- |
| `operator_guide` | 플레이어의 설비, 레시피, 문제 해결 질문에 답한다. | NPC 대화창이나 도움말 UI에서 질문할 때 |
| `process_optimizer` | 현재 공장 상태를 분석하고 플레이어가 검토할 최적화 제안을 만든다. | 주기 상태 전송, 최적화 버튼, 서브퀘스트, 승인/측정/되돌리기 시 |

역할은 다음과 같이 나눈다.

```text
Unreal
- NPC 대화창, 버튼, 기계 상호작용 UI 제공
- 최신 공장 상태와 factoryRevision 수집
- 백엔드 응답 표시
- 실행 명령을 실제 월드 규칙으로 최종 검증하고 실행

백엔드 Agent
- operator_guide 질문 분석과 답변 생성
- process_optimizer 병목 분석과 제안 생성
- 승인, revision, 명령 schema, Undo 충돌 검증
- Unreal이 처리할 구조화된 JSON 응답 반환
```

`process_optimizer`는 공장을 임의로 변경하지 않는다. 백엔드는 제안과 검증된
명령 후보를 반환하고, 실제 실행 여부는 플레이어 승인과 Unreal의 최종 검증으로
결정한다.

## 2. 공통 연결 정보

```text
개발 PC: ws://127.0.0.1:18000/ws/agent
다른 PC에서 접속: ws://<백엔드-PC-IP>:18000/ws/agent
요청 type: agent.request
성공 응답 type: agent.response
오류 응답 type: agent.error
```

모든 요청은 다음 공통 구조를 사용한다.

```json
{
  "type": "agent.request",
  "request_id": "요청마다-새로운-ID",
  "session_id": "플레이어-세션-ID",
  "client_id": "unreal-client",
  "agent": "operator_guide 또는 process_optimizer",
  "payload": {},
  "context": {
    "language": "ko",
    "mode": "gameplay"
  }
}
```

연동할 때 지켜야 할 기본 규칙:

1. `request_id`는 요청마다 새로 만든다.
2. 같은 플레이어 흐름에서는 같은 `session_id`를 사용한다.
3. 서버가 보낸 `agent.progress`는 진행 문구로 표시할 수 있다.
4. 최종 결과는 `agent.response.payload`에서 읽는다.
5. `agent.error`가 오면 공장 명령을 실행하지 않는다.

## 3. Operator Guide 연동

### 3.1 Unreal 팀에 설명하는 문장

> Operator Guide는 플레이어가 설비, 레시피, 생산 문제를 질문하면 게임 데이터와
> 매뉴얼 근거를 이용해 한국어 답변을 반환하는 도움말 에이전트입니다. Unreal은
> 질문을 보내고 `payload.final_answer`를 NPC 대화창에 표시하면 됩니다.

### 3.2 자유 질문

NPC 대화창에서 플레이어가 자유롭게 질문하는 경우다. `sub_agent`를 생략하면
백엔드가 질문에 맞는 하위 에이전트를 선택한다.

```json
{
  "type": "agent.request",
  "request_id": "guide-free-001",
  "session_id": "player-session-001",
  "client_id": "unreal-client",
  "agent": "operator_guide",
  "payload": {
    "question": "분쇄기가 뭐야? 어디에 써?"
  },
  "context": {
    "language": "ko",
    "mode": "gameplay"
  }
}
```

Unreal에서 주로 읽을 응답 값:

```text
payload.final_answer                -> NPC 대화창 본문
payload.actions                     -> 선택 가능한 후속 행동
payload.metadata.sources            -> 답변 근거 문서
payload.metadata.selectedLeafAgent  -> 실제 선택된 하위 에이전트
```

### 3.3 설비 도움말 탭에서 질문

UI에서 질문 종류가 이미 정해져 있다면 `sub_agent`를 직접 지정한다.

```text
operator_guide.machine_help       설비 설명
operator_guide.recipe_explainer  레시피 설명
operator_guide.troubleshooter    생산 문제 해결
```

선택한 기계의 상태가 필요하면 `context.current_game_state`도 함께 보낸다.

```json
{
  "type": "agent.request",
  "request_id": "guide-machine-001",
  "session_id": "player-session-001",
  "client_id": "unreal-client",
  "agent": "operator_guide",
  "payload": {
    "sub_agent": "operator_guide.machine_help",
    "question": "이 제련기는 왜 멈춰 있어?"
  },
  "context": {
    "language": "ko",
    "mode": "gameplay",
    "current_game_state": {
      "selectedMachine": {
        "id": "smelter_1",
        "type": "smelter",
        "status": "idle",
        "recipe_id": "iron_ingot"
      }
    }
  }
}
```

### 3.4 제작 가능한 설치물(Double Presence) 안내

게임 데이터 구조상, 장비이자 생산품인 대상(예: **통신탑**)은 `equipment.csv`와 `resources.csv` 테이블 모두에 중복 존재할 수 있습니다.

백엔드 하이브리드 의도 분류기는 이를 다음과 같이 처리합니다:
1. **명확한 의도**: "통신탑 어떻게 지어?", "건설 재료 알려줘" 등 제작 지시 키워드가 있으면 자동으로 자원/레시피로 분류합니다.
2. **모호한 의도**: "통신탑 알려줘" 처럼 질문만으로 장비 설명인지 제작법인지 구분할 수 없을 때는 응답 메타데이터에 `isAmbiguous: true`를 마크하며, 백엔드 LLM이 문맥을 분석하여 최종 의도를 지능적으로 보정합니다.
3. **Unreal 연동 시 참고**: 응답 메타데이터의 `isAmbiguous` 필드가 `true`인 경우, 클라이언트 UI 단에서 추가 선택지(예: '장비 역할 보기' / '제작법 보기')를 유도하거나 답변 흐름의 보조 힌트로 활용할 수 있습니다.

## 4. Process Optimizer 연동

### 4.1 Unreal 팀에 설명하는 문장

> Process Optimizer는 Unreal이 보낸 공장 snapshot을 분석해 입력 부족, 출력 적체,
> 컨베이어 혼잡, 전력 문제를 찾고 최적화 제안을 반환합니다. 주기 상태에서
> 서브퀘스트 후보를 만들거나 플레이어의 최적화 요청을 처리할 수 있으며, 실제
> 변경은 플레이어 승인 후 Unreal이 최종 검증하여 실행합니다.

전체 흐름은 다음과 같다.

```text
주기 상태 전송(state_update)
-> 필요하면 optimization_alert와 서브퀘스트 후보 수신
-> 플레이어가 최적화 버튼 또는 서브퀘스트 수락
-> 분석 요청(analyze)
-> preview 표시
-> 플레이어가 변경 항목 승인
-> 적용 요청(apply)
-> Unreal이 commands를 최종 검증하고 실행
-> 성과 측정(measure) 또는 플레이어 요청 Undo(undo)
```

### 4.2 공장 상태 snapshot 기본 형태

```json
{
  "machines": [
    {
      "id": "smelter_1",
      "type": "smelter",
      "status": "operating",
      "operating_rate": 0.2,
      "inputs": [
        {
          "item_id": "iron_ore",
          "amount": 0,
          "max_amount": 100
        }
      ],
      "outputs": [],
      "power_consumption": 15
    }
  ],
  "conveyors": [],
  "power_grid": {
    "produced": 120,
    "consumed": 90
  }
}
```

`factoryRevision`은 Unreal이 관리한다. 공장 상태가 바뀌면 값을 증가시키고,
최신 snapshot과 함께 전송한다.

### 4.3 주기 상태 업데이트와 서브퀘스트 후보

Unreal이 일정 주기 또는 중요한 상태 변경 시 보낸다. `subquest_mode: true`이면
최적화가 필요한 경우 응답에 서브퀘스트 후보가 포함될 수 있다.

```json
{
  "type": "agent.request",
  "request_id": "optimizer-state-001",
  "session_id": "player-session-001",
  "client_id": "unreal-client",
  "agent": "process_optimizer",
  "payload": {
    "operation": "state_update",
    "goal": "balance",
    "factoryRevision": 42,
    "subquest_mode": true,
    "factory_state": {
      "machines": [
        {
          "id": "smelter_1",
          "type": "smelter",
          "status": "operating",
          "operating_rate": 0.2,
          "inputs": [
            {
              "item_id": "iron_ore",
              "amount": 0,
              "max_amount": 100
            }
          ],
          "outputs": []
        }
      ],
      "conveyors": [],
      "power_grid": {
        "produced": 120,
        "consumed": 90
      }
    }
  },
  "context": {
    "language": "ko",
    "mode": "gameplay"
  }
}
```

응답 처리:

```text
payload.optimization_alert.needed
  false -> 별도 UI를 만들지 않음
  true  -> 경고 또는 서브퀘스트 후보 표시

payload.optimization_alert.suggested_subquest
  title        -> 서브퀘스트 제목
  objective    -> 목표 문구
  target       -> 대상 기계/컨베이어
  next_request -> 플레이어 수락 시 만들 analyze 요청의 기준
```

`state_update`는 결정론적인 상태 판정이므로 LLM 모델명이 표시되지 않아도 정상이다.

### 4.4 최적화 버튼 또는 특정 기계 상호작용

플레이어가 최적화 버튼을 누르면 최신 snapshot으로 `analyze`를 보낸다. 특정 기계
버튼에서 요청했다면 `target`을 포함한다.

```json
{
  "type": "agent.request",
  "request_id": "optimizer-analyze-001",
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
    "factoryRevision": 43,
    "factory_state": {
      "machines": [
        {
          "id": "smelter_1",
          "type": "smelter",
          "status": "operating",
          "operating_rate": 0.2,
          "inputs": [
            {
              "item_id": "iron_ore",
              "amount": 0,
              "max_amount": 100
            }
          ],
          "outputs": [],
          "power_consumption": 15
        }
      ],
      "conveyors": [],
      "power_grid": {
        "produced": 120,
        "consumed": 90
      }
    }
  },
  "context": {
    "language": "ko",
    "mode": "gameplay"
  }
}
```

서브퀘스트를 수락해서 분석하는 경우에는 다음 값만 달라진다.

```json
{
  "operation": "analyze",
  "goal": "balance",
  "request_source": "subquest",
  "target": {
    "type": "machine",
    "id": "smelter_1"
  }
}
```

실제 요청에는 위 값과 함께 최신 `factoryRevision`, `factory_state`를 넣는다.

Unreal에서 preview 응답의 주요 필드를 다음처럼 사용한다.

```text
payload.status = "preview"       -> 최적화 미리보기 UI 열기
payload.plan_id                 -> apply/measure/undo에 다시 사용할 ID
payload.summary                 -> 계획 요약
payload.changes                 -> 플레이어가 선택할 변경 카드
payload.expected_effect         -> 예상 효과
payload.ui_hints.highlight_targets -> 월드 하이라이트 대상 ID
payload.expires_at              -> 계획 만료 시각
```

### 4.5 플레이어 승인 후 적용 요청

`analyze` 응답의 `plan_id`와 선택한 `changes[].id`를 사용한다. 승인하지 않았다면
이 요청을 보내지 않는다.

```json
{
  "type": "agent.request",
  "request_id": "optimizer-apply-001",
  "session_id": "player-session-001",
  "client_id": "unreal-client",
  "agent": "process_optimizer",
  "payload": {
    "operation": "apply",
    "plan_id": "ANALYZE_RESPONSE_PLAN_ID",
    "factoryRevision": 43,
    "approval": true,
    "approved_change_ids": [
      "ANALYZE_RESPONSE_CHANGE_ID"
    ],
    "factory_state": {
      "machines": [
        {
          "id": "smelter_1",
          "type": "smelter",
          "status": "operating",
          "operating_rate": 0.2,
          "inputs": [
            {
              "item_id": "iron_ore",
              "amount": 0,
              "max_amount": 100
            }
          ],
          "outputs": []
        }
      ],
      "conveyors": [],
      "power_grid": {
        "produced": 120,
        "consumed": 90
      }
    },
    "before_states": {
      "ANALYZE_RESPONSE_CHANGE_ID": {
        "target": {
          "type": "machine",
          "id": "smelter_1"
        },
        "status": "operating",
        "operating_rate": 0.2
      }
    }
  },
  "context": {
    "language": "ko",
    "mode": "gameplay"
  }
}
```

응답의 `payload.status`가 `execute_ready`일 때만 `payload.commands`를 검토한다.
Unreal은 각 명령을 실제 월드 상태, 자원, 위치, 연결 가능 여부, 전력 한도로 다시
검증한 뒤 실행한다.

`approval: true`가 없으면 백엔드는 `approval_required`로 차단한다.

### 4.6 적용 후 성과 측정

최소 30초와 최소 3회 생산 사이클이 모두 지난 뒤 최신 상태를 보낸다.

```json
{
  "type": "agent.request",
  "request_id": "optimizer-measure-001",
  "session_id": "player-session-001",
  "client_id": "unreal-client",
  "agent": "process_optimizer",
  "payload": {
    "operation": "measure",
    "plan_id": "ANALYZE_RESPONSE_PLAN_ID",
    "production_cycles": 5,
    "current_time": "2030-01-01T00:00:00Z",
    "factory_state": {
      "machines": [
        {
          "id": "smelter_1",
          "type": "smelter",
          "status": "operating",
          "operating_rate": 1.0,
          "inputs": [
            {
              "item_id": "iron_ore",
              "amount": 10,
              "max_amount": 100
            }
          ],
          "outputs": []
        }
      ],
      "conveyors": [],
      "power_grid": {
        "produced": 120,
        "consumed": 95
      }
    }
  },
  "context": {
    "language": "ko",
    "mode": "gameplay"
  }
}
```

결과는 `payload.measurement_result`에서 확인한다. 측정 결과가 나빠도 백엔드는
자동으로 Undo하지 않고 `next_action: "reanalyze"`를 제안한다.

### 4.7 플레이어 요청 Undo

플레이어가 직접 되돌리기를 요청할 때만 최신 공장 상태와 함께 보낸다.

```json
{
  "type": "agent.request",
  "request_id": "optimizer-undo-001",
  "session_id": "player-session-001",
  "client_id": "unreal-client",
  "agent": "process_optimizer",
  "payload": {
    "operation": "undo",
    "plan_id": "ANALYZE_RESPONSE_PLAN_ID",
    "factoryRevision": 44,
    "factory_state": {
      "machines": [
        {
          "id": "smelter_1",
          "type": "smelter",
          "status": "operating",
          "operating_rate": 1.0,
          "inputs": [
            {
              "item_id": "iron_ore",
              "amount": 10,
              "max_amount": 100
            }
          ],
          "outputs": []
        }
      ],
      "conveyors": [],
      "power_grid": {
        "produced": 120,
        "consumed": 95
      }
    }
  },
  "context": {
    "language": "ko",
    "mode": "gameplay"
  }
}
```

현재 상태가 기록된 적용 후 상태와 다르면 `undo_conflict`가 반환된다. 이때
Unreal은 명령을 실행하지 않고 플레이어에게 재분석을 안내한다.

## 5. 현재 구현에서 꼭 알아둘 점

- `execute_ready`는 실행 완료가 아니라 Unreal이 실행할 명령의 준비 완료 상태다.
- 현재 공개 operation에는 Unreal 명령 실행 결과만 별도로 확정하는 요청이 없다.
- `before_states`를 보내면 Measure가 적용 전 상태를 더 정확하게 복원할 수 있다.
- 안전한 Undo에는 실행 후 실제 상태를 기준으로 만든 신뢰 가능한 `after_states`가
  필요하다. 적용 후 상태가 확정되지 않았다면 백엔드는 안전을 위해 Undo를
  `undo_conflict`로 차단할 수 있다.
- 실제 Unreal 연동 단계에서는 명령 실행 결과와 적용 후 snapshot을 언제
  백엔드에 확정할지 추가 협의가 필요하다.

## 6. Unreal 구현 체크리스트

### Operator Guide

- 질문 입력 UI에서 `operator_guide` 요청을 전송한다.
- `agent.progress`를 로딩 문구로 표시한다.
- `payload.final_answer`를 대화창에 표시한다.
- 고정된 도움말 탭에서는 올바른 `sub_agent`를 넣는다.

### Process Optimizer

- 공장 상태 변경 또는 일정 주기에 `state_update`를 보낸다.
- `optimization_alert.needed`가 참이면 서브퀘스트 후보를 표시한다.
- 최적화 버튼 또는 기계 상호작용에서 최신 snapshot으로 `analyze`를 보낸다.
- preview의 `plan_id`, `changes[].id`, `factoryRevision`을 보관한다.
- 플레이어가 승인한 경우에만 `apply`를 보낸다.
- `commands`는 Unreal 월드 규칙으로 다시 검증한다.
- 최소 30초와 3 생산 사이클 이후 `measure`를 보낸다.
- Undo는 플레이어가 요청한 경우에만 보낸다.
- revision 충돌, 계획 만료, 승인 누락, Undo 충돌 시 명령을 실행하지 않는다.

## 7. 상세 문서

- [공통 agent.request 계약](agent_request_contract.md)
- [전체 Unreal JSON 예시](unreal_agent_json_examples.md)
- [Process Optimizer WebSocket 상세 계약](../../docs/process_optimizer/unreal_websocket_contract.md)
- [Process Optimizer 서브퀘스트 연동 기획](../../docs/process_optimizer/process_optimizer_subquest_integration_plan.md)
- [Operator Guide Unreal 질문 UI 계약](../../docs/operator_guide/2026-06-11_operator_guide_unreal_question_guide_ui_contract.md)
