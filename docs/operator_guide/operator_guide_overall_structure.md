# operator_guide 전체 구조 안내서

이 문서는 `operator_guide` Agent가 프로젝트 안에서 어떤 구조로 동작하는지 빠르게 이해하기 위한 문서다.

특히 `material_generation`에는 `graph.py`, `nodes.py`가 있는데 `operator_guide`에는 왜 없는지 설명한다.

## 핵심 결론

`operator_guide`에는 별도 `graph.py`, `nodes.py`가 없어도 된다.

현재 프로젝트에는 Agent를 만드는 방식이 두 가지 있다.

```text
1. 전용 그래프형 Agent
   - material_generation
   - 자기 폴더 안에 graph.py, nodes.py, graph_state.py를 가진다.

2. 공통 파이프라인 탑승형 Agent
   - operator_guide
   - 공통 AgentPipeline의 node/state/edge를 사용한다.
   - 자기 폴더 안에는 routing, leaf agent, service, RAG, prompt 로직을 가진다.
```

즉, `operator_guide`는 그래프가 없는 것이 아니라 **공통 그래프 안에서 실행되는 구조**다.

## 전체 실행 흐름

플레이어 질문이 들어오면 전체 흐름은 다음과 같다.

```text
Unreal 또는 agent-test
-> WebSocket /ws/agent
-> AgentRequestEnvelope 검증
-> 공통 AgentPipeline 실행
-> top-level agent routing
-> operator_guide 선택
-> operator_guide 내부 leaf agent routing
-> machine_help / recipe_explainer / troubleshooter 선택
-> ManualQAService 실행
-> CSV evidence + RAG 검색 + 현재 게임 상태 확인
-> prompt 구성
-> LLM 답변 생성
-> 응답 검증 / 캐시 / 메모리 저장
-> agent.response 반환
```

Mermaid로 보면 다음과 같다.

```mermaid
flowchart TD
    A[Unreal / agent-test] --> B[/ws/agent]
    B --> C[AgentPipeline]
    C --> D[build_context]
    D --> E[validate_envelope]
    E --> F[top-level routing]
    F --> G{selectedAgent}
    G -->|operator_guide| H[operator_guide.route_sub_agent]
    G -->|other agent| Z[other agent]
    H --> I{selectedLeafAgent}
    I -->|machine_help| J[MachineHelpAgent]
    I -->|recipe_explainer| K[RecipeExplainerAgent]
    I -->|troubleshooter| L[TroubleshooterAgent]
    J --> M[ManualQAService]
    K --> M
    L --> M
    M --> N[CSV / RAG / current_game_state]
    N --> O[Prompt Builder]
    O --> P[LLM]
    P --> Q[Response Validation]
    Q --> R[agent.response]
```

## material_generation 구조

`material_generation`은 자기만의 LangGraph를 직접 구성하는 전용 그래프형 Agent다.

주요 파일:

```text
backend/src/agents/material_generation/graph.py
backend/src/agents/material_generation/nodes.py
backend/src/agents/material_generation/graph_state.py
backend/src/agents/material_generation/router.py
backend/src/agents/material_generation/agent.py
```

역할:

```text
graph.py
- material_generation 전용 StateGraph를 만든다.
- 어떤 node를 어떤 순서로 실행할지 정의한다.

nodes.py
- 그래프 안에서 실행되는 개별 node 함수들을 담는다.
- 예: 입력 정규화, 후보 생성, 검증, 최종 결과 생성.

graph_state.py
- material_generation 그래프에서 공유하는 state 타입을 정의한다.
```

이 구조는 특정 Agent가 독립적인 작업 흐름을 많이 가지고 있을 때 적합하다.

예를 들어 material_generation은 재료 생성, 후보 검증, 유사도 확인, 결과 정리처럼 자체 단계가 많기 때문에 전용 graph와 nodes를 가진다.

## operator_guide 구조

`operator_guide`는 전용 `graph.py`, `nodes.py` 대신 공통 AgentPipeline을 사용한다.

공통 그래프 파일:

```text
backend/src/agents/pipeline/runtime.py
backend/src/agents/pipeline/graph_edges.py
backend/src/agents/pipeline/state.py
backend/src/agents/pipeline/middleware.py
backend/src/agents/pipeline/llm_fallback.py
backend/src/agents/pipeline/tool_node.py
backend/src/agents/pipeline/utils.py
```

operator_guide 고유 파일:

```text
backend/src/agents/operator_guide/agent.py
backend/src/agents/operator_guide/machine_help.py
backend/src/agents/operator_guide/recipe_explainer.py
backend/src/agents/operator_guide/troubleshooter.py
backend/src/agents/operator_guide/service.py
backend/src/agents/operator_guide/prompt_builder.py
backend/src/agents/operator_guide/system_prompt.py
backend/src/agents/operator_guide/rag_retriever.py
backend/src/agents/operator_guide/rag_store.py
backend/src/agents/operator_guide/session_memory.py
```

역할:

```text
agent.py
- operator_guide 내부에서 어떤 leaf agent를 사용할지 고른다.

machine_help.py
- 장비 설명 질문을 처리하는 leaf agent다.

recipe_explainer.py
- 제작법, 레시피 질문을 처리하는 leaf agent다.

troubleshooter.py
- 고장 원인, 생산 실패, 현재 상태 기반 문제 해결 질문을 처리하는 leaf agent다.

service.py
- operator_guide의 실제 중심 서비스다.
- 질문 분류, 현재 상태 필요 여부 판단, CSV evidence, RAG 검색, prompt context 구성을 담당한다.

prompt_builder.py / system_prompt.py
- LLM에게 보낼 system/user prompt를 만든다.

rag_*.py
- CSV를 RAG 문서로 바꾸고, embedding하고, PostgreSQL + pgvector에 저장/검색한다.
```

## operator_guide에 graph.py가 없는 이유

`operator_guide`는 다음 기능을 공통 pipeline에서 이미 제공받는다.

```text
- request envelope 검증
- top-level agent routing
- leaf agent routing
- cache lookup / cache write
- middleware before / after
- LLM default / fallback 호출
- LLM 응답 파싱
- response schema validation
- agent.response envelope 생성
```

그래서 operator_guide 폴더 안에 다시 `graph.py`, `nodes.py`를 만들면 같은 책임이 중복될 수 있다.

현재 구조에서는 다음처럼 책임이 나뉜다.

| 책임 | 담당 파일 |
| --- | --- |
| 공통 graph 실행 | `backend/src/agents/pipeline/runtime.py` |
| 공통 graph 분기 | `backend/src/agents/pipeline/graph_edges.py` |
| 공통 state | `backend/src/agents/pipeline/state.py` |
| operator_guide leaf routing | `backend/src/agents/operator_guide/agent.py` |
| operator_guide 실제 답변 준비 | `backend/src/agents/operator_guide/service.py` |
| operator_guide prompt | `backend/src/agents/operator_guide/prompt_builder.py` |
| operator_guide RAG | `backend/src/agents/operator_guide/rag_retriever.py`, `rag_store.py` |

## 공통 AgentPipeline의 주요 node

`operator_guide`가 타는 공통 node 흐름은 다음과 같다.

```text
build_context
-> validate_envelope
-> route_top_agent
-> route_sub_agent
-> cache_lookup
-> middleware_before
-> build_prompt
-> call_llm.default
-> call_llm.fallback1 / fallback2
-> parse_llm_response
-> validate_response_schema
-> cache_write
-> middleware_after
-> build_agent_response
```

이 node들은 `operator_guide` 전용 파일에 있지 않고 `pipeline/runtime.py`에 있다.

## operator_guide 내부 leaf agent

`operator_guide`는 질문 유형에 따라 아래 leaf agent 중 하나를 선택한다.

```text
operator_guide.machine_help
- "분쇄기가 뭐야?"
- "컨베이어는 어디에 써?"

operator_guide.recipe_explainer
- "철괴는 어떻게 만들어?"
- "기어 제작에 뭐가 필요해?"

operator_guide.troubleshooter
- "철괴가 안 만들어져. 왜 그래?"
- "컨베이어가 멈췄는데 뭘 확인해야 해?"
```

leaf agent는 직접 모든 답변을 만들기보다, `ManualQAService`를 통해 필요한 근거를 모으고 prompt를 구성한다.

## RAG와 현재 상태 사용 흐름

`operator_guide`의 핵심은 RAG와 현재 게임 상태를 함께 다루는 것이다.

```text
질문
-> 질문 유형 분류
-> 현재 상태 필요 여부 판단
-> CSV evidence 구성
-> PostgreSQL + pgvector RAG 검색
-> current_game_state 필요 scope만 추출
-> prompt context 구성
-> LLM 답변
```

단순 설명 질문:

```text
"분쇄기가 뭐야?"
-> 현재 상태 필요 없음
-> RAG / CSV 근거로 답변
```

현재 상태 기반 질문:

```text
"철괴가 안 만들어져. 왜 그래?"
-> 현재 상태 필요
-> Unreal이 보낸 context.current_game_state 사용
-> selectedMachine, inputInventory, powerStatus 등을 prompt에 반영
```

## 파일별 빠른 위치

### graph / node / state 관련

```text
backend/src/agents/pipeline/runtime.py
backend/src/agents/pipeline/graph_edges.py
backend/src/agents/pipeline/state.py
```

### router 관련

```text
backend/src/agents/router.py
backend/src/agents/operator_guide/agent.py
backend/src/agents/operator_guide/debug_router.py
```

### prompt 관련

```text
backend/src/agents/operator_guide/system_prompt.py
backend/src/agents/operator_guide/prompt_builder.py
backend/src/agents/operator_guide/retrieved_context_guard.py
backend/src/agents/operator_guide/answer_sanitizer.py
```

### RAG / embedding / pgvector 관련

```text
backend/src/agents/operator_guide/rag_documents.py
backend/src/agents/operator_guide/rag_embedding.py
backend/src/agents/operator_guide/rag_ingestion.py
backend/src/agents/operator_guide/rag_retriever.py
backend/src/agents/operator_guide/rag_store.py
backend/src/agents/operator_guide/rag_schema.py
backend/src/agents/operator_guide/rag_upsert.py
```

### session memory 관련

```text
backend/src/agents/operator_guide/session_memory.py
```

### troubleshooting 데이터와 로그

```text
data/game/troubleshooting_rules.csv
docs/operator_guide/operator_guide_troubleshooting_log.md
```

## 언제 operator_guide에 graph.py를 만들면 좋을까?

지금은 필요 없다.

다만 아래 조건이 생기면 `operator_guide/graph.py`, `operator_guide/nodes.py`로 분리할 수 있다.

```text
- operator_guide만의 독립적인 node 흐름이 많아진다.
- leaf agent마다 완전히 다른 graph가 필요하다.
- RAG 검색, current state 조회, response validation을 공통 pipeline과 다른 순서로 실행해야 한다.
- LangGraph tool call을 operator_guide 전용으로 복잡하게 orchestration해야 한다.
```

현재는 공통 pipeline이 충분히 잘 맞는다.

## 면접용 설명

면접에서는 이렇게 설명하면 된다.

```text
material_generation은 독립적인 생성 파이프라인이 필요해서 전용 graph.py와 nodes.py를 둔 구조입니다.
반면 operator_guide는 여러 Agent가 공유하는 공통 AgentPipeline 위에서 동작하도록 설계했습니다.

그래서 operator_guide에는 별도 graph.py가 없고,
공통 pipeline의 routing, cache, middleware, fallback, response validation을 그대로 사용합니다.
operator_guide 고유 로직은 agent.py, leaf agent, service.py, prompt/RAG 모듈에 분리했습니다.
```

더 짧게 말하면 다음과 같다.

```text
operator_guide는 그래프가 없는 게 아니라 공통 그래프를 재사용하는 Agent입니다.
```
