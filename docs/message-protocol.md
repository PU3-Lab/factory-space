# 메시지 프로토콜

이 문서는 Unreal Engine과 Python 백엔드가 WebSocket으로 주고받는 JSON 메시지 계약을 설명합니다.

프로토콜은 agent 내부 구현과 분리되어야 합니다. 각 agent가 룰베이스, LLM, RAG, DB 조회, 시뮬레이션 등 어떤 방식을 사용하더라도 Unreal과 백엔드 사이의 메시지 구조는 안정적으로 유지하는 것을 목표로 합니다.

## 기본 원칙

- 모든 메시지는 JSON object입니다.
- 모든 메시지는 `type`, `version`, `session_id`를 가집니다.
- agent와 관련된 메시지는 `agent` field를 가집니다.
- 실제 내용은 `payload` 안에 넣습니다.
- 호환되지 않는 변경이 생기면 `version`을 갱신합니다.
- Unreal이 실행해야 하는 작업은 free text가 아니라 구조화된 `actions`로 표현합니다.

## 공통 Envelope

기본 형태:

```json
{
  "type": "agent_request",
  "version": "1.0",
  "request_id": "req-001",
  "session_id": "demo-session",
  "client_id": "unreal-client-01",
  "agent": "quest",
  "payload": {}
}
```

공통 필드:

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `type` | string | 예 | 메시지 종류 |
| `version` | string | 예 | 프로토콜 버전 |
| `request_id` | string | 권장 | 요청 추적용 ID |
| `session_id` | string | 예 | 플레이/대화/시뮬레이션 세션 ID |
| `client_id` | string | 권장 | Unreal 클라이언트 ID |
| `agent` | string | agent 메시지에서 예 | 대상 agent ID |
| `payload` | object | 예 | 메시지 본문 |

## 메시지 타입

초기 지원 대상:

- `ping`
- `pong`
- `agent_request`
- `agent_response`
- `action_result`
- `error`

추후 필요 시 추가 가능:

- `action_request`
- `state_update`
- `agent_event`
- `stream_delta`
- `stream_end`

## ping

연결 상태 확인용 메시지입니다.

Unreal -> Backend:

```json
{
  "type": "ping",
  "version": "1.0",
  "request_id": "req-ping-001",
  "session_id": "demo-session",
  "client_id": "unreal-client-01",
  "payload": {
    "timestamp": "2026-05-27T12:00:00Z"
  }
}
```

## pong

`ping`에 대한 응답입니다.

Backend -> Unreal:

```json
{
  "type": "pong",
  "version": "1.0",
  "request_id": "req-ping-001",
  "session_id": "demo-session",
  "client_id": "unreal-client-01",
  "payload": {
    "timestamp": "2026-05-27T12:00:00Z"
  }
}
```

## agent_request

Unreal이 특정 agent에게 처리를 요청할 때 사용합니다.

Unreal -> Backend:

```json
{
  "type": "agent_request",
  "version": "1.0",
  "request_id": "req-quest-001",
  "session_id": "demo-session",
  "client_id": "unreal-client-01",
  "agent": "quest",
  "payload": {
    "event": "player_entered_area",
    "area_id": "machine_room",
    "world_state": {
      "player_location": "machine_room",
      "nearby_objects": ["control_panel_01", "machine_01"]
    }
  }
}
```

agent별 payload는 각 agent가 정의합니다. 다만 envelope 구조는 유지해야 합니다.

## agent_response

agent 처리 결과를 Unreal로 보낼 때 사용합니다.

Backend -> Unreal:

```json
{
  "type": "agent_response",
  "version": "1.0",
  "request_id": "req-quest-001",
  "session_id": "demo-session",
  "client_id": "unreal-client-01",
  "agent": "quest",
  "payload": {
    "text": "기계실에 도착했습니다. 제어 패널을 확인하세요.",
    "actions": [
      {
        "name": "show_ui_message",
        "args": {
          "text": "제어 패널을 확인하세요."
        }
      },
      {
        "name": "highlight_object",
        "args": {
          "object_id": "control_panel_01"
        }
      }
    ],
    "metadata": {
      "quest_id": "quest-001",
      "step": "inspect_control_panel"
    }
  }
}
```

`payload.text`는 사용자에게 보여줄 수 있는 자연어 응답입니다.

`payload.actions`는 Unreal이 실행할 수 있는 구조화된 명령입니다.

## action schema

action은 다음 형태를 따릅니다.

```json
{
  "name": "highlight_object",
  "args": {
    "object_id": "control_panel_01"
  }
}
```

공통 필드:

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `name` | string | 예 | action 이름 |
| `args` | object | 예 | action 인자 |

초기 action 후보:

| action | 설명 |
| --- | --- |
| `show_ui_message` | Unreal UI에 메시지 표시 |
| `highlight_object` | 특정 오브젝트 강조 |
| `focus_camera` | 카메라 시선 이동 |
| `move_npc` | NPC 이동 |
| `play_animation` | 애니메이션 실행 |
| `set_object_state` | 오브젝트 상태 변경 |
| `spawn_object` | 오브젝트 생성 |
| `update_quest_marker` | 퀘스트 마커 갱신 |

action 목록은 Unreal 구현과 맞춰 별도 문서로 확장할 수 있습니다.

## action_result

Unreal이 action 실행 결과를 백엔드에 알릴 때 사용합니다.

Unreal -> Backend:

```json
{
  "type": "action_result",
  "version": "1.0",
  "request_id": "req-action-001",
  "session_id": "demo-session",
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

실패 예시:

```json
{
  "type": "action_result",
  "version": "1.0",
  "request_id": "req-action-002",
  "session_id": "demo-session",
  "client_id": "unreal-client-01",
  "agent": "quest",
  "payload": {
    "action": {
      "name": "highlight_object",
      "args": {
        "object_id": "missing_object"
      }
    },
    "status": "failed",
    "error": {
      "code": "OBJECT_NOT_FOUND",
      "message": "오브젝트를 찾을 수 없습니다."
    }
  }
}
```

## error

백엔드가 요청을 처리할 수 없을 때 사용합니다.

Backend -> Unreal:

```json
{
  "type": "error",
  "version": "1.0",
  "request_id": "req-quest-001",
  "session_id": "demo-session",
  "client_id": "unreal-client-01",
  "agent": "quest",
  "payload": {
    "code": "UNKNOWN_AGENT",
    "message": "요청한 agent를 찾을 수 없습니다.",
    "details": {
      "agent": "unknown_agent"
    }
  }
}
```

오류 코드 예시:

| code | 설명 |
| --- | --- |
| `INVALID_JSON` | JSON 파싱 실패 |
| `INVALID_MESSAGE` | 공통 envelope 검증 실패 |
| `UNKNOWN_MESSAGE_TYPE` | 지원하지 않는 message type |
| `UNKNOWN_AGENT` | 등록되지 않은 agent |
| `VALIDATION_ERROR` | payload schema 검증 실패 |
| `AGENT_EXECUTION_ERROR` | agent 실행 중 오류 |
| `UNSUPPORTED_ACTION` | 지원하지 않는 action |

## agent별 payload 예시

### 공장 최적화 Agent

```json
{
  "type": "agent_request",
  "version": "1.0",
  "request_id": "req-factory-001",
  "session_id": "demo-session",
  "client_id": "unreal-client-01",
  "agent": "factory_optimization",
  "payload": {
    "question": "현재 병목이 어디야?",
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

### Q&A 챗봇 Agent

```json
{
  "type": "agent_request",
  "version": "1.0",
  "request_id": "req-qa-001",
  "session_id": "demo-session",
  "client_id": "unreal-client-01",
  "agent": "qa_chatbot",
  "payload": {
    "question": "이 설비는 어떻게 점검하나요?",
    "context": {
      "selected_object_id": "machine_01"
    }
  }
}
```

### 퀘스트 Agent

```json
{
  "type": "agent_request",
  "version": "1.0",
  "request_id": "req-quest-001",
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

### 신물질 생성 Agent

```json
{
  "type": "agent_request",
  "version": "1.0",
  "request_id": "req-material-001",
  "session_id": "demo-session",
  "client_id": "unreal-client-01",
  "agent": "material_generation",
  "payload": {
    "goal": "가볍고 열에 강한 소재",
    "constraints": {
      "max_weight": "low",
      "heat_resistance": "high",
      "cost": "medium"
    }
  }
}
```

## 버전 관리

현재 프로토콜 버전:

```text
1.0
```

호환 가능한 변경:

- optional field 추가
- 새로운 action 추가
- 새로운 agent 추가
- 기존 payload에 optional metadata 추가

호환되지 않는 변경:

- 필수 field 제거
- field 타입 변경
- 기존 action args 구조 변경
- 기존 message type 의미 변경

호환되지 않는 변경이 필요하면 version을 올리고 문서에 변경 내용을 기록합니다.
