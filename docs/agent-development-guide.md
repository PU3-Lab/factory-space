# Agent 개발 가이드

이 문서는 각 agent 담당자가 Factory Space 백엔드에서 어디를 어떻게 수정하면 되는지 설명합니다.

전체 아키텍처는 [architecture.md](architecture.md), Unreal과 주고받는 JSON 계약은 [message-protocol.md](message-protocol.md)를 함께 참고하세요.

## 시작 전 확인

서버 실행:

```bash
uv run main.py
```

상태 확인:

```text
http://127.0.0.1:8000/health
```

WebSocket endpoint:

```text
ws://127.0.0.1:8000/ws/{client_id}
```

수동 WebSocket 테스트:

```bash
uv run python scripts/ws_test_client.py
```

테스트와 린트:

```bash
uv run --extra dev pytest
uv run --extra dev ruff check .
```

## 담당자별 작업 위치

각 담당자는 기본적으로 자기 agent 폴더 안에서 작업합니다.

```text
src/factory_space/agents/{agent_name}/
```

초기 agent 목록:

| agent id | 폴더 |
| --- | --- |
| `factory_optimization` | `src/factory_space/agents/factory_optimization/` |
| `qa_chatbot` | `src/factory_space/agents/qa_chatbot/` |
| `quest` | `src/factory_space/agents/quest/` |
| `material_generation` | `src/factory_space/agents/material_generation/` |

## Agent 폴더 구성

```text
agent.py        # WebSocket 요청이 최종 도착하는 agent 진입점
schemas.py      # agent 전용 payload/schema
service.py      # agent 도메인 로직
repository.py   # DB 접근이 필요할 때 사용하는 계층
models.py       # agent가 소유하는 DB 모델
rules.py        # 규칙 기반 로직이 필요할 때
prompts.py      # LLM prompt가 필요할 때
scenarios/      # agent 시나리오 예시
tests/          # agent 전용 테스트
```

처음에는 `agent.py`의 stub을 실제 구현으로 바꾸는 것부터 시작하면 됩니다.

## 공통 입출력 계약

모든 agent는 같은 `process()` 형태를 유지해야 합니다.

```python
async def process(
    self,
    request: AgentRequest,
    context: AgentContext,
) -> AgentResponse:
    ...
```

입력:

- `request.session_id`
- `request.request_id`
- `request.client_id`
- `request.agent`
- `request.payload`
- `context`

출력:

- `AgentResponse`
- `AgentResponsePayload.text`
- `AgentResponsePayload.actions`
- `AgentResponsePayload.metadata`

agent 내부 구현은 담당자가 선택합니다. 룰베이스, LLM, RAG, DB 조회, 외부 API, 시뮬레이션 모두 가능합니다. 단, 외부로 나가는 응답 구조는 공통 schema를 따라야 합니다.

## 기본 구현 예시

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
                text="요청을 처리했습니다.",
                actions=[
                    Action(
                        name="show_ui_message",
                        args={"text": "요청을 처리했습니다."},
                    )
                ],
                metadata={"status": "ok"},
            ),
        )
```

## Payload schema 작성

agent별 payload는 `schemas.py`에 정의합니다.

예시:

```python
from pydantic import BaseModel, ConfigDict


class QAChatbotPayload(BaseModel):
    model_config = ConfigDict(extra="forbid")

    question: str
    context: dict[str, object] = {}
```

`agent.py`에서는 다음처럼 검증해서 사용할 수 있습니다.

```python
payload = QAChatbotPayload.model_validate(request.payload)
```

payload 구조를 바꾸면 [message-protocol.md](message-protocol.md)의 agent별 예시도 함께 갱신하세요.

## Service와 Repository 사용 기준

간단한 로직은 `agent.py`에서 시작해도 됩니다. 다만 다음 경우에는 분리하세요.

`service.py`로 옮기기 좋은 경우:

- 여러 함수를 조합하는 도메인 로직
- DB 조회 결과를 agent가 쓰기 좋은 형태로 가공
- 외부 API나 모델 호출 흐름 관리
- 규칙/LLM/RAG 결과를 조합

`repository.py`로 옮기기 좋은 경우:

- DB query
- 저장/조회/수정/삭제
- DB row와 도메인 객체 변환

의존 방향:

```text
Agent -> Service -> Repository -> Database
```

## Action 작성 기준

Unreal이 실행할 작업은 `Action`으로 반환합니다.

```python
Action(
    name="highlight_object",
    args={"object_id": "machine_01"},
)
```

초기 action 후보:

- `show_ui_message`
- `highlight_object`
- `focus_camera`
- `move_npc`
- `play_animation`
- `set_object_state`
- `spawn_object`
- `update_quest_marker`

새 action을 추가하면 Unreal 담당자와 맞추고 [message-protocol.md](message-protocol.md)를 갱신하세요.

## Registry 등록

기존 4개 agent는 이미 `create_default_registry()`에 등록되어 있습니다.

위치:

```text
src/factory_space/core/agents/registry.py
```

새 agent를 추가할 때는 다음을 해야 합니다.

1. `src/factory_space/agents/{agent_name}/` 폴더 생성
2. `agent.py`에 `agent_id`와 `create_agent()` 구현
3. `create_default_registry()`에 등록
4. 테스트 추가
5. 문서 갱신

## 테스트 작성

agent 담당자는 최소한 자기 agent의 `process()` 테스트를 추가하세요.

권장 위치:

```text
tests/test_{agent_name}_agent.py
```

또는 agent 폴더 내부 테스트를 사용할 수 있습니다.

```text
src/factory_space/agents/{agent_name}/tests/
```

테스트에서 확인할 것:

- payload 검증
- 정상 응답 text
- 필요한 action 포함 여부
- metadata/status
- 잘못된 입력 처리

## WebSocket 테스트

서버 실행:

```bash
uv run main.py
```

테스트 client 실행:

```bash
uv run python scripts/ws_test_client.py
```

`agent_request` 테스트를 하고 싶으면 `scripts/ws_test_client.py`의 메시지를 다음처럼 바꿔서 실행할 수 있습니다.

```json
{
  "type": "agent_request",
  "version": "1.0",
  "session_id": "test-session",
  "agent": "qa_chatbot",
  "payload": {
    "question": "안녕?"
  }
}
```

## 공통 영역 수정 기준

다음 폴더는 모든 agent에 영향을 줄 수 있습니다.

```text
src/factory_space/core/
src/factory_space/messages/
src/factory_space/websocket/
src/factory_space/shared/
```

공통 영역을 수정할 때는 다음을 확인하세요.

- 기존 agent 테스트가 깨지지 않는가
- WebSocket message envelope이 호환되는가
- Unreal-facing JSON 구조가 바뀌는가
- 새 action/message type을 문서화했는가

## 작업 완료 기준

agent 작업 PR 전 체크리스트:

- agent 입출력 계약 유지
- 필요한 schema 추가
- 필요한 service/repository 분리
- action 구조 확인
- 테스트 추가 또는 갱신
- `uv run --extra dev pytest` 통과
- `uv run --extra dev ruff check .` 통과
- protocol 변경 시 문서 갱신
