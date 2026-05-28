# Factory Space 아키텍처

이 문서는 현재 Factory Space Python 백엔드의 구조와 책임 분리를 설명합니다.

Factory Space는 Unreal Engine과 WebSocket으로 통신하는 FastAPI 백엔드입니다. Unreal에서 들어온 메시지를 공통 envelope로 검증하고, `agent_request`는 registry에 등록된 agent로 라우팅합니다. Unreal과 맞닿는 메시지 계약과 agent 입출력 계약은 안정적으로 유지하고, agent 내부 구현은 각 agent 폴더 안에서 독립적으로 발전시키는 것이 기본 방향입니다.

## 실행 흐름

```text
Unreal Engine
  -> WebSocket endpoint (/ws)
  -> MessageEnvelope 검증
  -> Message Router
  -> Agent Orchestrator (LangGraph StateGraph)
  -> Agent Registry
  -> Target Agent
  -> AgentResponse
  -> MessageEnvelope
  -> Unreal Engine
```

현재 `agent_request` 처리 흐름은 다음과 같습니다.

1. Unreal이 `/ws`로 JSON text message를 전송합니다.
2. 첫 메시지에서 `client_id`를 추출해 WebSocket connection manager에 등록합니다.
3. `handle_raw_message()`가 JSON을 파싱하고 `MessageEnvelope`로 검증합니다.
4. `MessageRouter`가 `type`에 따라 메시지를 분기합니다.
5. `agent_request`는 `AgentRequest`로 정규화되어 `AgentOrchestrator`로 전달됩니다.
6. orchestrator는 LangGraph graph에서 `AgentContext`를 만들고 registry에서 대상 agent를 조회합니다.
7. 대상 agent의 `process(request, context)`를 호출합니다.
8. agent가 반환한 `AgentResponse`를 전송용 `MessageEnvelope`로 변환해 Unreal로 보냅니다.

## 소스 구조

```text
src/factory_space/
  app.py                         # FastAPI app factory
  websocket/
    endpoint.py                  # /ws endpoint, raw JSON parsing, client_id handling
    manager.py                   # WebSocket connection registry
  messages/
    protocol.py                  # WebSocket/agent message Pydantic models
    router.py                    # message type routing
  core/
    agents/
      base.py                    # BaseAgent protocol
      registry.py                # in-memory agent registry
      orchestrator.py            # LangGraph-based agent orchestration
    actions/
      schemas.py                 # Action and ActionResult schemas
    state/
      context.py                 # AgentContext model
    db/
      base.py
      session.py
  agents/
    factory_optimization/
    qa_chatbot/
    quest/
    material_generation/
  shared/
    repositories/
    schemas/
    services/
    vectorstores/
```

## 컴포넌트 책임

| 컴포넌트 | 위치 | 책임 |
| --- | --- | --- |
| FastAPI 앱 | `src/factory_space/app.py` | ASGI 앱을 만들고, `/health`와 WebSocket router를 등록합니다. |
| WebSocket endpoint | `src/factory_space/websocket/endpoint.py` | `/ws` 연결을 받고, raw JSON을 파싱하며, 첫 메시지의 `client_id`를 요구합니다. |
| 메시지 프로토콜 | `src/factory_space/messages/protocol.py` | envelope, agent request/response, action, error Pydantic 모델을 정의합니다. |
| 메시지 라우터 | `src/factory_space/messages/router.py` | `ping`, `agent_request`, `action_result`를 처리합니다. |
| Agent orchestrator | `src/factory_space/core/agents/orchestrator.py` | 하나의 agent 요청을 LangGraph 실행 graph로 처리합니다. |
| Agent registry | `src/factory_space/core/agents/registry.py` | `agent_id`로 등록된 agent를 저장하고 조회합니다. |
| Agent 폴더 | `src/factory_space/agents/{agent_name}/` | 각 agent의 진입점, schema, rule, service, repository, model, prompt, test를 소유합니다. |
| Action schema | `src/factory_space/core/actions/schemas.py` | agent가 Unreal에 요청할 수 있는 구조화된 명령을 정의합니다. |

## LangGraph Orchestrator 구조

현재 orchestrator는 의도적으로 작게 유지되어 있습니다. LangGraph를 실행 프레임워크로 사용하지만, 기존 agent 계약은 그대로 보존합니다.

```text
START
  -> build_context
  -> dispatch_agent
  -> END
```

Graph state 구성:

| 키 | 타입 | 설명 |
| --- | --- | --- |
| `request` | `AgentRequest` | router에서 전달된 정규화된 agent 요청입니다. |
| `context` | `AgentContext` | request metadata로 만든 실행 context입니다. |
| `response` | `AgentResponse` | 선택된 agent가 반환한 최종 응답입니다. |

Node별 책임:

| 노드 | 책임 |
| --- | --- |
| `build_context` | `AgentContext(session_id, client_id, request_id)`를 생성합니다. |
| `dispatch_agent` | `request.agent`를 `AgentRegistry`에서 조회하고 `agent.process()`를 호출합니다. |

이 구조는 현재 동작을 단순하게 유지하면서도, 이후 validation, trace logging, memory update, fallback routing, multi-agent workflow, streaming 같은 노드를 자연스럽게 추가할 수 있게 해줍니다.

## Agent 계약

모든 agent는 `BaseAgent` protocol을 따릅니다.

```python
class BaseAgent(Protocol):
    agent_id: str

    async def process(
        self,
        request: AgentRequest,
        context: AgentContext,
    ) -> AgentResponse:
        ...
```

agent는 WebSocket 메시지를 직접 보내지 않습니다. agent는 text, actions, metadata가 담긴 `AgentResponse`를 반환하고, transport 계층이 이 응답을 WebSocket envelope로 변환합니다.

## 등록된 Agent

기본 registry에는 현재 다음 agent가 등록되어 있습니다.

| Agent ID | Factory 함수 | 폴더 |
| --- | --- | --- |
| `factory_optimization` | `create_factory_optimization_agent()` | `src/factory_space/agents/factory_optimization/` |
| `qa_chatbot` | `create_qa_chatbot_agent()` | `src/factory_space/agents/qa_chatbot/` |
| `quest` | `create_quest_agent()` | `src/factory_space/agents/quest/` |
| `material_generation` | `create_material_generation_agent()` | `src/factory_space/agents/material_generation/` |

현재 agent 구현은 통합 테스트를 위한 stub입니다. 각 agent는 다음 값을 반환합니다.

- `payload.text`: placeholder text
- `payload.actions`: `show_ui_message` action 1개
- `payload.metadata.status`: `"stub"`
- `payload.metadata.received_payload`: 원본 request payload

## 데이터 접근 방향

agent별 저장소나 외부 시스템 접근이 필요할 때는 다음 의존 방향을 따릅니다.

```text
Agent -> Service -> Repository -> Database / Vector Store / External System
```

원칙:

- agent 코드는 다른 agent의 내부 파일에 직접 의존하지 않습니다.
- agent 전용 schema, model, service, repository는 우선 해당 agent 폴더 안에서 시작합니다.
- 둘 이상의 agent가 실제로 같은 구현을 재사용할 때만 `shared/`로 옮깁니다.
- 공통 runtime 계약은 `core/`에 둡니다.

## 오류 경계

transport 단계의 오류는 메시지가 router에 도달하기 전에 WebSocket endpoint에서 만들어집니다. 예시는 invalid JSON, JSON object가 아닌 메시지, 첫 메시지의 `client_id` 누락, envelope 검증 실패입니다.

routing 단계의 오류는 `MessageRouter`에서 만들어집니다. 예시는 `agent_request`의 `agent` 누락, `AgentRequest` 검증 실패, 등록되지 않은 agent id입니다.

현재 코드는 agent 실행 중 발생한 일반 예외를 별도의 error response로 감싸지 않습니다. 이 동작을 추가하면 error code와 response shape를 `message-protocol.md`에 함께 문서화해야 합니다.

## 확장 지점

가까운 확장 지점은 다음과 같습니다.

- 각 agent의 `schemas.py`에 구체적인 payload schema 추가
- stub agent 로직을 rule-based, LLM, RAG, simulation, DB, hybrid 구현으로 교체
- agent가 domain logic이나 persistence를 갖기 시작할 때 service/repository 경계 추가
- LangGraph에 validation, logging, trace capture, memory update, fallback routing 노드 추가
- Unreal이 더 엄격한 command 보장을 요구할 때 action validation 또는 action catalog 추가
