# 아키텍처

이 문서는 Factory Space Python 백엔드의 내부 구조와 책임 분리를 설명합니다.

백엔드는 Unreal Engine과 WebSocket으로 통신하고, 들어온 메시지를 목적별 agent에 전달한 뒤, agent가 만든 응답과 action을 Unreal로 돌려보냅니다.

## 큰 그림

```text
Unreal Engine
  -> WebSocket Endpoint
  -> Message Router
  -> Agent Orchestrator
  -> Target Agent
  -> Service
  -> Repository
  -> Database / Vector Store / External System
  -> Agent Response
  -> Action Dispatcher
  -> WebSocket Endpoint
  -> Unreal Engine
```

각 단계는 서로 다른 책임을 가집니다.

## 책임 분리

### WebSocket Endpoint

Unreal과 실제 WebSocket 연결을 담당합니다.

주요 책임:

- 클라이언트 연결 수락 (`/ws` 엔드포인트)
- 메시지 수신 및 client_id 추출 (JSON 필드)
- 메시지 직렬화/역직렬화
- 연결 종료 처리
- transport 수준의 에러 처리
- 응답 메시지 전송

**라우팅:**
모든 클라이언트는 **단일 WebSocket 채널 (`/ws`)** 로 연결됩니다. 클라이언트 식별은 JSON의 `client_id` 필드로 이루어집니다. 첫 메시지에서 `client_id`를 반드시 포함해야 합니다.

이 계층은 agent 내부 동작을 알지 않아야 합니다.

예상 위치:

```text
src/factory_space/websocket/
```

### Message Router

수신한 메시지의 타입과 대상 agent를 확인합니다.

주요 책임:

- `ping`, `agent_request`, `action_result` 같은 message type 구분
- 메시지 schema 검증
- agent 요청이면 orchestrator로 전달
- 잘못된 메시지면 error response 생성

예상 위치:

```text
src/factory_space/messages/
```

### Agent Orchestrator

agent 실행을 조율합니다.

주요 책임:

- 요청에 맞는 agent 선택
- session/context 구성
- agent 실행
- agent 응답 표준화
- 필요 시 state 업데이트
- 공통 로그 또는 trace 기록

orchestrator는 agent의 내부 구현 방식에는 관여하지 않습니다.

예상 위치:

```text
src/factory_space/core/agents/orchestrator.py
```

### Agent

각 도메인의 의사결정 진입점입니다.

예시:

- 공장 최적화 Agent
- Q&A 챗봇 Agent
- 퀘스트 Agent
- 신물질 생성 Agent

주요 책임:

- agent 전용 request 해석
- 필요한 service 호출
- 내부 추론 방식 실행
- 응답 text와 action 생성
- agent 전용 상태 변경 요청

agent 내부 구현 방식은 담당자가 선택합니다. 룰베이스, LLM, RAG, 시뮬레이션, DB 조회, 외부 API 호출 등을 사용할 수 있습니다.

예상 위치:

```text
src/factory_space/agents/{agent_name}/agent.py
```

### Service

agent 도메인 로직을 담당합니다.

주요 책임:

- repository 여러 개를 조합
- DB 데이터를 agent가 쓰기 좋은 형태로 변환
- 도메인 규칙 처리
- 외부 시스템 호출을 agent에서 분리

agent가 모든 로직을 직접 가지면 커지기 쉬우므로, DB 조회나 도메인 처리 흐름은 service로 나눕니다.

예상 위치:

```text
src/factory_space/agents/{agent_name}/service.py
```

### Repository

DB 접근을 담당합니다.

주요 책임:

- query 작성
- 데이터 저장/조회/수정
- DB row와 도메인 객체 간 변환

agent는 raw database session에 직접 접근하지 않고 service를 통해 repository를 사용합니다.

예상 위치:

```text
src/factory_space/agents/{agent_name}/repository.py
```

### Action Dispatcher

agent 응답 안의 action을 Unreal이 실행할 수 있는 메시지로 정리합니다.

주요 책임:

- action schema 검증
- action 이름과 args 정규화
- Unreal로 보낼 action response 구성
- 지원하지 않는 action 처리

처음에는 단순히 agent response 안의 action을 검증하는 정도로 시작할 수 있습니다.

예상 위치:

```text
src/factory_space/core/actions/
```

## Agent 입출력 계약

모든 agent는 공통 입력/출력 계약을 지켜야 합니다.

개념적 형태:

```python
class BaseAgent:
    agent_id: str

    async def process(self, request, context):
        ...
```

입력:

- 공통 request metadata
- agent 이름
- session id
- payload
- runtime context

출력:

- response type
- agent 이름
- session id
- 사용자에게 보여줄 text
- Unreal이 실행할 actions
- 선택적 debug/metadata

agent 내부에서 어떤 방식으로 판단하는지는 공통부가 알 필요 없습니다.

## Context와 State

agent 실행에는 현재 상황 정보가 필요합니다.

예상 context:

```text
AgentContext
- session_id
- client_id
- user_id
- world_state
- agent_state
- request_id
- timestamp
```

state 종류:

- session state
- world state
- quest progress
- factory state
- material generation history
- conversation memory

초기에는 메모리 또는 SQLite로 시작하고, 필요에 따라 Redis, PostgreSQL, vector DB 등을 추가할 수 있습니다.

## DB 사용 방향

기본 의존 방향:

```text
Agent -> Service -> Repository -> DB
```

예시:

```text
QuestAgent
  -> QuestService
  -> QuestRepository
  -> quest_progress table
```

```text
QAChatbotAgent
  -> QAService
  -> DocumentRepository
  -> documents table / vector store
```

agent 전용 DB 모델은 우선 agent 폴더 안에 둡니다.

```text
src/factory_space/agents/{agent_name}/models.py
```

여러 agent가 공유하는 모델만 shared 또는 core 쪽으로 이동합니다.

## Agent Registry

orchestrator가 agent를 찾으려면 registry가 필요합니다.

개념적 형태:

```python
AGENT_REGISTRY = {
    "factory_optimization": create_factory_optimization_agent,
    "qa_chatbot": create_qa_chatbot_agent,
    "quest": create_quest_agent,
    "material_generation": create_material_generation_agent,
}
```

새 agent를 추가할 때는 registry에 등록해야 WebSocket 요청에서 호출할 수 있습니다.

## 메시지 처리 흐름

`agent_request` 기준 처리 흐름:

```text
1. Unreal이 WebSocket으로 JSON 메시지를 보낸다.
2. WebSocket endpoint가 메시지를 수신한다.
3. Message router가 message type과 schema를 확인한다.
4. `agent` field를 기준으로 대상 agent를 찾는다.
5. Orchestrator가 AgentContext를 구성한다.
6. 대상 agent의 `process()`를 호출한다.
7. agent는 service/repository 등을 사용해 응답을 만든다.
8. 응답 안의 action을 검증한다.
9. WebSocket endpoint가 Unreal로 response를 보낸다.
```

## 에러 처리 방향

에러는 가능한 구조화해서 반환합니다.

예상 error response:

```json
{
  "type": "error",
  "version": "1.0",
  "session_id": "demo-session",
  "agent": "quest",
  "payload": {
    "code": "UNKNOWN_AGENT",
    "message": "요청한 agent를 찾을 수 없습니다."
  }
}
```

에러 분류 예시:

- 잘못된 JSON
- 지원하지 않는 message type
- 알 수 없는 agent
- schema validation 실패
- agent 실행 실패
- 지원하지 않는 action

## 확장 방향

처음에는 단순한 구조로 시작하고, 필요가 생기면 확장합니다.

가능한 확장:

- LLM engine 추가
- RAG/vector search 추가
- agent별 DB 모델 추가
- Redis 기반 session/runtime state 추가
- PostgreSQL 전환
- agent execution log 저장
- scenario replay 테스트
- Unreal action catalog 문서화

확장할 때도 Unreal-facing message protocol과 agent 입출력 계약은 최대한 안정적으로 유지합니다.
