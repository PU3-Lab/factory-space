# 퀘스트 에이전트 서비스 구현 계획

## 상태

구현 완료 후 작성한 사후 계획 문서입니다. 실제 구현된 계약, 작업 순서, 검증 결과가 계획과 맞는지 확인하기 위해 작성했습니다.

## 목표

Unreal에서 퀘스트 버튼을 눌렀을 때 `agent: "quest_generator"`가 포함된 `agent.request` JSON을 보내면, backend가 서버 내부 예제 퀘스트 풀에서 5개를 랜덤으로 선택해 JSON으로 반환합니다.

이번 범위에서는 Unreal이 창고 상태나 `game_state`를 보내지 않습니다. 퀘스트 요청 신호는 JSON 요청 자체로 판단합니다.

## 최종 요청 계약

퀘스트 버튼 요청에서는 `payload`를 생략할 수 있습니다.

```json
{
  "type": "agent.request",
  "request_id": "request-quest",
  "session_id": "session-1",
  "client_id": "unreal-client",
  "agent": "quest_generator"
}
```

정리:

- `payload` 필드는 다른 agent가 쓰는 공통 protocol 필드이므로 제거하지 않습니다.
- 퀘스트 생성 요청에서는 `payload`가 없거나 비어 있으면 퀘스트 5개 요청으로 처리합니다.
- Unreal이 mock `game_state`나 창고 상태를 보내지 않아도 됩니다.

## 최종 응답 계약

backend는 단일 `payload.quest`가 아니라 `payload.quests` 배열을 반환합니다.

```json
{
  "type": "agent.response",
  "request_id": "request-quest",
  "session_id": "session-1",
  "client_id": "unreal-client",
  "agent": "quest_generator",
  "payload": {
    "quests": [
      {
        "id": 1,
        "type": "production",
        "title": "철광석 10개 채굴",
        "description": "기초 생산 라인을 준비하기 위해 철광석 10개를 채굴하세요.",
        "objectives": [
          {
            "target_item_id": "iron_ore",
            "quantity": 10
          }
        ]
      }
    ],
    "metadata": {
      "fallback": true,
      "sub_agent": "quest_generator.production_quest",
      "selectedAgent": "quest_generator",
      "selectedLeafAgent": "quest_generator.production_quest"
    }
  },
  "streams": []
}
```

## 예제 퀘스트 풀

초기 예제 퀘스트는 6개입니다.

- `1`: 철광석 10개 채굴
- `2`: 구리광석 8개 채굴
- `3`: 철판 5개 생산
- `4`: 구리선 12개 생산
- `5`: 석탄 6개 확보
- `6`: 기어 4개 제작

서비스는 이 6개 중 중복 없이 5개를 랜덤으로 선택해 반환합니다.

## 구현 계획

### Task 1. 퀘스트 스키마 변경

대상 파일:

- `backend/src/agents/quest_generator/schemas.py`
- `backend/tests/test_quest_agent_service.py`

작업:

- [x] 응답 payload를 `quest`에서 `quests`로 변경합니다.
- [x] quest `id`를 문자열에서 양의 정수로 변경합니다.
- [x] objective에서 `action`을 제거합니다.
- [x] objective에서 `target_item_name`을 제거합니다.
- [x] objective에는 `target_item_id`, `quantity`만 유지합니다.
- [x] Pydantic으로 `quantity > 0`을 검증합니다.

검증:

- [x] `test_service_returns_five_random_quests_from_example_pool`
- [x] `test_service_quest_objectives_keep_item_id_and_quantity_only`
- [x] `test_quest_response_rejects_invalid_quantity`

### Task 2. 퀘스트 서비스 변경

대상 파일:

- `backend/src/agents/quest_generator/service.py`
- `backend/src/agents/quest_generator/production_quest.py`

작업:

- [x] 클라이언트가 보내는 `game_state` 의존을 제거합니다.
- [x] 서버 내부 예제 퀘스트 풀을 추가합니다.
- [x] 예제 6개 중 5개를 랜덤 선택합니다.
- [x] 선택된 퀘스트를 Pydantic으로 검증합니다.
- [x] `model_dump(mode="json")`으로 JSON 직렬화 가능한 dict를 반환합니다.
- [x] `ProductionQuestAgent.fallback()`이 변경된 서비스를 호출하도록 연결합니다.

검증:

- [x] `test_production_quest_fallback_returns_five_example_quests`
- [x] 기존 quest leaf agent fallback 테스트 통과

### Task 3. payload 없는 퀘스트 요청 라우팅

대상 파일:

- `backend/src/agents/pipeline/runtime.py`
- `backend/src/agents/pipeline/graph_edges.py`
- `backend/src/agents/pipeline/state.py`
- `backend/tests/test_message_router.py`
- `backend/tests/test_pipeline_edges.py`

작업:

- [x] `agent == "quest_generator"`이고 `payload`가 없거나 비어 있으면 직접 퀘스트 요청으로 처리합니다.
- [x] 해당 요청은 `quest_generator.production_quest`로 라우팅합니다.
- [x] 퀘스트 버튼 요청 경로에서는 top-level routing LLM을 호출하지 않습니다.
- [x] 퀘스트 버튼 요청 경로에서는 generation LLM도 호출하지 않고 deterministic fallback을 사용합니다.
- [x] 다른 agent 요청의 기존 prompt 기반 라우팅은 유지합니다.

검증:

- [x] `test_pipeline_routes_empty_quest_request_without_llm`
- [x] 기존 prompt-routed quest 테스트 통과
- [x] 기존 invalid sub-agent 테스트 통과

### Task 4. Smoke runner 변경

대상 파일:

- `backend/scripts/smoke_agent_pipeline.py`
- `backend/tests/test_smoke_agent_pipeline_script.py`

작업:

- [x] quest smoke 요청에서 `payload.game_state`를 제거합니다.
- [x] quest smoke 요청에서 `payload.sub_agent`를 제거합니다.
- [x] 단일 quest id 대신 `payload.quests` 개수를 검증합니다.
- [x] 기존 smoke profile과 provider opt-in 동작은 유지합니다.

검증:

- [x] `test_local_profile_exercises_all_agent_paths`
- [x] `test_response_validation_rejects_wrong_quest_count`
- [x] `scripts/smoke_agent_pipeline.py none --base-url http://127.0.0.1:8012`
- [x] 직접 WebSocket 퀘스트 요청으로 5개 quest 응답 확인

## 구현 결과 대조

| 요구사항 | 결과 | 근거 |
| --- | --- | --- |
| Unreal 퀘스트 요청에서 payload가 필수가 아니어야 함 | 충족 | 빈 payload 요청 테스트, 직접 WebSocket smoke |
| 공통 protocol의 payload는 유지해야 함 | 충족 | `AgentRequestEnvelope`는 유지 |
| backend가 5개 퀘스트를 반환해야 함 | 충족 | service, pipeline, smoke 테스트 |
| quest id는 정수형이어야 함 | 충족 | service 테스트, 직접 WebSocket smoke assertion |
| objective에서 action 제거 | 충족 | schema, service 테스트 |
| objective에서 이름 제거, id만 유지 | 충족 | schema, service 테스트 |
| 예제 퀘스트 풀은 6개 | 충족 | `QuestAgentService` 예제 풀 |
| 기존 라우팅 경로가 깨지지 않아야 함 | 충족 | 전체 backend 테스트 |

## 최종 검증

실행한 명령:

- `python -m pytest tests`
- `python -m ruff check src tests scripts`
- `python scripts/smoke_agent_pipeline.py none --base-url http://127.0.0.1:8012`
- `ws://127.0.0.1:8012/ws/agent`로 직접 WebSocket 퀘스트 요청

결과:

- `105 passed`
- `All checks passed!`
- `PASS none/health`
- `PASS none/invalid_json`
- `PASS none/invalid_envelope`
- `PASS none/routing_unavailable`
- 직접 WebSocket quest 응답에서 정수형 id를 가진 quest 5개 반환 확인

## 관련 커밋

- `d5f53a2 퀘스트 에이전트 서비스 추가`
- `6dfe854 퀘스트 5개 랜덤 응답 계약 반영`
