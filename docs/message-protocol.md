# 메시지 프로토콜

이 문서는 현재 코드에서 지원하는 Unreal Engine과 Python 백엔드 사이의 WebSocket JSON 메시지 계약을 설명합니다.

현재 프로토콜 버전은 `1.0`입니다. 호환되지 않는 변경이 필요하면 `version`을 올리고 이 문서를 함께 갱신합니다.

## 엔드포인트

```text
ws://{host}/ws
```

로컬 개발 기본 주소:

```text
ws://127.0.0.1:8000/ws
```

첫 번째 WebSocket 메시지는 반드시 `client_id`를 포함해야 합니다. 서버는 첫 메시지에서 추출한 `client_id`를 연결 식별자로 사용하고, 이후 raw message를 처리할 때 같은 `client_id`를 주입합니다.

## 공통 Envelope

모든 transport message는 `MessageEnvelope` 구조를 따릅니다.

```json
{
  "type": "agent_request",
  "version": "1.0",
  "session_id": "demo-session",
  "request_id": "req-001",
  "client_id": "unreal-client-01",
  "agent": "quest",
  "payload": {}
}
```

| 필드 | 타입 | 필수 여부 | 기본값 | 설명 |
| --- | --- | --- | --- | --- |
| `type` | string | 필수 | 없음 | 메시지 종류입니다. 아래 지원 목록 중 하나여야 합니다. |
| `version` | string | 선택 | `"1.0"` | 프로토콜 버전입니다. |
| `session_id` | string | 필수 | 없음 | Unreal 또는 플레이 세션 식별자입니다. |
| `request_id` | string 또는 null | 선택 | `null` | 요청 추적용 id입니다. |
| `client_id` | string 또는 null | 첫 WebSocket 메시지에서는 필수 | `null` | Unreal client 식별자입니다. |
| `agent` | string 또는 null | `agent_request`에서는 필수 | `null` | 대상 agent id입니다. |
| `payload` | object | 선택 | `{}` | 메시지 본문입니다. |

`MessageEnvelope`는 정의되지 않은 top-level field를 허용하지 않습니다.

## 지원하는 메시지 타입

현재 `MessageType` union은 다음 값을 지원합니다.

- `ping`
- `pong`
- `agent_request`
- `agent_response`
- `action_result`
- `error`

현재 router가 정상 요청으로 처리하는 type은 다음과 같습니다.

| 타입 | 방향 | 현재 동작 |
| --- | --- | --- |
| `ping` | Unreal -> Backend | 같은 payload와 요청 metadata를 담아 `pong`을 반환합니다. |
| `agent_request` | Unreal -> Backend | orchestrator를 통해 대상 agent를 호출합니다. |
| `action_result` | Unreal -> Backend | `{"status": "received"}` payload를 담은 `pong`을 반환합니다. |

`pong`, `agent_response`, `error`는 주로 backend가 Unreal로 보내는 응답 type입니다. Unreal이 이 type을 보내면 envelope 검증은 통과할 수 있지만, router에서는 일반 요청으로 처리하지 않습니다.

지원 목록에 없는 `type` 값은 WebSocket endpoint의 `MessageEnvelope` 검증에서 실패하고 `INVALID_MESSAGE`를 반환합니다.

## ping

Unreal -> Backend:

```json
{
  "type": "ping",
  "version": "1.0",
  "session_id": "demo-session",
  "request_id": "req-ping-001",
  "client_id": "unreal-client-01",
  "payload": {
    "timestamp": "2026-05-28T12:00:00Z"
  }
}
```

Backend -> Unreal:

```json
{
  "type": "pong",
  "version": "1.0",
  "session_id": "demo-session",
  "request_id": "req-ping-001",
  "client_id": "unreal-client-01",
  "payload": {
    "timestamp": "2026-05-28T12:00:00Z"
  }
}
```

## agent_request

Unreal은 특정 agent에 처리를 요청할 때 `agent_request`를 보냅니다.

Unreal -> Backend:

```json
{
  "type": "agent_request",
  "version": "1.0",
  "session_id": "demo-session",
  "request_id": "req-quest-001",
  "client_id": "unreal-client-01",
  "agent": "quest",
  "payload": {
    "event": "player_entered_area",
    "area_id": "machine_room"
  }
}
```

router는 envelope를 `AgentRequest`로 변환합니다.

| 필드 | 타입 | 필수 여부 | 기본값 |
| --- | --- | --- | --- |
| `version` | string | 선택 | `"1.0"` |
| `session_id` | string | 필수 | 없음 |
| `agent` | string | 필수 | 없음 |
| `payload` | object | 선택 | `{}` |
| `request_id` | string 또는 null | 선택 | `null` |
| `client_id` | string 또는 null | 선택 | `null` |

각 agent는 자기 `payload`의 의미를 직접 소유합니다. agent별 payload schema는 `src/factory_space/agents/{agent_name}/schemas.py`에 둡니다.

## agent_response

모든 agent는 `AgentResponse`를 반환합니다. backend는 이를 `MessageEnvelope`로 변환해 Unreal로 전송합니다.

Backend -> Unreal:

```json
{
  "type": "agent_response",
  "version": "1.0",
  "session_id": "demo-session",
  "request_id": "req-quest-001",
  "client_id": "unreal-client-01",
  "agent": "quest",
  "payload": {
    "text": "Quest request received.",
    "actions": [
      {
        "name": "show_ui_message",
        "args": {
          "text": "Quest request received."
        }
      }
    ],
    "metadata": {
      "status": "stub"
    }
  }
}
```

Response payload 필드:

| 필드 | 타입 | 필수 여부 | 기본값 | 설명 |
| --- | --- | --- | --- | --- |
| `text` | string | 선택 | `""` | Unreal이 사용자에게 보여줄 수 있는 응답 text입니다. |
| `actions` | `Action` 배열 | 선택 | `[]` | Unreal이 실행할 구조화된 명령 목록입니다. |
| `metadata` | object | 선택 | `{}` | debug, trace, status, agent별 metadata를 담습니다. |

현재 기본 agent들은 placeholder 응답을 반환하며 `metadata.status`는 `"stub"`입니다.

## Action Schema

`Action`은 Unreal이 실행할 수 있는 구조화된 명령입니다.

```json
{
  "name": "highlight_object",
  "args": {
    "object_id": "control_panel_01"
  }
}
```

| 필드 | 타입 | 필수 여부 | 기본값 | 설명 |
| --- | --- | --- | --- | --- |
| `name` | string | 필수 | 없음 | 비어 있지 않은 action 이름입니다. |
| `args` | object | 선택 | `{}` | action 인자입니다. |

현재 코드는 `name`이 비어 있지 않은지와 `args`가 object인지까지만 검증합니다. 아직 고정 action catalog를 강제하지는 않습니다.

agent가 사용하거나 사용할 예정인 공통 action 이름은 다음과 같습니다.

- `show_ui_message`
- `highlight_object`
- `focus_camera`
- `move_npc`
- `play_animation`
- `set_object_state`
- `spawn_object`
- `update_quest_marker`

새 action 이름이나 인자 계약이 Unreal 연동에 포함되면 이 문서를 갱신하고, 필요하면 코드 검증도 추가합니다.

## action_result

Unreal은 action 실행 결과를 `action_result`로 보고할 수 있습니다.

Unreal -> Backend:

```json
{
  "type": "action_result",
  "version": "1.0",
  "session_id": "demo-session",
  "request_id": "req-action-001",
  "client_id": "unreal-client-01",
  "agent": "quest",
  "payload": {
    "action": {
      "name": "highlight_object",
      "args": {
        "object_id": "control_panel_01"
      }
    },
    "status": "success",
    "result": {
      "object_id": "control_panel_01"
    }
  }
}
```

Backend -> Unreal:

```json
{
  "type": "pong",
  "version": "1.0",
  "session_id": "demo-session",
  "request_id": "req-action-001",
  "client_id": "unreal-client-01",
  "agent": "quest",
  "payload": {
    "status": "received"
  }
}
```

`protocol.py`에는 `ActionResultMessage` 모델이 있지만, 현재 router는 `action_result` payload를 `ActionResult`로 엄격 검증하지 않고 generic envelope 기준으로 받은 뒤 `pong`을 반환합니다.

## error

오류는 `ErrorMessage`로 반환합니다.

```json
{
  "type": "error",
  "version": "1.0",
  "session_id": "demo-session",
  "request_id": "req-quest-001",
  "client_id": "unreal-client-01",
  "agent": "quest",
  "payload": {
    "code": "UNKNOWN_AGENT",
    "message": "Requested agent was not found.",
    "details": {
      "agent": "missing"
    }
  }
}
```

Error payload 필드:

| 필드 | 타입 | 필수 여부 | 기본값 |
| --- | --- | --- | --- |
| `code` | string | 필수 | 없음 |
| `message` | string | 필수 | 없음 |
| `details` | object | 선택 | `{}` |

현재 error code:

| Code | 생성 위치 | 의미 |
| --- | --- | --- |
| `INVALID_JSON` | WebSocket endpoint | raw message가 올바른 JSON이 아닙니다. |
| `INVALID_MESSAGE` | WebSocket endpoint | JSON이 object가 아니거나 `MessageEnvelope` 검증에 실패했습니다. |
| `MISSING_CLIENT_ID` | WebSocket endpoint | 첫 WebSocket 메시지에 `client_id`가 없습니다. |
| `VALIDATION_ERROR` | Message router | `agent_request`에 `agent`가 없거나 `AgentRequest` 검증에 실패했습니다. |
| `UNKNOWN_AGENT` | Message router | 요청한 agent id가 registry에 없습니다. |
| `UNKNOWN_MESSAGE_TYPE` | Message router fallback | 이미 검증된 envelope가 router에서 처리하지 않는 요청 type으로 들어왔습니다. |

## 등록된 Agent ID

현재 기본 registry에 등록된 agent id는 다음과 같습니다.

- `factory_optimization`
- `qa_chatbot`
- `quest`
- `material_generation`

## Agent Payload 예시

공장 최적화:

```json
{
  "type": "agent_request",
  "version": "1.0",
  "session_id": "demo-session",
  "client_id": "unreal-client-01",
  "agent": "factory_optimization",
  "payload": {
    "question": "Where is the bottleneck?",
    "factory_state": {
      "machines": [
        {
          "id": "packaging_01",
          "input_rate": 100,
          "output_rate": 62,
          "status": "running"
        }
      ]
    }
  }
}
```

Q&A 챗봇:

```json
{
  "type": "agent_request",
  "version": "1.0",
  "session_id": "demo-session",
  "client_id": "unreal-client-01",
  "agent": "qa_chatbot",
  "payload": {
    "question": "How do I restart this machine?",
    "context": {
      "selected_object_id": "machine_01"
    }
  }
}
```

퀘스트:

```json
{
  "type": "agent_request",
  "version": "1.0",
  "session_id": "demo-session",
  "client_id": "unreal-client-01",
  "agent": "quest",
  "payload": {
    "event": "player_interacted",
    "object_id": "control_panel_01",
    "quest_id": "quest-001"
  }
}
```

신물질 생성:

```json
{
  "type": "agent_request",
  "version": "1.0",
  "session_id": "demo-session",
  "client_id": "unreal-client-01",
  "agent": "material_generation",
  "payload": {
    "goal": "lightweight heat-resistant material",
    "constraints": {
      "max_weight": "low",
      "heat_resistance": "high",
      "cost": "medium"
    }
  }
}
```
