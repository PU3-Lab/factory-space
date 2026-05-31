# 서버 구현 계획

## 1. 목적

이 문서는 `protocol_plans.md`를 기반으로 Factory Space MVP 서버를 새로 구현하기 위한 작업 계획이다.

기존 backend 코드는 호환 대상으로 보지 않는다. 새 서버는 실시간 게임 서버가 아니라 WebSocket 기반 Agent 처리 서버다. 클라이언트는 공장 상태, 장비 설치, 철거, 회전, 배치 검증을 로컬에서 처리하고, 서버는 Agent 요청에 필요한 요약 스냅샷만 받아 분석, 추천, 질의응답, 퀘스트 생성, 신물질 생성을 수행한다.

## 2. 구현 범위

### 포함 범위

- FastAPI 기반 backend 서버
- `GET /health`
- `WS /ws/agent`
- WebSocket JSON message protocol
- Agent router
- 공통 Agent pipeline
- LLM adapter
- Agent별 payload schema
- Agent별 deterministic fallback
- in-memory response cache
- 신물질 `visualProfile` 및 placeholder visual 응답
- unit test 및 WebSocket integration test

### 제외 범위

- 장비 설치 서버 검증
- 장비 철거 서버 검증
- 장비 회전 서버 검증
- 공장 상태 실시간 동기화
- 컨베이어 tick 계산
- 서버 상시 시뮬레이션
- 멀티플레이 공동 건설
- 클라이언트 UI 상태 관리
- RAG 실제 연동
- 영구 game DB 연동
- 실제 이미지 생성 provider 연동
- texture/icon 파일 저장소

## 3. Public Protocol

### Endpoint

```txt
ws://localhost:8000/ws/agent
```

배포 환경에서는 같은 path를 유지한다.

```txt
wss://{api-host}/ws/agent
```

### Message Types

| Type | Direction | Description |
|---|---|---|
| `agent.request` | Client -> Server | Agent 실행 요청 |
| `agent.stream` | Server -> Client | 중간 진행 이벤트 |
| `agent.response` | Server -> Client | 최종 성공 응답 |
| `agent.error` | Server -> Client | 오류 응답 |
| `ping` | Client -> Server | 연결 확인 |
| `pong` | Server -> Client | 연결 확인 응답 |

### Agent IDs

| Agent ID | 역할 |
|---|---|
| `process_optimizer` | 생산량, 소비량, 전력, 병목 후보를 분석해 최적화 제안 생성 |
| `quest_generator` | 진행도, 해금 상태, 최근 생산 정보를 기반으로 다음 퀘스트 생성 |
| `manual_qa` | 질문과 현재 플레이 컨텍스트를 기반으로 매뉴얼 답변 생성 |
| `new_material_generator` | 미등록 재료 조합을 기반으로 신물질, 레시피, 비주얼 프로파일 생성 |

### Request Envelope

```json
{
  "type": "agent.request",
  "requestId": "req_process_001",
  "agent": "process_optimizer",
  "snapshotType": "summary",
  "snapshotHash": "fac_9ab23c",
  "payload": {}
}
```

### Response Envelope

```json
{
  "type": "agent.response",
  "requestId": "req_process_001",
  "agent": "process_optimizer",
  "status": "success",
  "payload": {}
}
```

### Error Envelope

```json
{
  "type": "agent.error",
  "requestId": "req_process_001",
  "agent": "process_optimizer",
  "status": "error",
  "error": {
    "code": "VALIDATION_ERROR",
    "message": "요청 payload가 올바르지 않습니다.",
    "details": {}
  }
}
```

## 4. 서버 구조

새 backend는 다음 구조를 목표로 한다.

```txt
backend
 ├─ main.py
 ├─ src/factory_space
 │   ├─ app.py
 │   ├─ websocket
 │   │   ├─ gateway.py
 │   │   └─ connection.py
 │   ├─ protocol
 │   │   ├─ messages.py
 │   │   └─ errors.py
 │   ├─ agents
 │   │   ├─ base.py
 │   │   ├─ router.py
 │   │   ├─ pipeline/
 │   │   │   ├─ __init__.py
 │   │   │   ├─ runtime.py
 │   │   │   ├─ graph_edges.py
 │   │   │   ├─ llm_fallback.py
 │   │   │   ├─ state.py
 │   │   │   └─ utils.py
 │   │   ├─ process_optimizer.py
 │   │   ├─ quest_generator.py
 │   │   ├─ manual_qa.py
 │   │   └─ new_material_generator.py
 │   ├─ llm
 │   │   ├─ adapter.py
 │   │   └─ prompts.py
 │   ├─ cache
 │   │   └─ response_cache.py
 │   └─ visual
 │       └─ profile.py
 └─ tests
     ├─ test_protocol.py
     ├─ test_agent_router.py
     ├─ test_agent_pipeline.py
     └─ test_websocket_agent.py
```

기존 파일을 보존하면서 점진 수정하지 않는다. 구현 단계에서는 backend를 새 구조에 맞게 정리하고, 남는 구형 protocol/router/agent 코드는 제거한다.

## 5. 공통 Agent Pipeline

모든 Agent 요청은 같은 pipeline을 통과한다.

```txt
WebSocket raw text
 -> JSON parse
 -> envelope validation
 -> request context 생성
 -> ping 처리 또는 agent routing
 -> agent별 payload validation
 -> cache lookup
 -> prompt/context build
 -> LLM call
 -> LLM JSON parse
 -> response schema validation
 -> cache write
 -> agent.response 반환
```

실패 시 처리 흐름은 다음과 같다.

```txt
invalid JSON / invalid envelope / unknown agent
 -> agent.error 반환

LLM timeout / provider error / invalid LLM JSON / response schema failure
 -> agent별 deterministic fallback 생성
 -> fallback 응답 schema validation
 -> agent.response 반환
```

LLM 실패는 사용자가 볼 수 있는 기능 실패로 처리하지 않는다. fallback도 정상 `agent.response`로 반환하되, payload metadata에 fallback 사용 여부를 남긴다.

## 6. LLM 정책

MVP에서는 OpenAI-compatible adapter를 기본으로 둔다.

### Environment Variables

```txt
OPENAI_API_KEY
OPENAI_MODEL
OPENAI_BASE_URL
LLM_TIMEOUT_SECONDS
```

`OPENAI_BASE_URL`은 선택값이다. 기본 OpenAI endpoint를 사용할 경우 설정하지 않는다.

### LLM Response Rules

- Agent prompt는 JSON object만 출력하도록 요구한다.
- 서버는 LLM raw text를 클라이언트에 직접 전달하지 않는다.
- LLM 응답은 JSON parse 후 agent별 response schema로 검증한다.
- 검증 실패 시 fallback으로 전환한다.
- timeout 기본값은 20초로 둔다.

## 7. Agent별 구현 계획

### 7.1 `process_optimizer`

입력 payload:

```json
{
  "factorySummary": {
    "powerStatus": "near_limit",
    "bottleneckCandidates": ["iron_powder"],
    "machineGroups": [],
    "itemFlows": [],
    "warnings": []
  }
}
```

출력 payload:

```json
{
  "summary": "철 가루 생산량이 부족합니다.",
  "bottlenecks": [],
  "suggestions": [],
  "metadata": {
    "fallback": false
  }
}
```

fallback 규칙:

- `bottleneckCandidates`가 있으면 첫 번째 후보를 병목으로 사용한다.
- `itemFlows`에서 소비량이 생산량보다 큰 항목을 suggestion 대상으로 사용한다.
- `powerStatus`가 `near_limit`이면 전력 증설 suggestion을 추가한다.

### 7.2 `quest_generator`

입력 payload:

```json
{
  "progress": {
    "stage": 3,
    "unlockedMachines": [],
    "unlockedRecipes": [],
    "recentlyProducedItems": [],
    "unusedSystems": [],
    "currency": 1200,
    "completedQuestIds": []
  }
}
```

출력 payload:

```json
{
  "quest": {
    "id": "quest_use_trade_001",
    "title": "거래 시스템 사용하기",
    "description": "거래 시스템을 사용해 부족한 자원을 확보하세요.",
    "objective": {},
    "reward": {},
    "reason": "아직 사용하지 않은 시스템입니다."
  },
  "metadata": {
    "fallback": false
  }
}
```

fallback 규칙:

- `unusedSystems`가 있으면 해당 시스템 사용 퀘스트를 생성한다.
- 부족 자원 정보가 있으면 해당 자원 생산 또는 확보 퀘스트를 생성한다.
- 정보가 부족하면 현재 stage 기준 탐색 퀘스트를 생성한다.

### 7.3 `manual_qa`

입력 payload:

```json
{
  "question": "철 부품은 어떻게 만들어?",
  "context": {
    "currentScreen": "factory",
    "selectedMachineId": "assembler_basic",
    "unlockedRecipes": [],
    "unlockedMachines": [],
    "playerStage": 3
  }
}
```

출력 payload:

```json
{
  "answer": "철 부품은 조립기에서 철 가루를 사용해 만들 수 있습니다.",
  "relatedItems": [],
  "relatedMachines": [],
  "suggestedAction": {
    "type": "open_recipe",
    "targetId": "iron_part"
  },
  "metadata": {
    "fallback": false
  }
}
```

fallback 규칙:

- 질문에 포함된 item 또는 machine 키워드를 기반으로 짧은 안내를 생성한다.
- 해금되지 않은 recipe나 machine은 “아직 해금되지 않았을 수 있음”으로 답변한다.
- 질문이 모호하면 현재 화면과 선택 장비 기준으로 다음 행동을 제안한다.

### 7.4 `new_material_generator`

입력 payload:

```json
{
  "machineType": "synthesizer",
  "ruleVersion": "v1",
  "playerStage": 4,
  "knownRecipeExists": false,
  "inputs": [
    {
      "itemId": "iron_powder",
      "amount": 10,
      "properties": ["metal", "powder", "conductive"]
    }
  ]
}
```

출력 payload:

```json
{
  "created": true,
  "material": {
    "id": "conductive_iron_composite",
    "name": "전도성 철 복합재",
    "type": "solid",
    "rarity": "uncommon",
    "properties": ["conductive", "metal"],
    "description": "전도성을 가진 철 기반 신소재입니다."
  },
  "recipe": {
    "id": "recipe_conductive_iron_composite",
    "inputs": [],
    "output": {
      "itemId": "conductive_iron_composite",
      "amount": 1
    }
  },
  "visual": {
    "status": "pending",
    "placeholderIcon": "/assets/materials/placeholders/solid_unknown.png",
    "visualProfile": {
      "baseColor": "#8A8F95",
      "secondaryColor": "#BFC5CC",
      "surface": "porous_metal",
      "pattern": "micro_holes",
      "roughness": 0.6,
      "reflectance": 0.35,
      "transparency": 0.0,
      "glow": false,
      "shapeHint": "compressed_chunk"
    }
  },
  "metadata": {
    "fallback": false
  }
}
```

fallback 규칙:

- `knownRecipeExists`가 true면 `created`를 false로 반환한다.
- 입력 재료 properties를 합쳐 material properties를 만든다.
- material type은 properties에 `gas`, `liquid`, `powder`, `metal`, `crystal`이 있는지 보고 결정한다.
- `visualProfile`은 material type과 rarity를 기반으로 deterministic하게 생성한다.

## 8. Streaming

MVP에서는 `agent.stream`을 선택 기능으로 구현한다. 오래 걸리는 LLM 요청 전후에 진행 상태를 보낼 수 있다.

예시:

```json
{
  "type": "agent.stream",
  "requestId": "req_process_001",
  "agent": "process_optimizer",
  "event": "progress",
  "payload": {
    "message": "생산 요약을 분석 중입니다."
  }
}
```

최종 결과는 항상 `agent.response` 하나로 끝난다.

`visual.ready` 이벤트는 실제 이미지 생성 provider와 storage가 붙은 뒤 후속 단계에서 구현한다. MVP에서는 `new_material_generator` 응답의 `visual.status`를 `pending`으로 반환하는 데까지만 구현한다.

## 9. Cache

MVP cache는 in-memory로 구현한다.

Cache key:

```txt
agent + snapshotHash + normalizedPayloadHash
```

동작:

- `snapshotHash`가 없으면 payload hash만 사용한다.
- cache hit 시 LLM을 호출하지 않고 이전 `agent.response` payload를 반환한다.
- cache는 프로세스 재시작 시 사라진다.
- `manual_qa`는 question이 payload에 포함되므로 별도 key field를 추가하지 않는다.

## 10. Error Handling

| Code | Case |
|---|---|
| `INVALID_JSON` | WebSocket message가 JSON이 아님 |
| `INVALID_MESSAGE` | message가 JSON object가 아님 |
| `VALIDATION_ERROR` | envelope 또는 payload schema 검증 실패 |
| `UNKNOWN_AGENT` | 등록되지 않은 agent id |
| `INTERNAL_ERROR` | fallback으로도 복구할 수 없는 서버 오류 |

LLM provider 오류는 `agent.error`가 아니라 fallback `agent.response`로 복구한다.

## 11. 구현 순서

1. 기존 backend 테스트와 실행 명령을 확인한다.
2. 새 protocol model을 구현한다.
3. `/ws/agent` WebSocket gateway를 구현한다.
4. Agent router와 공통 pipeline을 구현한다.
5. in-memory cache를 구현한다.
6. LLM adapter와 mock adapter를 구현한다.
7. `process_optimizer` schema, prompt, fallback을 구현한다.
8. `quest_generator` schema, prompt, fallback을 구현한다.
9. `manual_qa` schema, prompt, fallback을 구현한다.
10. `new_material_generator` schema, prompt, fallback, visual profile builder를 구현한다.
11. unit test와 WebSocket integration test를 작성한다.
12. README 또는 backend 실행 문서를 새 endpoint 기준으로 갱신한다.

## 12. 테스트 계획

### Unit Tests

- valid envelope validation
- invalid JSON error
- unknown message type error
- missing agent error
- unknown agent error
- agent별 payload validation
- LLM success response validation
- LLM invalid JSON fallback
- LLM timeout fallback
- cache hit/miss
- new material visual profile 생성

### WebSocket Tests

- `ping` 요청 시 `pong` 반환
- `agent.request` 요청 시 `agent.response` 반환
- invalid JSON 요청 시 `agent.error` 반환
- unknown agent 요청 시 `agent.error` 반환
- process optimizer 요청이 병목 suggestion을 반환
- quest generator 요청이 quest를 반환
- manual QA 요청이 answer를 반환
- new material 요청이 material, recipe, visualProfile을 반환

### Acceptance Criteria

- 서버는 `uv run uvicorn factory_space.app:app --host 127.0.0.1 --port 8000`로 실행된다.
- `ws://127.0.0.1:8000/ws/agent`로 접속 가능하다.
- 네 agent id가 모두 정상 라우팅된다.
- LLM API key가 없어도 fallback으로 모든 agent가 정상 응답한다.
- LLM API key가 있으면 LLM 응답을 사용하되 schema 검증을 통과한 결과만 반환한다.
- 서버는 장비 설치, 배치 검증, tick 시뮬레이션 API를 제공하지 않는다.

## 13. 후속 단계

- RAG retriever 연결
- game DB 또는 static content DB 연결
- persistent cache 도입
- request rate limit
- auth/session 정책
- actual image generation provider 연동
- texture/icon storage
- `agent.stream` 기반 `visual.ready` 이벤트
- observability와 tracing
