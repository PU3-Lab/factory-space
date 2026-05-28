# Agent 개발 가이드

이 문서는 Factory Space에 agent를 추가하거나 기존 agent를 구현할 때 따라야 할 기준을 설명합니다.

전체 구조는 [architecture.md](architecture.md)를, Unreal과 주고받는 JSON 계약은 [message-protocol.md](message-protocol.md)를 함께 확인하세요.

## 로컬 명령어

개발 서버 실행:

```bash
uv run main.py
```

Health check:

```text
http://127.0.0.1:8000/health
```

WebSocket endpoint:

```text
ws://127.0.0.1:8000/ws
```

테스트와 린트:

```bash
uv run --extra dev pytest
uv run --extra dev ruff check src tests
```

## Agent 담당 영역

대부분의 agent 작업은 자신의 폴더 안에서 끝나야 합니다.

```text
src/factory_space/agents/{agent_name}/
```

현재 기본 agent:

| Agent ID | 폴더 |
| --- | --- |
| `factory_optimization` | `src/factory_space/agents/factory_optimization/` |
| `qa_chatbot` | `src/factory_space/agents/qa_chatbot/` |
| `quest` | `src/factory_space/agents/quest/` |
| `material_generation` | `src/factory_space/agents/material_generation/` |

한 agent가 다른 agent의 내부 파일에 직접 의존하지 않도록 합니다. 둘 이상의 agent가 실제로 같은 구현을 공유해야 할 때만 `shared/`로 옮깁니다.

## 권장 폴더 구조

```text
agent.py        # Agent 진입점과 create_agent()
schemas.py      # Agent 전용 payload 또는 domain schema
rules.py        # Rule-based logic이 필요할 때 사용
service.py      # Domain orchestration logic
repository.py   # Agent 전용 DB 또는 vector store 접근
models.py       # Agent가 소유하는 DB model
prompts.py      # LLM prompt가 필요할 때 사용
tests/          # Agent 전용 test
scenarios/      # Scenario 예시 또는 replay data
```

현재 초기 agent들은 stub 상태입니다. 주변 파일은 각 agent가 공통 계약을 바꾸지 않고 확장될 수 있도록 미리 마련되어 있습니다.

## 필수 Agent 계약

모든 agent는 `BaseAgent`와 호환되도록 `agent_id`와 async `process()` method를 제공해야 합니다.

```python
from factory_space.core.state.context import AgentContext
from factory_space.messages.protocol import AgentRequest, AgentResponse


class SomeAgent:
    agent_id = "some_agent"

    async def process(
        self,
        request: AgentRequest,
        context: AgentContext,
    ) -> AgentResponse:
        ...
```

입력으로 사용할 수 있는 값:

- `request.version`
- `request.session_id`
- `request.request_id`
- `request.client_id`
- `request.agent`
- `request.payload`
- `context.session_id`
- `context.client_id`
- `context.request_id`
- `context.world_state`
- `context.agent_state`
- `context.metadata`
- `context.timestamp`

출력해야 하는 값:

- `AgentResponse`
- `AgentResponsePayload.text`
- `AgentResponsePayload.actions`
- `AgentResponsePayload.metadata`

agent는 WebSocket 메시지를 직접 보내지 않습니다. 구조화된 응답을 반환하고, transport 계층이 이를 직렬화합니다.

## 최소 Agent 예시

```python
from factory_space.core.actions.schemas import Action
from factory_space.core.state.context import AgentContext
from factory_space.messages.protocol import (
    AgentRequest,
    AgentResponse,
    AgentResponsePayload,
)


class ExampleAgent:
    agent_id = "example"

    async def process(
        self,
        request: AgentRequest,
        context: AgentContext,
    ) -> AgentResponse:
        return AgentResponse(
            session_id=context.session_id,
            request_id=context.request_id,
            client_id=context.client_id,
            agent=self.agent_id,
            payload=AgentResponsePayload(
                text="Request received.",
                actions=[
                    Action(
                        name="show_ui_message",
                        args={"text": "Request received."},
                    )
                ],
                metadata={"status": "ok"},
            ),
        )


def create_agent() -> ExampleAgent:
    return ExampleAgent()
```

## Payload Schema

각 agent는 `request.payload`의 의미를 직접 소유합니다. payload 구조가 중요해지는 시점에는 agent 전용 payload model을 `schemas.py`에 정의합니다.

```python
from pydantic import BaseModel, ConfigDict, Field


class QAChatbotPayload(BaseModel):
    model_config = ConfigDict(extra="forbid")

    question: str = Field(min_length=1)
    context: dict[str, object] = Field(default_factory=dict)
```

`agent.py` 또는 service 코드에서 다음처럼 검증합니다.

```python
payload = QAChatbotPayload.model_validate(request.payload)
```

payload 예시나 필수 field가 바뀌면 [message-protocol.md](message-protocol.md)도 함께 갱신합니다.

## Service와 Repository 경계

agent가 stub이거나 작은 rule-based 구현일 때는 단순 logic을 `agent.py`에 둘 수 있습니다.

다음 경우에는 `service.py`로 분리하는 것이 좋습니다.

- 여러 decision이나 rule을 조합해야 하는 경우
- DB, vector store, 외부 API 결과를 agent 응답에 맞게 가공해야 하는 경우
- LLM, RAG, simulation 호출 흐름을 조율해야 하는 경우
- 독립적인 unit test가 필요할 만큼 logic이 중요해진 경우

다음 경우에는 `repository.py`로 persistence 접근을 분리합니다.

- SQL query가 도입되는 경우
- vector store lookup이 도입되는 경우
- domain object를 저장하거나 조회해야 하는 경우
- DB row를 agent가 쓰기 좋은 객체로 변환해야 하는 경우

의존 방향:

```text
Agent -> Service -> Repository -> Database / Vector Store / External API
```

## Action

agent는 Unreal이 실행할 명령을 `Action` 객체로 반환합니다.

```python
Action(
    name="highlight_object",
    args={"object_id": "machine_01"},
)
```

현재 검증은 비어 있지 않은 `name`과 object 형태의 `args`만 요구합니다. 새 action 이름을 추가하거나 인자를 바꾸기 전에는 Unreal 담당자와 계약을 맞추고, [message-protocol.md](message-protocol.md)에 문서화합니다.

## Registry 등록

기본 agent는 다음 파일에서 등록합니다.

```text
src/factory_space/core/agents/registry.py
```

새 agent 추가 절차:

1. `src/factory_space/agents/{agent_name}/` 폴더를 만듭니다.
2. `agent.py`에 `agent_id`, `process()`, `create_agent()`를 구현합니다.
3. 필요하면 `schemas.py`, `rules.py`, `service.py`, `repository.py`, `models.py`, `prompts.py`를 추가합니다.
4. `create_default_registry()`에 agent factory를 등록합니다.
5. 테스트를 추가합니다.
6. Unreal에서 직접 호출할 agent라면 `message-protocol.md`에 agent id와 payload 예시를 추가합니다.

## 테스트 기준

최소한 다음 내용을 테스트합니다.

- registry에 agent id가 등록되어 있는지
- `process()`가 `AgentResponse`를 반환하는지
- response가 `session_id`, `request_id`, `client_id`를 유지하는지
- 기대하는 `text`, `actions`, `metadata`가 포함되는지
- payload schema가 도입된 경우 잘못된 payload가 예측 가능하게 실패하는지

기존 통합 수준 테스트는 다음 위치에 있습니다.

```text
tests/test_agent_contracts.py
tests/test_message_router.py
tests/test_websocket_endpoint.py
```

agent 전용 테스트는 둘 중 한 위치를 사용할 수 있습니다.

```text
tests/test_{agent_name}_agent.py
```

또는:

```text
src/factory_space/agents/{agent_name}/tests/
```

## WebSocket 간단 테스트

서버 실행:

```bash
uv run main.py
```

script client 실행:

```bash
uv run python scripts/ws_test_client.py
uv run python scripts/ws_test_quest.py
uv run python scripts/ws_test_qa_chatbot.py
uv run python scripts/ws_test_factory_optimization.py
uv run python scripts/ws_test_material_generation.py
```

요청 예시:

```json
{
  "type": "agent_request",
  "version": "1.0",
  "session_id": "test-session",
  "client_id": "test-client",
  "agent": "qa_chatbot",
  "payload": {
    "question": "hello"
  }
}
```

## 공통 영역 수정 규칙

다음 폴더는 모든 agent에 영향을 줄 수 있으므로 신중하게 수정합니다.

```text
src/factory_space/core/
src/factory_space/messages/
src/factory_space/websocket/
src/factory_space/shared/
```

공통 계약을 바꾸기 전에는 다음을 확인합니다.

- 기존 agent 테스트가 통과하는가?
- WebSocket envelope가 기존 caller와 호환되는가?
- Unreal 쪽 protocol version 변경이 필요한가?
- 문서와 예시가 새 구조를 반영하는가?

## PR 체크리스트

- Agent 계약을 유지했습니다.
- payload 구조가 의미를 가지는 경우 명시적인 schema를 추가했습니다.
- domain 또는 persistence logic이 커진 경우 service/repository 경계를 사용했습니다.
- action 구조를 명확히 했고 필요한 문서를 갱신했습니다.
- 테스트 또는 scenario를 추가하거나 갱신했습니다.
- `pytest`가 통과합니다.
- `ruff check src tests`가 통과합니다.
- Unreal-facing 계약이 바뀐 경우 `message-protocol.md`를 갱신했습니다.
