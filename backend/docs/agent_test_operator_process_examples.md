# Agent-Test JSON 예시 가이드

이 문서는 로컬 백엔드 서버를 띄운 뒤 브라우저의 `/agent-test` 화면에서 `operator_guide`와 `process_optimizer`를 직접 테스트하기 위한 JSON 예시를 정리한다.

## 1. 서버 실행

agent-test에서 실제 `.env.prod`의 DB, RAG, LLM 설정을 확인하려면 아래 명령으로 서버를 실행한다.

PowerShell 또는 cmd.exe 모두 같은 명령을 사용할 수 있다.

```powershell
cd C:\factory-space\backend
uv run --env-file .env.prod python scripts/run_dev_server.py
```

prod 서버 스크립트로 확인하려면 다음 명령을 사용한다.

```bat
cd C:\factory-space\backend
uv run --env-file .env.prod python scripts/run_prod_server.py
```

RAG DB와 외부 LLM에 의존하지 않는 로컬 fallback 동작만 빠르게 확인하려면 아래처럼 mock 설정으로 실행할 수 있다. 이 경우 실제 LLM 답변은 생성되지 않는다.

PowerShell:

```powershell
cd C:\factory-space\backend
$env:FACTORY_RAG_RUNTIME_MOCK = "true"
$env:FACTORY_LLM_DEFAULT_PROVIDER = "none"
$env:FACTORY_LLM_FALLBACK1_PROVIDER = "none"
$env:FACTORY_LLM_FALLBACK2_PROVIDER = "none"
uv run python scripts/run_dev_server.py
```

cmd.exe 또는 `(factory-space) C:\...>` 형태의 프롬프트:

```bat
cd C:\factory-space\backend
set FACTORY_RAG_RUNTIME_MOCK=true
set FACTORY_LLM_DEFAULT_PROVIDER=none
set FACTORY_LLM_FALLBACK1_PROVIDER=none
set FACTORY_LLM_FALLBACK2_PROVIDER=none
uv run python scripts/run_dev_server.py
```

주의:

```text
- .env.prod는 uv run --env-file .env.prod ... 명령을 사용할 때만 적용된다.
- 같은 cmd/PowerShell 창에서 이전에 설정한 FACTORY_LLM_* 환경변수가 남아 있으면 .env.prod 값보다 우선될 수 있다. LLM mock 테스트 후에는 새 터미널을 열거나 해당 변수를 비우고 실행한다.
- RAG DB가 꺼져 있거나 접근 불가하면 operator_guide는 CSV fallback으로 응답한다.
- .env.prod의 모델명이 실제 사용 가능한 모델이 아니면 LLM 호출은 실패할 수 있다.
- LLM을 끈 로컬 테스트에서는 operator_guide payload에 sub_agent를 직접 넣어 leaf routing을 건너뛴다.
- LLM을 끈 로컬 테스트에서는 process_optimizer도 계산 결과 기반 fallback 설명을 반환한다.
- WebSocket 내부 예외는 연결 종료 대신 agent.error 응답으로 표시된다.
```

정상 실행 로그 예시:

```text
Uvicorn running on http://0.0.0.0:18000
```

접속 주소:

```text
agent-test 화면: http://127.0.0.1:18000/agent-test
health check:    http://127.0.0.1:18000/health
WebSocket:       ws://127.0.0.1:18000/ws/agent
```

서버가 켜진 뒤 `/agent-test` 화면의 JSON 입력칸에 아래 예시를 하나씩 넣고 전송한다.

## 2. operator_guide 예시

### 2.1 장비 설명 질문

```json
{
  "type": "agent.request",
  "request_id": "operator-guide-machine-001",
  "session_id": "agent-test-session",
  "client_id": "agent-test-console",
  "agent": "operator_guide",
  "payload": {
    "sub_agent": "operator_guide.machine_help",
    "question": "분쇄기가 뭐야? 어디에 써?"
  },
  "context": {
    "language": "ko",
    "mode": "agent_test"
  }
}
```

기대 결과:

```text
agent: operator_guide
payload.final_answer 또는 payload.answer 계열 응답
metadata.selectedLeafAgent: operator_guide.machine_help 계열
```

### 2.2 레시피 설명 질문

```json
{
  "type": "agent.request",
  "request_id": "operator-guide-recipe-001",
  "session_id": "agent-test-session",
  "client_id": "agent-test-console",
  "agent": "operator_guide",
  "payload": {
    "sub_agent": "operator_guide.recipe_explainer",
    "question": "철괴를 만들려면 어떻게 해야 돼?"
  },
  "context": {
    "language": "ko",
    "mode": "agent_test"
  }
}
```

기대 결과:

```text
agent: operator_guide
metadata.selectedLeafAgent: operator_guide.recipe_explainer 계열
철광석, 제련기, 철괴 생산 흐름 안내
```

### 2.3 문제 해결 질문

```json
{
  "type": "agent.request",
  "request_id": "operator-guide-trouble-001",
  "session_id": "agent-test-session",
  "client_id": "agent-test-console",
  "agent": "operator_guide",
  "payload": {
    "sub_agent": "operator_guide.troubleshooter",
    "question": "컨베이어가 멈췄는데 뭘 확인해야 해?"
  },
  "context": {
    "language": "ko",
    "mode": "agent_test"
  }
}
```

기대 결과:

```text
agent: operator_guide
metadata.selectedLeafAgent: operator_guide.troubleshooter 계열
전력, 입력 자원, 출력 공간, 연결 상태 점검 안내
```

### 2.4 답변 길이 스타일 옵션

`operator_guide`는 `context.response_style`로 답변 길이를 조절할 수 있다.

```text
short: 1~2문장 중심의 짧은 NPC 대사
normal: 기본값. 2~3문장 중심의 설명
detailed: 4~6문장 정도의 자세한 설명
```

예시:

```json
{
  "type": "agent.request",
  "request_id": "operator-guide-machine-short-001",
  "session_id": "agent-test-session",
  "client_id": "agent-test-console",
  "agent": "operator_guide",
  "payload": {
    "sub_agent": "operator_guide.machine_help",
    "question": "분쇄기가 뭐야?"
  },
  "context": {
    "language": "ko",
    "mode": "agent_test",
    "response_style": "short"
  }
}
```

### 2.5 응답 품질 평가 질문 세트

agent-test에서 아래 질문을 바꿔 넣으며 `final_answer`를 확인한다.

```text
장비 설명:
- 분쇄기가 뭐야?
- 제련기는 어디에 써?
- 컨베이어 벨트는 뭘 연결하는 장비야?

레시피 설명:
- 철괴 만들려면 뭐가 필요해?
- 나무판자는 어떻게 만들어?
- 구리선을 만들려면 어떤 장비가 필요해?

문제 해결:
- 제련기가 왜 안 돌아가?
- 컨베이어가 막힌 것 같아. 뭘 확인해야 해?
- 전력이 부족하면 어떤 순서로 확인해야 해?

실패/근거 부족:
- 우주 엘리베이터는 어떻게 업그레이드해?
- 없는 장비 이름으로 물어보기
```

확인 기준:

```text
- 영어 에러 문구가 노출되지 않는다.
- JSON, RAG, LLM, database 같은 내부 구현 용어가 player-facing 답변에 나오지 않는다.
- 장비/자원/레시피 이름은 자연스러운 한국어 표시명으로 나온다.
- 짧은 질문은 2~3문장 안에서 끝난다.
- 근거가 부족하면 추측하지 않고 다시 물어볼 수 있게 안내한다.
```

### 2.6 통신탑(제작 가능한 설치물) 제작 및 애매한 질문

통신탑처럼 장비(`equipment`)와 생산 가능한 자원(`resource`) 양쪽의 성격을 모두 가진 설치물의 예시입니다.

#### A. 명확한 제작 질문 (Unambiguous Crafting)
질문에 `지어`, `건설`, `재료` 등 생산 관련 키워드가 포함되면 바로 자원 레시피 질문으로 수렴합니다.

```json
{
  "type": "agent.request",
  "request_id": "operator-guide-telecom-recipe-001",
  "session_id": "agent-test-session",
  "client_id": "agent-test-console",
  "agent": "operator_guide",
  "payload": {
    "question": "통신탑 어떻게 지어야 해?"
  },
  "context": {
    "language": "ko",
    "mode": "agent_test"
  }
}
```

기대 결과:
- `isAmbiguous: false`
- `question_type: resource_question` 또는 `recipe_question`
- 합성기에서 철근 20개, 구리선 20개, 주석판 20개 등을 사용해 조립한다는 답변이 나옴.

#### B. 모호한 질문 (Ambiguous Question)
의도 분류 키워드가 명확하지 않은 경우, 내부적으로 `isAmbiguous: true`로 마킹한 뒤 백엔드 LLM 보조 의도 분류기를 거쳐 최종 의도로 보정되거나 룰 기반 fallback이 수행됩니다.

```json
{
  "type": "agent.request",
  "request_id": "operator-guide-telecom-ambiguous-001",
  "session_id": "agent-test-session",
  "client_id": "agent-test-console",
  "agent": "operator_guide",
  "payload": {
    "question": "통신탑 알려줘"
  },
  "context": {
    "language": "ko",
    "mode": "agent_test"
  }
}
```

기대 결과:
- `isAmbiguous: true` 또는 LLM 보정이 성공하면 `false`
- `target_ids`에 장비 ID(`equipment_telecommunication_tower`)와 자원 ID(`resource_TeleCommunicationTower`) 후보가 안전하게 획득됨.

## 3. process_optimizer 예시

`process_optimizer`는 자동 실행 Agent가 아니다. `state_update`는 상태 기억만 하고, `analyze`는 preview 계획만 반환한다. 실제 공장 변경 명령은 플레이어가 승인한 `apply` 요청 이후에만 생성된다.

분석 지표, 병목 판단, 명령 후보는 코드가 계산한다. LLM이 켜져 있으면 `analyze` 응답의 `summary`, `player_message`, 변경 항목별 `reason`과 `priority_explanation` 같은 플레이어용 설명만 보강한다. LLM이 꺼져 있으면 metadata에 `llm: "fallback"`이 표시되고 코드 기반 기본 설명이 반환된다.

### 3.1 주기 상태 업데이트

Unreal이 플레이 중 주기적으로 보낼 수 있는 요청이다. 백엔드는 session memory만 갱신하고 공장을 변경하지 않는다.

```json
{
  "type": "agent.request",
  "request_id": "optimizer-state-update-001",
  "session_id": "optimizer-agent-test-session",
  "client_id": "agent-test-console",
  "agent": "process_optimizer",
  "payload": {
    "operation": "state_update",
    "goal": "balance",
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
    "mode": "agent_test"
  }
}
```

기대 결과:

```text
payload.status: success
payload.factoryRevision: 12
```

### 3.2 최적화 분석 preview

입력 부족인 제련기 상태를 보내 preview 계획을 받는 예시다.

```json
{
  "type": "agent.request",
  "request_id": "optimizer-analyze-001",
  "session_id": "optimizer-agent-test-session",
  "client_id": "agent-test-console",
  "agent": "process_optimizer",
  "payload": {
    "operation": "analyze",
    "goal": "balance",
    "factoryRevision": 12,
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
    "mode": "agent_test"
  }
}
```

기대 결과:

```text
payload.status: preview
payload.plan_id: 생성된 plan id
payload.changes 또는 payload.suggestions: 최대 3개 제안
payload.ui_hints.highlight_targets: ["smelter_1"] 포함
commands 없음
```

`apply`, `undo`, `measure`는 `analyze` 응답에서 받은 실제 `plan_id`를 사용해야 한다.

### 3.3 승인 없는 적용 차단

아래 `plan_id`는 예시다. 실제 테스트에서는 3.2 응답의 `payload.plan_id`로 바꾼다.

```json
{
  "type": "agent.request",
  "request_id": "optimizer-apply-no-approval-001",
  "session_id": "optimizer-agent-test-session",
  "client_id": "agent-test-console",
  "agent": "process_optimizer",
  "payload": {
    "operation": "apply",
    "plan_id": "PASTE_PLAN_ID_FROM_ANALYZE",
    "factoryRevision": 12,
    "approval": false
  },
  "context": {
    "language": "ko",
    "mode": "agent_test"
  }
}
```

기대 결과:

```text
payload.status: approval_required
commands 없음
```

### 3.4 승인 적용

`approved_change_ids`는 3.2 preview 응답의 변경 id를 넣는다. 입력 부족 제련기 예시는 보통 `suggest_input_smelter_1`이 생성된다.

`before_states`는 선택 항목이지만 권장한다. Unreal이 플레이어 승인 직전의 변경 대상 상태를 `change_id`별로 보내면 백엔드가 실행 기록에 정확한 before 값을 저장할 수 있어, 이후 `measure`와 `undo` 품질이 좋아진다.

```json
{
  "type": "agent.request",
  "request_id": "optimizer-apply-001",
  "session_id": "optimizer-agent-test-session",
  "client_id": "agent-test-console",
  "agent": "process_optimizer",
  "payload": {
    "operation": "apply",
    "plan_id": "PASTE_PLAN_ID_FROM_ANALYZE",
    "factoryRevision": 12,
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
    "mode": "agent_test"
  }
}
```

기대 결과:

```text
payload.status: execute_ready
payload.commands: Unreal이 최종 월드 검증 후 실행할 명령 목록
```

Unreal이 명령 실행 직후의 실제 상태까지 알고 있는 경우에는 같은 형식으로 `after_states`를 보낼 수 있다. 없으면 백엔드는 `commands` 기반의 계획된 after 상태를 저장하고, 최종 월드 검증은 Unreal이 담당한다.

### 3.5 성과 측정

적용 후 최소 30초와 3 production cycle이 지난 상태를 가정한 예시다.

```json
{
  "type": "agent.request",
  "request_id": "optimizer-measure-001",
  "session_id": "optimizer-agent-test-session",
  "client_id": "agent-test-console",
  "agent": "process_optimizer",
  "payload": {
    "operation": "measure",
    "plan_id": "PASTE_PLAN_ID_FROM_ANALYZE",
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
              "max_amount": 10.0
            }
          ]
        }
      ],
      "conveyors": []
    }
  },
  "context": {
    "language": "ko",
    "mode": "agent_test"
  }
}
```

기대 결과:

```text
payload.status: measurement_ready
payload.measurement_result.status: success | failed | degraded
payload.measurement_result.next_action: monitor | reanalyze
```

### 3.6 되돌리기 충돌 확인

적용 후 플레이어가 제련기 레시피를 직접 바꾼 상황을 가정한다. 이 경우 자동 Undo가 차단되어야 한다.

```json
{
  "type": "agent.request",
  "request_id": "optimizer-undo-conflict-001",
  "session_id": "optimizer-agent-test-session",
  "client_id": "agent-test-console",
  "agent": "process_optimizer",
  "payload": {
    "operation": "undo",
    "plan_id": "PASTE_PLAN_ID_FROM_ANALYZE",
    "factory_state": {
      "machines": [
        {
          "id": "smelter_1",
          "type": "smelter",
          "status": "operating",
          "recipe_id": "copper_ingot"
        }
      ],
      "conveyors": []
    }
  },
  "context": {
    "language": "ko",
    "mode": "agent_test"
  }
}
```

기대 결과:

```text
payload.status: undo_conflict
commands 없음
```

## 4. 서버 smoke 명령

서버를 띄운 상태에서 실제 WebSocket smoke를 돌릴 수 있다.

```powershell
cd C:\factory-space\backend
uv run python scripts/smoke_agent_pipeline.py local --base-url http://127.0.0.1:18000
```

기대 결과:

```text
PASS local/process_optimizer
PASS local/process_optimizer_state_update
PASS local/process_optimizer_apply_no_app
PASS local/process_optimizer_apply_success
PASS local/process_optimizer_apply_conflict
PASS local/process_optimizer_undo_conflict
PASS local/process_optimizer_measure_not_ready
PASS local/process_optimizer_measure_ready
```

## 5. 주의할 점

- `process_optimizer`의 `apply`, `undo`, `measure`는 반드시 이전 `analyze` 응답의 `plan_id`를 사용한다.
- `factoryRevision`이 preview 생성 시점과 다르면 `revision_conflict`가 정상이다.
- `state_update`는 자동 최적화 실행이 아니라 최신 상태 기억용이다.
- `operator_guide`는 일반 질문 답변 Agent이고, `process_optimizer`는 공장 변경 후보를 preview/approval 흐름으로 다루는 Agent다.

## 6. Unreal 연동 JSON 흐름

실제 Unreal 클라이언트도 같은 `/ws/agent` WebSocket envelope를 사용한다.

```text
WebSocket: ws://127.0.0.1:18000/ws/agent
공통 필드: type, request_id, session_id, client_id, agent, payload, context
```

### 6.1 operator_guide: 자유 질문

플레이어가 NPC에게 일반 질문을 입력한 경우다. `sub_agent`를 생략하면 백엔드가 LLM 라우팅으로 장비/레시피/문제 해결 leaf agent를 고른다.

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

기대 응답:

```text
type: agent.response
agent: operator_guide
payload.final_answer: 플레이어에게 표시할 답변
payload.metadata.selectedLeafAgent: 선택된 leaf agent
payload.metadata.currentModel: 실제 사용된 LLM slot/provider/model
```

### 6.2 operator_guide: NPC 메뉴가 이미 정해진 질문

Unreal UI에서 “설비 도움말”, “레시피 설명”, “문제 해결” 탭이 이미 선택된 경우에는 `sub_agent`를 명시해 라우팅을 건너뛸 수 있다.

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

사용 가능한 `operator_guide` leaf agent:

```text
operator_guide.machine_help
operator_guide.recipe_explainer
operator_guide.troubleshooter
```

### 6.3 process_optimizer: 주기 상태 업데이트

Unreal이 플레이 중 일정 주기 또는 공장 변경 이벤트 이후 최신 경량 상태를 보낼 때 사용한다. 이 요청은 공장을 바꾸지 않고 백엔드 memory만 갱신한다.

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

기대 응답:

```text
payload.status: success
payload.factoryRevision: 42
LLM 호출 없음
```

### 6.4 process_optimizer: 플레이어가 최적화 버튼을 누른 경우

NPC 또는 UI에서 플레이어가 최적화 분석을 명시적으로 요청한 시점에 최신 전체 snapshot을 보내는 흐름이다.

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

기대 응답:

```text
payload.status: preview
payload.plan_id: 이후 apply/undo/measure에 사용할 id
payload.changes: 플레이어가 검토할 최대 3개 변경 후보
payload.player_message: LLM이 보강한 플레이어용 안내
payload.ui_hints.highlight_targets: Unreal에서 하이라이트할 actor id
commands 없음
```

Unreal 처리:

```text
1. payload.changes를 UI 카드로 표시한다.
2. payload.ui_hints.highlight_targets에 해당하는 설비/컨베이어를 하이라이트한다.
3. 플레이어가 승인하기 전에는 월드 상태를 변경하지 않는다.
```

### 6.5 process_optimizer: 플레이어 승인 후 적용 요청

플레이어가 전체 적용 또는 선택 적용을 누른 경우에만 보낸다.

```json
{
  "type": "agent.request",
  "request_id": "unreal-optimizer-apply-001",
  "session_id": "player-session-001",
  "client_id": "unreal-client",
  "agent": "process_optimizer",
  "payload": {
    "operation": "apply",
    "plan_id": "PASTE_PLAN_ID_FROM_ANALYZE",
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
    }
  },
  "context": {
    "language": "ko",
    "mode": "gameplay"
  }
}
```

기대 응답:

```text
payload.status: execute_ready
payload.commands: Unreal이 최종 검증 후 실행할 명령 payload
```

Unreal 처리:

```text
1. commands를 즉시 실행하지 말고 월드 기준으로 대상 존재, 위치 점유, 연결 가능, 자원 보유, 전력 한도, factoryRevision을 다시 검증한다.
2. 검증 통과 명령만 실행한다.
3. 실행 결과와 새 factoryRevision을 클라이언트 상태에 반영한다.
```

### 6.6 process_optimizer: 적용 후 성과 측정

적용 후 최소 30초와 최소 3 production cycle이 모두 충족된 뒤 보낸다.

```json
{
  "type": "agent.request",
  "request_id": "unreal-optimizer-measure-001",
  "session_id": "player-session-001",
  "client_id": "unreal-client",
  "agent": "process_optimizer",
  "payload": {
    "operation": "measure",
    "plan_id": "PASTE_PLAN_ID_FROM_ANALYZE",
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

### 6.7 process_optimizer: 플레이어 요청 Undo

플레이어가 되돌리기를 명시적으로 누른 경우에만 보낸다. 현재 상태가 실행 기록의 `after` 값과 다르면 자동 복구가 차단된다.

```json
{
  "type": "agent.request",
  "request_id": "unreal-optimizer-undo-001",
  "session_id": "player-session-001",
  "client_id": "unreal-client",
  "agent": "process_optimizer",
  "payload": {
    "operation": "undo",
    "plan_id": "PASTE_PLAN_ID_FROM_ANALYZE",
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

## 7. 다음 단계

현재 백엔드 기준 다음 작업은 Unreal 연동 smoke를 맞추는 것이다.

```text
1. Unreal에서 /ws/agent 연결을 연다.
2. state_update를 주기적으로 보낸다.
3. 플레이어가 최적화 버튼을 누르면 analyze를 보낸다.
4. preview 응답의 plan_id, changes, ui_hints를 UI에 표시한다.
5. 플레이어 승인 후 apply를 보낸다.
6. execute_ready.commands를 Unreal 월드 규칙으로 재검증한 뒤 실행한다.
7. 적용 후 30초와 3 production cycle이 지나면 measure를 보낸다.
8. 플레이어가 undo를 누르면 undo를 보낸다.
```
## Sprint 3 추가 예시: process_optimizer state_update 서브퀘스트 후보 흐름

Sprint 3 기준으로 `state_update` 응답에는 문제가 감지된 경우 `optimization_alert.suggested_subquest`가 포함될 수 있다. 이 값은 Unreal UI가 플레이어에게 "최적화 분석을 열어볼까요?" 같은 후보 카드로 보여주기 위한 데이터이며, 자동 실행 명령이 아니다.

### state_update 응답에서 확인할 필드

```text
payload.optimization_alert.needed
payload.optimization_alert.severity
payload.optimization_alert.target
payload.optimization_alert.suggested_subquest.title
payload.optimization_alert.suggested_subquest.objective
payload.optimization_alert.suggested_subquest.target
payload.optimization_alert.suggested_subquest.severity
payload.optimization_alert.suggested_subquest.next_request
```

`next_request`는 그대로 실행하는 완성 요청이 아니라, Unreal이 최신 `factoryRevision`과 `factory_state`를 다시 붙여서 `analyze` 요청으로 보내기 위한 기본값이다.

### suggested_subquest 클릭 후 analyze 요청 예시

```json
{
  "type": "agent.request",
  "request_id": "optimizer-subquest-analyze-001",
  "session_id": "optimizer-agent-test-session",
  "client_id": "agent-test-console",
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
    "mode": "agent_test"
  }
}
```

기대 결과:

```text
payload.status: preview
payload.plan_id: 이후 apply/undo/measure에 사용할 id
payload.changes[0].target.id: smelter_1 우선 배치
payload.ui_hints.highlight_targets: smelter_1 포함
commands 없음
```

### Sprint 3 smoke 테스트 명령

```powershell
cd C:\factory-space\backend
uv run pytest tests/test_process_optimizer_subquest_unreal_flow.py -q
```

기대 결과:

```text
2 passed
```
