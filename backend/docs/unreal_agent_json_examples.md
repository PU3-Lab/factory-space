# Unreal Agent JSON 입출력 예시

이 문서는 Unreal에서 `/ws/agent` WebSocket으로 `operator_guide`와 `process_optimizer`에 어떤 JSON을 보내면 백엔드가 어떤 형태로 응답하는지 정리한다.

```text
WebSocket: ws://127.0.0.1:18000/ws/agent
공통 요청 type: agent.request
공통 응답 type: agent.response 또는 agent.error
```

## 0. Unreal UI와 백엔드 Agent의 역할

백엔드 Agent는 게임 화면을 직접 만들지 않는다. 백엔드는 Unreal이 보낸 JSON 요청을 처리하고, Unreal이 UI에 표시하거나 월드에서 검증할 수 있는 JSON 응답을 돌려주는 역할이다.

```text
Unreal NPC / 버튼 / 기계 상호작용 UI
-> 정해진 JSON으로 백엔드 Agent 호출
-> 백엔드가 질문 답변, 최적화 preview, 실행 명령 후보 반환
-> Unreal이 응답을 UI에 표시하거나 월드 규칙으로 최종 검증
```

따라서 NPC 대화창, HUD 버튼, 특정 기계 상호작용 버튼 중 어떤 입구를 만들지는 Unreal UI 설계에 가깝다. 백엔드 입장에서는 아래처럼 `agent`, `payload`, `context`가 맞으면 같은 Agent 요청으로 처리한다.

### 0.1 백엔드 수정이 필요 없는 경우

기존 JSON 계약 안에서 호출 방식만 달라지는 경우다.

```text
NPC 대화창에서 질문
-> agent: "operator_guide"
-> payload.question

설비 도움말 탭에서 질문
-> agent: "operator_guide"
-> payload.sub_agent: "operator_guide.machine_help"
-> payload.question

공장 최적화 버튼 클릭
-> agent: "process_optimizer"
-> payload.operation: "analyze"
-> payload.factory_state

최적화 제안 승인 버튼 클릭
-> agent: "process_optimizer"
-> payload.operation: "apply"
-> payload.approval: true
```

이 경우 Unreal이 화면과 버튼을 만들고, 이 문서의 JSON 형식에 맞춰 요청을 보내면 된다.

### 0.2 백엔드 수정이 필요한 경우

Unreal UI가 기존 계약에 없는 새로운 의미를 요구할 때다.

```text
특정 기계만 우선 분석하고 싶다
-> payload.target을 백엔드가 우선순위에 반영해야 함

주기 상태 업데이트만 보고 자동으로 서브퀘스트 후보를 만들고 싶다
-> state_update 응답에 optimization_hint 또는 quest_candidate 로직 추가 필요

최적화 모드를 버튼별로 나누고 싶다
-> payload.scope, payload.goal, payload.target 같은 필드 처리 강화 필요

Unreal UI가 원하는 응답 구조가 현재 payload와 다르다
-> 백엔드 응답 포맷 조정 필요
```

현재 구현 기준으로는 `operator_guide` 질문과 `process_optimizer`의 `state_update`, `analyze`, `apply`, `measure`, `undo` 호출은 이미 준비되어 있다. 우선은 Unreal이 이 계약에 맞춰 버튼/대화창/상호작용 UI에서 JSON을 보내는 방식으로 연동하면 된다.

## 1. 공통 Envelope

Unreal은 모든 Agent 요청을 아래 공통 구조로 보낸다.

```json
{
  "type": "agent.request",
  "request_id": "요청마다 고유한 ID",
  "session_id": "플레이어 세션 ID",
  "client_id": "unreal-client",
  "agent": "operator_guide 또는 process_optimizer",
  "payload": {},
  "context": {
    "language": "ko",
    "mode": "gameplay"
  }
}
```

백엔드는 같은 `request_id`, `session_id`, `agent`를 포함해 응답한다.

```json
{
  "type": "agent.response",
  "request_id": "요청마다 고유한 ID",
  "session_id": "플레이어 세션 ID",
  "client_id": "unreal-client",
  "agent": "operator_guide 또는 process_optimizer",
  "payload": {},
  "streams": []
}
```

## 2. operator_guide

`operator_guide`는 플레이어가 장비, 레시피, 문제 해결을 질문하면 매뉴얼/RAG 근거와 현재 게임 상태를 바탕으로 답변하는 Agent다.

### 2.1 자유 질문

플레이어가 AI 대화창에 자유롭게 질문하는 경우다. `sub_agent`를 생략하면 백엔드가 질문 의미를 보고 `machine_help`, `recipe_explainer`, `troubleshooter` 중 하나를 고른다.

#### Unreal 요청

```json
{
  "type": "agent.request",
  "request_id": "unreal-guide-free-001",
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

#### 백엔드 응답 예시

```json
{
  "type": "agent.response",
  "request_id": "unreal-guide-free-001",
  "session_id": "player-session-001",
  "client_id": "unreal-client",
  "agent": "operator_guide",
  "payload": {
    "final_answer": "분쇄기는 원석 같은 재료를 가공해서 다음 생산 단계에 쓸 수 있는 형태로 만드는 장비입니다. 보통 입력 자원을 넣고, 출력 자원을 컨베이어나 보관함으로 빼는 흐름으로 사용합니다.",
    "actions": [],
    "question": "분쇄기가 뭐야? 어디에 써?",
    "topic": "machine",
    "metadata": {
      "selectedAgent": "operator_guide",
      "selectedLeafAgent": "operator_guide.machine_help",
      "sources": [
        {
          "doc_id": "equipment_grinder",
          "type": "equipment",
          "title": "분쇄기"
        }
      ],
      "confidence": "high"
    }
  },
  "streams": []
}
```

Unreal 표시 기준:

```text
payload.final_answer -> NPC 대화창 본문
payload.metadata.sources -> 필요하면 근거/출처 UI
payload.metadata.selectedLeafAgent -> 디버그 표시용
```

### 2.2 UI 탭이 정해진 질문

Unreal UI에서 이미 “설비 도움말”, “레시피 설명”, “문제 해결” 탭이 정해져 있다면 `payload.sub_agent`를 직접 보낼 수 있다.

사용 가능한 값:

```text
operator_guide.machine_help
operator_guide.recipe_explainer
operator_guide.troubleshooter
```

#### Unreal 요청

```json
{
  "type": "agent.request",
  "request_id": "unreal-guide-machine-001",
  "session_id": "player-session-001",
  "client_id": "unreal-client",
  "agent": "operator_guide",
  "payload": {
    "sub_agent": "operator_guide.machine_help",
    "question": "제련기는 입력과 출력이 어떻게 연결돼?"
  },
  "context": {
    "language": "ko",
    "mode": "gameplay",
    "current_game_state": {
      "selectedMachine": {
        "id": "smelter_1",
        "type": "smelter",
        "recipe_id": "iron_ingot"
      }
    }
  }
}
```

#### 백엔드 응답 예시

```json
{
  "type": "agent.response",
  "request_id": "unreal-guide-machine-001",
  "session_id": "player-session-001",
  "client_id": "unreal-client",
  "agent": "operator_guide",
  "payload": {
    "final_answer": "제련기는 입력 슬롯으로 원석을 받고, 선택된 레시피에 따라 제련된 자원을 출력합니다. 현재 선택된 제련기가 철괴 레시피라면 철광석이 입력되고 철괴가 출력되는 흐름으로 보면 됩니다.",
    "actions": [],
    "question": "제련기는 입력과 출력이 어떻게 연결돼?",
    "topic": "machine",
    "metadata": {
      "selectedAgent": "operator_guide",
      "selectedLeafAgent": "operator_guide.machine_help",
      "usedCurrentGameState": true
    }
  },
  "streams": []
}
```

### 2.3 답변 길이 조절

`context.response_style`로 NPC 답변 길이를 조절할 수 있다.

```text
short: 짧은 답변
normal: 기본 답변
detailed: 자세한 답변
```

```json
{
  "type": "agent.request",
  "request_id": "unreal-guide-short-001",
  "session_id": "player-session-001",
  "client_id": "unreal-client",
  "agent": "operator_guide",
  "payload": {
    "sub_agent": "operator_guide.recipe_explainer",
    "question": "철괴 만들려면 뭐가 필요해?"
  },
  "context": {
    "language": "ko",
    "mode": "gameplay",
    "response_style": "short"
  }
}
```

## 3. process_optimizer

`process_optimizer`는 공장 상태를 분석해 최적화 preview를 만들고, 플레이어가 승인한 경우에만 Unreal 실행용 명령을 반환한다.

```text
state_update -> 상태 기억만 함. 실행 없음.
analyze -> 최적화 preview 생성. 실행 없음.
apply -> 플레이어 승인 후 실행 명령 생성.
measure -> 적용 후 실제 효과 측정.
undo -> 플레이어 요청 시 안전한 되돌리기 명령 생성.
```

### 3.1 주기 상태 업데이트

Unreal이 주기적으로 공장 상태를 보내는 경우다. 백엔드는 상태를 기억하지만 최적화를 자동 실행하지 않는다.

#### Unreal 요청

```json
{
  "type": "agent.request",
  "request_id": "unreal-optimizer-state-001",
  "session_id": "player-session-001",
  "client_id": "unreal-client",
  "agent": "process_optimizer",
  "payload": {
    "operation": "state_update",
    "goal": "balance",
    "factoryRevision": 42,
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
              "amount": 0.0,
              "max_amount": 100.0
            }
          ],
          "outputs": []
        }
      ],
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

#### 백엔드 응답 예시

```json
{
  "type": "agent.response",
  "request_id": "unreal-optimizer-state-001",
  "session_id": "player-session-001",
  "client_id": "unreal-client",
  "agent": "process_optimizer",
  "payload": {
    "status": "success",
    "factoryRevision": 42,
    "goal": "balance",
    "metadata": {
      "selectedAgent": "process_optimizer",
      "selectedLeafAgent": "process_optimizer"
    }
  },
  "streams": []
}
```

### 3.2 플레이어가 최적화 버튼을 누른 경우

공장 UI 또는 특정 기계 UI에서 “최적화 요청” 버튼을 누르면 `operation: "analyze"`를 보낸다. 특정 기계 중심 요청이면 `payload.target`을 함께 보낸다.

#### Unreal 요청

```json
{
  "type": "agent.request",
  "request_id": "unreal-optimizer-analyze-001",
  "session_id": "player-session-001",
  "client_id": "unreal-client",
  "agent": "process_optimizer",
  "payload": {
    "operation": "analyze",
    "goal": "balance",
    "target": {
      "type": "machine",
      "id": "smelter_1"
    },
    "factoryRevision": 42,
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
              "amount": 0.0,
              "max_amount": 100.0
            }
          ],
          "outputs": [],
          "power_consumption": 15.0
        }
      ],
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

#### 백엔드 응답 예시

```json
{
  "type": "agent.response",
  "request_id": "unreal-optimizer-analyze-001",
  "session_id": "player-session-001",
  "client_id": "unreal-client",
  "agent": "process_optimizer",
  "payload": {
    "status": "preview",
    "plan_id": "plan-bf67a123",
    "factoryRevision": 42,
    "goal": "balance",
    "summary": "smelter_1의 원자재 입력 부족을 해결하기 위한 최적화 개선안을 제안합니다.",
    "changes": [
      {
        "id": "suggest_input_smelter_1",
        "target": {
          "type": "machine",
          "id": "smelter_1"
        },
        "problem": "smelter_1 설비의 원자재 입력 재고가 고갈되었습니다.",
        "recommended_action": "공급 라인의 컨베이어 벨트 연결과 상류 설비의 생산 상태를 점검하십시오.",
        "expected_effect": "설비 가동율이 복구되어 smelter_1의 정상 공정이 다시 가동됩니다.",
        "risk": "low",
        "confidence": 1.0,
        "reason": "원자재 입력 부족이 직접적인 공정 중단 원인입니다.",
        "priority_explanation": "위험도가 낮고 병목 원인이 명확하므로 우선 점검 대상입니다."
      }
    ],
    "expected_effect": {
      "estimated": false,
      "resolved_input_shortages_count": 1,
      "resolved_output_blocks_count": 0,
      "resolved_conveyor_congestions_count": 0
    },
    "ui_hints": {
      "highlight_targets": ["smelter_1"]
    },
    "expires_at": "2026-06-26T12:35:00Z",
    "player_message": "smelter_1에 원자재가 다시 투입될 수 있도록 공급 라인과 상류 설비 상태를 확인해 주세요.",
    "metadata": {
      "llm": "used",
      "llmProvider": "openai",
      "llmModel": "gpt-5.4-nano",
      "selectedAgent": "process_optimizer",
      "selectedLeafAgent": "process_optimizer"
    }
  },
  "streams": []
}
```

Unreal 표시 기준:

```text
payload.summary -> 최적화 창 상단 요약
payload.changes -> 제안 카드 목록
payload.ui_hints.highlight_targets -> 월드 하이라이트 대상
payload.plan_id -> apply/undo/measure에 다시 사용
```

### 3.3 플레이어 승인 후 적용 요청

플레이어가 preview 카드를 검토하고 “적용”을 누른 경우에만 보낸다.

#### Unreal 요청

```json
{
  "type": "agent.request",
  "request_id": "unreal-optimizer-apply-001",
  "session_id": "player-session-001",
  "client_id": "unreal-client",
  "agent": "process_optimizer",
  "payload": {
    "operation": "apply",
    "plan_id": "plan-bf67a123",
    "factoryRevision": 42,
    "approval": true,
    "approved_change_ids": ["suggest_input_smelter_1"],
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
              "amount": 0.0,
              "max_amount": 100.0
            }
          ]
        }
      ],
      "conveyors": [],
      "power_grid": {
        "produced": 120.0,
        "consumed": 90.0
      }
    },
    "before_states": {
      "suggest_input_smelter_1": {
        "id": "smelter_1",
        "type": "smelter",
        "status": "operating",
        "operating_rate": 0.2,
        "inputs": [
          {
            "item_id": "iron_ore",
            "amount": 0.0,
            "max_amount": 100.0
          }
        ]
      }
    }
  },
  "context": {
    "language": "ko",
    "mode": "gameplay"
  }
}
```

#### 백엔드 응답 예시

```json
{
  "type": "agent.response",
  "request_id": "unreal-optimizer-apply-001",
  "session_id": "player-session-001",
  "client_id": "unreal-client",
  "agent": "process_optimizer",
  "payload": {
    "status": "execute_ready",
    "plan_id": "plan-bf67a123",
    "factoryRevision": 42,
    "goal": "balance",
    "summary": "최적화 계획 실행 준비가 완료되었습니다.",
    "approved_changes": [
      {
        "id": "suggest_input_smelter_1",
        "target": {
          "type": "machine",
          "id": "smelter_1"
        },
        "problem": "smelter_1 설비의 원자재 입력 재고가 고갈되었습니다."
      }
    ],
    "commands": [
      {
        "command": "set_machine_enabled",
        "machine_id": "smelter_1",
        "enabled": true
      }
    ]
  },
  "streams": []
}
```

Unreal 처리 기준:

```text
1. payload.commands를 바로 실행하지 말고 Unreal 월드 규칙으로 최종 검증한다.
2. 대상 존재, 연결 가능, 자원 보유, 전력 한도, factoryRevision 충돌을 확인한다.
3. 검증 통과 후 실제 월드에 적용한다.
```

### 3.4 승인 없는 apply 차단

`approval: true`가 없으면 백엔드는 실행 명령을 만들지 않는다.

```json
{
  "type": "agent.request",
  "request_id": "unreal-optimizer-no-approval-001",
  "session_id": "player-session-001",
  "client_id": "unreal-client",
  "agent": "process_optimizer",
  "payload": {
    "operation": "apply",
    "plan_id": "plan-bf67a123",
    "factoryRevision": 42,
    "approval": false,
    "approved_change_ids": ["suggest_input_smelter_1"]
  },
  "context": {
    "language": "ko",
    "mode": "gameplay"
  }
}
```

응답 예시:

```json
{
  "type": "agent.response",
  "request_id": "unreal-optimizer-no-approval-001",
  "session_id": "player-session-001",
  "client_id": "unreal-client",
  "agent": "process_optimizer",
  "payload": {
    "status": "approval_required",
    "factoryRevision": 42,
    "goal": "balance",
    "summary": "최적화 계획 실행 준비 실패: Optimization apply requires explicit approval (approval=true).",
    "commands": []
  },
  "streams": []
}
```

### 3.5 적용 후 성과 측정

최소 30초와 3 production cycle 이후에 보낸다.

```json
{
  "type": "agent.request",
  "request_id": "unreal-optimizer-measure-001",
  "session_id": "player-session-001",
  "client_id": "unreal-client",
  "agent": "process_optimizer",
  "payload": {
    "operation": "measure",
    "plan_id": "plan-bf67a123",
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
              "amount": 10.0,
              "max_amount": 100.0
            }
          ]
        }
      ],
      "conveyors": [],
      "power_grid": {
        "produced": 120.0,
        "consumed": 95.0
      }
    }
  },
  "context": {
    "language": "ko",
    "mode": "gameplay"
  }
}
```

응답 예시:

```json
{
  "type": "agent.response",
  "request_id": "unreal-optimizer-measure-001",
  "session_id": "player-session-001",
  "client_id": "unreal-client",
  "agent": "process_optimizer",
  "payload": {
    "status": "measurement_ready",
    "plan_id": "plan-bf67a123",
    "summary": "최적화 효과 측정 결과, 공장 가동 상태가 'success' 등급으로 분석되었습니다.",
    "commands": [],
    "measurement_result": {
      "status": "success",
      "next_action": "monitor",
      "actual_effect": {
        "resolved_input_shortages_count": 1,
        "average_operating_rate_before": 0.2,
        "average_operating_rate_after": 1.0
      },
      "production_cycles": 5
    }
  },
  "streams": []
}
```

### 3.6 플레이어 요청 Undo

플레이어가 되돌리기를 명시적으로 누른 경우에만 보낸다.

```json
{
  "type": "agent.request",
  "request_id": "unreal-optimizer-undo-001",
  "session_id": "player-session-001",
  "client_id": "unreal-client",
  "agent": "process_optimizer",
  "payload": {
    "operation": "undo",
    "plan_id": "plan-bf67a123",
    "factoryRevision": 43,
    "factory_state": {
      "machines": [
        {
          "id": "smelter_1",
          "type": "smelter",
          "status": "operating",
          "recipe_id": "iron_ingot"
        }
      ],
      "conveyors": []
    }
  },
  "context": {
    "language": "ko",
    "mode": "gameplay"
  }
}
```

응답 예시:

```json
{
  "type": "agent.response",
  "request_id": "unreal-optimizer-undo-001",
  "session_id": "player-session-001",
  "client_id": "unreal-client",
  "agent": "process_optimizer",
  "payload": {
    "status": "undo_ready",
    "plan_id": "plan-bf67a123",
    "summary": "되돌리기 계획 준비가 완료되었습니다.",
    "commands": [
      {
        "command": "set_machine_enabled",
        "machine_id": "smelter_1",
        "enabled": false
      }
    ]
  },
  "streams": []
}
```

## 4. Unreal 구현 요약

```text
operator_guide:
1. 플레이어 질문 텍스트를 payload.question에 담아 보낸다.
2. UI 탭이 정해져 있으면 payload.sub_agent를 함께 보낸다.
3. payload.final_answer를 대화창에 표시한다.

process_optimizer:
1. 주기적으로 operation: state_update를 보낼 수 있다.
2. 최적화 버튼 또는 특정 기계 상호작용 버튼은 operation: analyze를 보낸다.
3. preview 응답의 plan_id, changes, ui_hints를 UI에 저장한다.
4. 플레이어 승인 후 operation: apply를 보낸다.
5. execute_ready.commands는 Unreal 월드 규칙으로 최종 검증 후 실행한다.
6. 적용 후 measure, 플레이어 요청 시 undo를 보낸다.
```
## Sprint 3 추가 계약: process_optimizer 서브퀘스트 후보와 상세 분석 연결

`process_optimizer`의 `state_update` 응답은 공장을 자동으로 바꾸지 않는다. 문제가 감지되면 `optimization_alert.suggested_subquest`를 내려주고, Unreal은 이 값을 UI 후보 카드로 보여줄 수 있다.

Unreal이 후보를 표시할 때 사용할 주요 필드:

```text
optimization_alert.needed
optimization_alert.severity
optimization_alert.reason
optimization_alert.target
optimization_alert.suggested_subquest.title
optimization_alert.suggested_subquest.objective
optimization_alert.suggested_subquest.target
optimization_alert.suggested_subquest.severity
optimization_alert.suggested_subquest.next_request
```

플레이어가 후보를 클릭하면 Unreal은 `suggested_subquest.next_request`에 최신 `factoryRevision`과 `factory_state`를 붙여서 다시 `analyze` 요청을 보낸다.

```json
{
  "type": "agent.request",
  "request_id": "unreal-optimizer-subquest-analyze-001",
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
              "amount": 0.0,
              "max_amount": 100.0
            }
          ],
          "outputs": []
        }
      ],
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

중요한 안전 규칙:

```text
state_update -> suggested_subquest 후보만 반환, commands 없음
subquest analyze -> preview만 반환, commands 없음
apply approval=true -> execute_ready commands 반환
Unreal -> commands를 실제 월드 규칙으로 최종 검증 후 실행
```
