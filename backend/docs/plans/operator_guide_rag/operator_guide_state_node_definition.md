# operator_guide State/Node 정의 문서

## 목적

이 문서는 operator_guide agent에서 사용하는 `state`와 `node`를 정의한다.

짧게 말하면 다음과 같다.

```text
state = 플레이어 질문을 처리하는 동안 들고 다니는 런타임 데이터 묶음
node = state를 읽고, 필요한 처리를 한 뒤, state를 갱신하는 실행 단계
```

operator_guide는 단순히 질문을 LLM에 바로 보내지 않는다. 질문을 정리하고, 어떤 leaf agent가 처리할지 고르고, 현재 게임 상태가 필요한지 판단하고, RAG 문서를 검색한 뒤, LLM 답변과 Unreal 응답 JSON을 만든다.

이 모든 단계에서 같은 state를 이어받아 갱신한다.

## 전체 흐름

```text
Unreal Input JSON
-> input_normalization_node
-> orchestrator_node
-> operator_guide_entry_node
-> question_type_router_node
-> context_need_classifier_node
-> current_game_state_node
-> rag_retriever_node
-> source_formatter_node
-> llm_answer_generator_node
-> response_validation_node
-> response_builder_node
-> Unreal Output JSON
```

## State 정의

operator_guide state는 한 번의 플레이어 질문을 처리하는 동안 유지되는 데이터다.

대표 필드는 다음과 같다.

| 필드 | 설명 | 예시 |
| --- | --- | --- |
| `request_id` | 요청 단위 식별자 | `unreal-manual-qa-001` |
| `session_id` | 플레이어 대화 세션 식별자 | `player-session-001` |
| `client_id` | 클라이언트 식별자 | `unreal-client` |
| `trace_id` | 서버 내부 추적 ID | `trace-20260611-001` |
| `question` | 플레이어 질문 | `철괴가 안 만들어져. 왜 그래?` |
| `language` | 응답 언어 | `ko` |
| `selected_agent` | 선택된 상위 agent | `operator_guide` |
| `selected_leaf_agent` | operator_guide 내부 질문 유형 | `troubleshooting_question` |
| `question_type` | 질문 분류 결과 | `troubleshooting_question` |
| `context_need` | 현재 게임 상태 필요 여부 | `selected_machine_state_needed` |
| `current_game_state` | 필요한 경우 조회한 현재 상태 | 선택 장비, 전력, 입력 자원 |
| `retrieval_query` | RAG 검색용 query | 정규화된 질문 |
| `retrieved_documents` | pgvector 검색 결과 | manual chunk 목록 |
| `sources` | 최종 응답에 표시할 출처 | 문서 ID, 제목, 점수 |
| `confidence` | 검색/응답 신뢰도 | `high`, `medium`, `low` |
| `llm_prompt` | LLM에 보낼 prompt | system + context + question |
| `llm_answer` | LLM 원문 답변 | 생성된 답변 |
| `final_answer` | Unreal에 보낼 최종 답변 | 검증 후 정리된 답변 |
| `recommended_actions` | Unreal UI가 표시할 추천 액션 | `open_question_guide_tab` |
| `metadata` | 디버깅/관측용 정보 | 모델, latency, retrieval |
| `middleware_logs` | node별 실행 로그 | 시작/종료/오류 |

## State 예시

```json
{
  "request_id": "unreal-manual-qa-001",
  "session_id": "player-session-001",
  "client_id": "unreal-client",
  "trace_id": "trace-20260611-001",
  "question": "철괴가 안 만들어져. 왜 그래?",
  "language": "ko",
  "selected_agent": "operator_guide",
  "selected_leaf_agent": "troubleshooting_question",
  "question_type": "troubleshooting_question",
  "context_need": {
    "required": true,
    "scopes": ["selected_machine", "inventory", "power"],
    "reason": "플레이어가 현재 제작 실패 원인을 물었기 때문에 선택 장비와 자원 상태가 필요하다."
  },
  "current_game_state": {
    "selected_machine": {
      "id": "smelter_001",
      "name": "제련기",
      "power": "off",
      "recipe": "iron_ingot"
    },
    "inventory": {
      "iron_ore": 0
    }
  },
  "retrieval": {
    "top_score": 0.86,
    "matched_documents": 3
  },
  "confidence": "medium",
  "final_answer": null
}
```

## Node 정의

node는 state를 받아서 특정 일을 수행하고, 결과를 state에 추가하는 단계다.

### 1. input_normalization_node

역할:

- Unreal input JSON을 읽는다.
- 질문, 언어, session 정보, 선택 객체 정보를 정리한다.
- `trace_id`를 생성한다.

읽는 state:

- `request_id`
- `session_id`
- `client_id`
- `payload.question`
- `context`

쓰는 state:

- `question`
- `language`
- `trace_id`
- `input_context`

### 2. orchestrator_node

역할:

- 질문을 어떤 상위 agent가 처리할지 판단한다.
- operator_guide NPC 대화에서 직접 들어온 요청이면 `operator_guide`를 유지한다.

읽는 state:

- `question`
- `input_context`
- optional `agent`

쓰는 state:

- `selected_agent`

### 3. operator_guide_entry_node

역할:

- operator_guide agent로 들어온 요청인지 확인한다.
- operator_guide가 처리할 수 있는 범위인지 이후 node에서 판단할 수 있게 기본 metadata를 준비한다.

읽는 state:

- `selected_agent`
- `question`

쓰는 state:

- `agent_started_at`
- `metadata.selectedAgent`

### 4. question_type_router_node

역할:

- operator_guide 내부에서 질문 유형을 분리한다.
- 이 판단은 룰베이스 고정 답변이 아니라 LLM/분류 로직을 통해 수행한다.

질문 유형:

```text
equipment_question
resource_question
recipe_question
troubleshooting_question
progression_question
unknown_question
```

읽는 state:

- `question`
- `language`
- `input_context`

쓰는 state:

- `question_type`
- `selected_leaf_agent`

### 5. context_need_classifier_node

역할:

- 현재 게임 상태가 필요한 질문인지 판단한다.
- 단순 매뉴얼 질문이면 현재 상태 조회를 생략한다.
- 문제 해결 질문이면 필요한 상태 범위를 정한다.

예시:

```text
"기어는 어떻게 만들어?" -> current state 필요 없음
"철괴가 안 만들어져. 왜 그래?" -> selected machine, inventory, power 필요
```

읽는 state:

- `question`
- `question_type`
- `input_context`

쓰는 state:

- `context_need.required`
- `context_need.scopes`
- `context_need.reason`

### 6. current_game_state_node

역할:

- `context_need.required = true`일 때만 실행한다.
- Unreal 또는 게임 서버에서 현재 선택 장비, 인벤토리, 전력, 생산 라인 상태를 조회한다.

읽는 state:

- `context_need`
- `input_context`

쓰는 state:

- `current_game_state`

### 7. rag_retriever_node

역할:

- 질문과 질문 유형을 기반으로 RAG 검색 query를 만든다.
- PostgreSQL + pgvector에서 관련 매뉴얼 문서를 검색한다.

읽는 state:

- `question`
- `question_type`
- `selected_leaf_agent`
- `current_game_state`

쓰는 state:

- `retrieval_query`
- `retrieved_documents`
- `retrieval.top_score`
- `retrieval.matched_documents`

### 8. source_formatter_node

역할:

- 검색 결과를 최종 응답에 넣을 수 있는 출처 형식으로 정리한다.
- confidence 계산에 필요한 신호를 정리한다.

읽는 state:

- `retrieved_documents`
- `retrieval`

쓰는 state:

- `sources`
- `confidence`
- `confidence_reason`

### 9. llm_answer_generator_node

역할:

- system prompt, 질문, RAG 검색 결과, 필요한 경우 현재 게임 상태를 합쳐 LLM prompt를 만든다.
- LLM이 CSV/RAG 기반 근거를 사용해 답변하도록 한다.

읽는 state:

- `question`
- `question_type`
- `sources`
- `retrieved_documents`
- `current_game_state`
- `confidence`

쓰는 state:

- `llm_prompt`
- `llm_answer`
- `llm_provider`
- `llm_model`

### 10. response_validation_node

역할:

- LLM 답변이 지원 범위를 벗어나지 않았는지 확인한다.
- 출처가 없는 내용을 단정하지 않도록 점검한다.
- 범위 밖 질문이면 질문 가이드 액션을 붙인다.

읽는 state:

- `llm_answer`
- `sources`
- `confidence`
- `question_type`

쓰는 state:

- `validated_answer`
- `validation_warnings`
- `recommended_actions`

### 11. response_builder_node

역할:

- Unreal에 보낼 최종 JSON을 만든다.
- 답변, 출처, confidence, 추천 액션, metadata를 정리한다.

읽는 state:

- `validated_answer`
- `sources`
- `confidence`
- `recommended_actions`
- `metadata`

쓰는 state:

- `final_answer`
- `output_json`

## 단순 질문과 현재 상태 질문 차이

### 현재 상태가 필요 없는 질문

```json
{
  "question": "기어는 어떻게 만들어?",
  "context_need": {
    "required": false,
    "scopes": [],
    "reason": "제작법 설명은 매뉴얼/RAG 검색만으로 답할 수 있다."
  }
}
```

실행 흐름:

```text
question_type_router_node
-> context_need_classifier_node
-> rag_retriever_node
-> source_formatter_node
-> llm_answer_generator_node
-> response_builder_node
```

### 현재 상태가 필요한 질문

```json
{
  "question": "철괴가 안 만들어져. 왜 그래?",
  "context_need": {
    "required": true,
    "scopes": ["selected_machine", "inventory", "power"],
    "reason": "제작 실패 원인은 현재 장비와 자원 상태가 있어야 진단할 수 있다."
  }
}
```

실행 흐름:

```text
question_type_router_node
-> context_need_classifier_node
-> current_game_state_node
-> rag_retriever_node
-> source_formatter_node
-> llm_answer_generator_node
-> response_builder_node
```

## Unreal Input/Output 연결

Unreal은 JSON으로 질문을 보낸다.

```json
{
  "type": "agent.request",
  "request_id": "unreal-manual-qa-001",
  "session_id": "player-session-001",
  "client_id": "unreal-client",
  "agent": "operator_guide",
  "payload": {
    "question": "철괴가 안 만들어져. 왜 그래?"
  },
  "context": {
    "language": "ko",
    "selected_object": {
      "type": "machine",
      "id": "smelter_001",
      "name": "제련기"
    }
  }
}
```

operator_guide는 최종적으로 다음 형태의 JSON을 반환한다.

```json
{
  "type": "agent.response",
  "request_id": "unreal-manual-qa-001",
  "session_id": "player-session-001",
  "client_id": "unreal-client",
  "agent": "operator_guide",
  "payload": {
    "final_answer": "철괴가 안 만들어질 때는 제련기의 전력, 입력 철광석, 출력 공간, 레시피 설정을 먼저 확인해 주세요.",
    "question_type": "troubleshooting_question",
    "sources": [],
    "confidence": "medium",
    "recommended_actions": [],
    "metadata": {
      "selectedAgent": "operator_guide",
      "selectedLeafAgent": "troubleshooting_question",
      "contextNeed": {
        "required": true,
        "scopes": ["selected_machine", "inventory", "power"]
      }
    }
  }
}
```

## 로그 기준

로그는 node 단위로 남긴다.

```json
{
  "node": "rag_retriever_node",
  "event": "retrieval_completed",
  "details": {
    "top_score": 0.86,
    "matched_documents": 3
  }
}
```

권장 이벤트:

| event | 설명 |
| --- | --- |
| `node_started` | node 실행 시작 |
| `node_finished` | node 실행 완료 |
| `node_failed` | node 실행 실패 |
| `routing_decided` | agent 또는 leaf agent 선택 완료 |
| `context_need_decided` | 현재 상태 필요 여부 판단 완료 |
| `game_state_loaded` | 현재 게임 상태 조회 완료 |
| `retrieval_completed` | RAG 검색 완료 |
| `llm_invoked` | LLM 호출 시작 |
| `llm_completed` | LLM 응답 완료 |
| `response_validated` | 응답 검증 완료 |

## 설계 원칙

- state는 node 사이에서 공유되는 데이터이며, node는 자기 책임에 맞는 필드만 갱신한다.
- node는 가능한 한 작게 유지한다.
- 현재 게임 상태는 필요한 질문에서만 조회한다.
- RAG 검색 결과가 부족하면 LLM이 추측하지 않게 한다.
- confidence는 LLM이 임의로 정하지 않고 검색 결과와 검증 신호를 기반으로 backend가 계산한다.
- Unreal에 필요한 최종 응답은 `response_builder_node`에서만 구성한다.

## 작업 로그

- 2026-06-11: operator_guide agent 기준 state/node 정의 문서를 추가했다.

## 트러블슈팅 로그

- 2026-06-11: state와 node가 추상적으로 보이는 문제를 줄이기 위해 필드 표, node별 읽기/쓰기 state, Unreal JSON 예시를 함께 정리했다.
