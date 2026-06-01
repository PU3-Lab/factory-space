# `backend/src` 폴더 역할

이 문서는 새 서버 구조에서 `backend/src` 아래 각 폴더가 맡는 책임을 정리한다.
Agent별 세부 책임은 `AGENT_ROLES.md`에 둔다.

## 최상위 구성

```txt
backend/src
 ├─ app.py
 ├─ websocket_gateway
 ├─ protocol
 ├─ agents
 ├─ llm
 ├─ cache
 └─ visual
```

## `app.py`

FastAPI application entrypoint다.

역할:

- FastAPI app 생성
- `GET /health` 등록
- WebSocket router 등록
- uvicorn 실행 대상 app 제공

하지 않는 일:

- Agent 실행 로직
- LLM 호출
- 요청 payload 검증 세부 로직

## `websocket_gateway`

WebSocket transport 계층이다.

역할:

- `/ws/agent` endpoint 제공
- WebSocket 연결 accept/close 관리
- raw text message 수신
- JSON parse 결과를 protocol/pipeline 계층으로 전달
- `agent.stream`, `agent.response`, `agent.error`, `pong` 송신

하지 않는 일:

- Agent별 비즈니스 로직
- LLM prompt 생성
- fallback 생성
- cache 판단

대표 파일:

- `gateway.py`: WebSocket endpoint
- `connection.py`: 연결 관리 helper

## `protocol`

클라이언트와 서버가 주고받는 public message schema 계층이다.

역할:

- `agent.request` envelope 정의
- `agent.stream` envelope 정의
- `agent.response` envelope 정의
- `agent.error` envelope 정의
- `ping` / `pong` message 정의
- protocol error code와 error payload 정의

하지 않는 일:

- Agent routing
- LLM 호출
- Agent별 domain payload 해석

대표 파일:

- `messages.py`: WebSocket message model
- `errors.py`: error code와 error payload model

## `agents`

AI Agent 실행 계층이다.

역할:

- Agent 공통 interface 정의
- Agent registry와 router 제공
- 공통 Agent pipeline 제공
- Agent별 payload schema 검증
- Agent별 prompt 생성
- Agent별 LLM response parsing
- Agent별 deterministic fallback 생성

포함 Agent:

- `orchestrator`
- `process_optimizer`
- `quest_generator`
- `operator_guide`
- `new_material_generator`

대표 파일:

- `base.py`: Agent interface와 공통 타입
- `router.py`: agent id 기반 registry/router
- `agent_catalog.py`: top-level routing prompt에 넣는 read-only routing support tool과 Agent capability catalog
- `pipeline/`: LangGraph 실행 파이프라인 패키지
  - `runtime.py`: validation/cache/prompt/response envelope 실행 흐름
  - `graph_edges.py`: LangGraph node edge와 routing predicate
  - `llm_fallback.py`: default/fallback1/fallback2 LLM slot 호출
  - `state.py`: graph state 타입
  - `utils.py`: cache key, fallback, validation error helper
- `orchestrator.py`: 요청 의도 분석과 전문 Agent 선택을 담당하는 오케스트레이터 Agent
- `process_optimizer.py`: 공정 최적화 Agent
- `quest_generator/`: 퀘스트 생성 Agent와 서브 에이전트
- `operator_guide/`: 운영자 가이드 Agent와 서브 에이전트
- `new_material_generator.py`: 신물질 생성 Agent

오케스트레이터 수:

- 서버 전체 오케스트레이터 Agent는 `orchestrator.py` 1개다.
- 도메인 오케스트레이터는 `operator_guide/agent.py`, `quest_generator/agent.py` 2개다.
- `pipeline/`은 실행 파이프라인이고, `router.py`는 registry/router이므로 오케스트레이터가 아니다.
- `operator_guide/agent.py`와 `quest_generator/agent.py`는 각 도메인 내부에서 서브 에이전트 선택, payload 정리, 결과 정규화를 맡는 도메인 오케스트레이터다.
- top-level Agent 중 `process_optimizer`, `new_material_generator`는 현재 내부 서브 에이전트가 없으므로 leaf Agent로 처리한다.
- top-level Agent라도 내부 서브 에이전트나 실행 전략을 다시 선택해야 하는 도메인은 `operator_guide`, `quest_generator`처럼 도메인 오케스트레이터로 분리한다.

공식 용어:

- `Global Orchestrator`: 서버 전체 오케스트레이터
- `Domain Orchestrator`: 도메인 오케스트레이터, 즉 중간 계층에서 하위 Agent를 고르는 Agent
- `Leaf Agent`: 하위 Agent를 고르지 않는 실행 Agent

`operator_guide/` 내부:

- `agent.py`: 운영자 가이드 요청을 서브 에이전트로 분기하는 도메인 오케스트레이터
- `recipe_explainer.py`: 레시피 설명 서브 에이전트
- `machine_help.py`: 장비 도움말 서브 에이전트
- `troubleshooter.py`: 문제 해결 서브 에이전트

`quest_generator/` 내부:

- `agent.py`: 퀘스트 생성 요청을 서브 에이전트로 분기하는 도메인 오케스트레이터
- `tutorial_quest.py`: 튜토리얼 퀘스트 서브 에이전트
- `production_quest.py`: 생산 퀘스트 서브 에이전트
- `exploration_quest.py`: 탐험 퀘스트 서브 에이전트
- `economy_quest.py`: 경제 퀘스트 서브 에이전트

`quest_generator.route_sub_agent` 역할:

- 서브 에이전트를 구분하는 주체는 도메인 오케스트레이터인 `agents/quest_generator/agent.py`다.
- `route_sub_agent`는 별도 Agent가 아니라 `quest_generator` 도메인 오케스트레이터가 제공하는 routing 동작을 LangGraph 노드 이름으로 표현한 것이다.
- LangGraph에서는 `quest_generator` 최상위 Agent가 선택된 뒤 이 내부 결정 함수를 호출한다.
- 입력은 이미 envelope 검증을 통과한 퀘스트 생성 payload와 player/session context다.
- 출력은 모델 raw decision을 trim한 `selectedLeafAgent` 값이다.
- `route_sub_agent` 노드는 허용 목록 검증을 직접 완료하지 않고, 이후 `route_selected_leaf_agent` conditional edge가 허용 leaf Agent id인지 검증한다.
- 이 단계는 퀘스트를 직접 생성하지 않고, 어떤 퀘스트 생성 서브 에이전트가 요청을 처리할지만 결정한다.
- 판단 기준은 플레이어 진행도, 현재 공장 상태, 최근 이벤트, 명시된 quest type, 튜토리얼 필요 여부다.
- 선택 prompt와 raw decision은 observability와 debugging을 위해 graph state에 남긴다.

## `llm`

LLM provider adapter 계층이다.

역할:

- OpenAI-compatible LLM 호출 adapter 제공
- API key가 없을 때 fallback 경로로 전환할 수 있는 no-op adapter 제공
- 테스트용 fake adapter 제공
- LLM timeout과 provider error를 pipeline이 처리할 수 있는 형태로 변환
- LLM slot provider/model/base URL/API key 설정 해석

하지 않는 일:

- WebSocket message 송신
- Agent별 fallback 판단
- Agent별 prompt 구성
- 클라이언트 응답 envelope 생성

대표 파일:

- `adapter.py`: LLM adapter interface 및 provider 구현
- `settings.py`: LLM slot 설정 해석

## `cache`

Agent 응답 cache 계층이다.

역할:

- in-memory response cache 제공
- `agent + leaf_agent + payload + session_id + client_id + context.metadata` 기반 cache key 생성 지원
- cache hit/miss 판단
- cache hit metadata 구성 지원

MVP 제약:

- 프로세스 재시작 시 cache는 사라진다.
- persistent cache는 후속 단계에서 구현한다.

대표 파일:

- `response_cache.py`: in-memory response cache

## `visual`

신물질 비주얼 프로파일 생성 계층이다.

역할:

- `new_material_generator`가 사용할 `visualProfile` 생성
- material type, rarity, properties 기반 색상/표면/패턴/형태 힌트 결정
- placeholder visual 응답 구성 지원

MVP 제약:

- 실제 이미지 생성은 하지 않는다.
- texture/icon 파일 저장소는 다루지 않는다.
- `visual.ready` 이벤트는 후속 단계에서 구현한다.

대표 파일:

- `profile.py`: material visual profile builder

## 의존 방향

권장 의존 방향:

```txt
websocket_gateway -> protocol
websocket_gateway -> agents.pipeline
agents.pipeline -> agents.router
agents.pipeline -> cache
agents.pipeline -> llm
agents.orchestrator -> agents.router
agents.new_material_generator -> visual
```

피해야 할 의존:

- `protocol`이 `agents`를 import하지 않는다.
- `llm`이 `websocket_gateway`를 import하지 않는다.
- `cache`가 Agent별 구현을 import하지 않는다.
- Agent module이 WebSocket connection object를 직접 다루지 않는다.
- `agents/orchestrator.py`는 실행 흐름을 직접 소유하지 않고, 어떤 전문 Agent를 사용할지 결정하는 역할만 맡는다.

## 구현 원칙

- transport, protocol, pipeline, agent domain logic을 분리한다.
- LLM raw text는 클라이언트로 직접 보내지 않는다.
- Agent별 최종 payload는 항상 schema validation을 통과해야 한다.
- 최상위 routing 실패는 fallback 전에 `agent.error` / `ROUTING_UNAVAILABLE`로 종료한다.
- Agent 실행 단계의 LLM slot 실패는 fallback `agent.response`로 복구한다.
- 요청 envelope, payload, routing, fallback schema가 잘못된 경우에는 `agent.error`를 반환할 수 있다.

## LangGraph 구성도

LangGraph는 `agents/pipeline/` 내부 실행 흐름을 표현하는 데 사용한다. WebSocket 연결 자체는 LangGraph에 넣지 않고, 검증된 `agent.request` 하나를 graph input으로 넣는다.

```mermaid
flowchart TD
    Start([START]) --> BuildContext[build_context]
    BuildContext --> ValidateEnvelope[validate_envelope]
    ValidateEnvelope --> RouteTopAgent[route_top_agent]
    RouteTopAgent --> RouteSelectedAgent{route_selected_agent}

    RouteSelectedAgent -->|process_optimizer| ProcessPayload[validate_process_payload]
    RouteSelectedAgent -->|operator_guide| OperatorGuideRoute[operator_guide.route_sub_agent]
    RouteSelectedAgent -->|quest_generator| QuestRoute[quest_generator.route_sub_agent]
    RouteSelectedAgent -->|new_material_generator| MaterialPayload[validate_material_payload]
    RouteSelectedAgent -->|unknown / invalid| ErrorNode[build_agent_error]

    ProcessPayload --> ProcessLeafResult{route_selected_leaf_agent}
    OperatorGuideRoute --> OperatorGuideLeafResult{route_selected_leaf_agent}
    QuestRoute --> QuestLeafResult{route_selected_leaf_agent}
    MaterialPayload --> MaterialLeafResult{route_selected_leaf_agent}

    ProcessLeafResult -->|valid| CacheLookup[cache_lookup]
    OperatorGuideLeafResult -->|valid| CacheLookup
    QuestLeafResult -->|valid| CacheLookup
    MaterialLeafResult -->|valid| CacheLookup
    ProcessLeafResult -->|error| ErrorNode
    OperatorGuideLeafResult -->|error| ErrorNode
    QuestLeafResult -->|error| ErrorNode
    MaterialLeafResult -->|error| ErrorNode

    CacheLookup --> CacheDecision{cache_hit?}
    CacheDecision -->|yes| BuildCachedResponse[build_cached_response]
    CacheDecision -->|no| AgentBefore[agent.middleware.before]

    AgentBefore --> BuildPrompt[build_prompt]
    BuildPrompt --> DefaultLLM[call_llm.default]
    DefaultLLM --> DefaultDecision{default valid?}
    DefaultDecision -->|yes| ParseLLM[parse_llm_response]
    DefaultDecision -->|no| Fallback1LLM[call_llm.fallback1]
    Fallback1LLM --> Fallback1Decision{fallback1 valid?}
    Fallback1Decision -->|yes| ParseLLM
    Fallback1Decision -->|no| Fallback2LLM[call_llm.fallback2]
    Fallback2LLM --> Fallback2Decision{fallback2 valid?}
    Fallback2Decision -->|yes| ParseLLM
    Fallback2Decision -->|no| AgentFallback[agent.middleware.fallback]
    ParseLLM --> ValidateResponse[validate_response_schema]
    AgentFallback --> ValidateResponse

    ValidateResponse --> ResponseDecision{response_valid?}
    ResponseDecision -->|yes| CacheWrite[cache_write]
    ResponseDecision -->|no| ErrorNode

    CacheWrite --> AgentAfter[agent.middleware.after]
    AgentAfter --> BuildResponse[build_agent_response]
    BuildCachedResponse --> BuildResponse
    BuildResponse --> End
    ErrorNode --> End
```

### LangGraph State

`route_selected_leaf_agent`는 `selectedLeafAgent`가 선택된 top-level Agent의 허용 leaf Agent id인지 확인하는 공통 validity/error edge다. `operator_guide`, `quest_generator`는 도메인 오케스트레이터 prompt 결과를 여기서 검증하고, `process_optimizer`, `new_material_generator`는 leaf top-level Agent라서 payload validation 결과를 이 공통 edge에 넘긴다.

```txt
AgentGraphState
 ├─ envelope
 ├─ context
 ├─ selectedAgent
 ├─ selectedLeafAgent
 ├─ typedPayload
 ├─ cacheKey
 ├─ cachedPayload
 ├─ cachedMetadata
 ├─ prompt
 ├─ routingPrompt
 ├─ routingRaw
 ├─ llmRaw
 ├─ llmSlot
 ├─ llmProvider
 ├─ llmModel
 ├─ fallbackReason
 ├─ middlewareLogs
 ├─ responsePayload
 ├─ responseMetadata
 ├─ responseEnvelope
 ├─ streams
 └─ error
```

### 노드 책임

- `build_context`: `requestId`, `agent`, `snapshotType`, `snapshotHash`로 실행 context를 만든다.
- `validate_envelope`: public message envelope를 검증한다.
- `route_top_agent`: 최상위 Agent 선택 prompt를 호출하고 `selectedAgent` state를 기록한다. 명시 `agent`는 prompt hint일 뿐 직접 선택 shortcut으로 쓰지 않는다.
- `route_selected_agent`: `selectedAgent` state를 보고 LangGraph conditional edge로 최상위 Agent 경로를 나눈다.
- `operator_guide.route_sub_agent`: structured prompt를 호출하고 raw decision을 `selectedLeafAgent` state에 기록한다.
- `quest_generator.route_sub_agent`: structured prompt를 호출하고 raw decision을 `selectedLeafAgent` state에 기록한다.
- `route_selected_leaf_agent`: `selectedLeafAgent`가 선택된 top-level Agent의 허용 leaf Agent id인지 검증하고 다음 실행 단계와 error 경로를 나눈다.
- `cache_lookup`: cache key로 이전 response payload를 찾는다.
- `build_prompt`: 선택된 Agent 또는 sub-agent prompt를 만든다.
- `call_llm.default`: 기본 LLM slot adapter를 호출한다.
- `call_llm.fallback1`: 기본 slot이 응답하지 못했을 때 1차 fallback slot adapter를 호출한다.
- `call_llm.fallback2`: fallback1 slot이 응답하지 못했을 때 2차 fallback slot adapter를 호출한다.
- `agent.middleware.before`: cache miss 이후 selected leaf Agent 실행 직전 middleware log를 남긴다.
- `agent.middleware.fallback`: LLM 실패 또는 API key 없음 상태에서 기존 deterministic fallback을 실행하고 middleware log를 남긴다.
- `agent.middleware.after`: response schema 검증과 cache write 이후 middleware log를 남긴다.
- `validate_response_schema`: 최종 payload가 Agent response schema를 만족하는지 검증한다.
- `cache_write`: 검증된 payload를 cache에 저장한다.
- `build_agent_response`: `agent.response` envelope를 만든다.
- `build_agent_error`: `agent.error` envelope를 만든다.
