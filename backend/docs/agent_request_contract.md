# 클라이언트 → 서버 요청 JSON 계약 (agent.request)

Unreal/웹 클라이언트가 WebSocket으로 보내는 `agent.request` 메시지의 규격을 정리한 문서입니다.

- **WebSocket 경로**: `/ws/agent`
- **연결 매니페스트(GET)**: `/api/v1/agent-connection` — 사용 가능한 에이전트 목록과 샘플 요청을 런타임에 조회할 수 있습니다.
- **생성 이미지 정적 경로(GET)**: `/materials/{material_id}/{asset_name}.png` — 생성된 material 이미지 파일을 직접 조회합니다.
- **헬스 체크**: `/health`
- 근거 소스: `src/app.py`, `src/protocol/messages.py`, `src/agent_connection/router.py`, `src/agents/material_generation/schemas.py`, `src/agents/material_generation/router.py`, `src/agents/pipeline/runtime.py`

---

## 1. 공통 요청 엔벨로프

모든 요청은 아래 엔벨로프로 감싸 보냅니다. (정의: `protocol/messages.py::AgentRequestEnvelope`)

```json
{
  "type": "agent.request",
  "request_id": "unreal-smoke-1",
  "session_id": "dev-session",
  "client_id": "unreal-client",
  "agent": "material_generation",
  "payload": { },
  "context": { }
}
```

| 필드 | 타입 | 필수 | 설명 |
|------|------|------|------|
| `type` | `"agent.request"` | 권장 | 메시지 종류. 생략 시 기본값 `agent.request` |
| `request_id` | string | 권장 | 요청 추적용 ID. 생략 시 서버가 UUID 자동 생성 |
| `session_id` | string \| null | 선택 | 세션 식별자. **material_generation에서는 `player_id`로도 사용**됨(없으면 `default_player`) |
| `client_id` | string \| null | 선택 | 클라이언트 식별자 |
| `agent` | string \| null | **필수** | 호출할 최상위 에이전트 ID (아래 2장 참고) |
| `payload` | object | **필수** | 에이전트별 도메인 데이터 (아래 3장) |
| `context` | object | 선택 | 부가 메타데이터 / LLM 오버라이드 (아래 4장) |

> 응답은 `agent.response`, 오류는 `agent.error` 엔벨로프로 돌아옵니다(5장).

---

## 2. 에이전트 종류

`agent` 필드에 넣는 최상위 에이전트와, `payload.sub_agent`로 지정하는 하위 에이전트입니다.
(실시간 목록은 `/api/v1/agent-connection`의 `top_level_agents` / `leaf_agents` 참고)

| `agent` | 하위 에이전트(`payload.sub_agent`) | 비고 |
|---------|-----------------------------------|------|
| `process_optimizer` | (없음) | 단일 leaf |
| `operator_guide` | `operator_guide.machine_help` / `.recipe_explainer` / `.troubleshooter` | |
| `quest_generator` | `quest_generator.production_quest` / `.economy_quest` | |
| `material_generation` | (없음 — sub_agent 지정 금지) | 단일 leaf. 머신+레시피로 단일 물질 합성 |
| `new_material_generator` | (없음 — sub_agent 지정 금지) | 단일 leaf. 설계 제약(목표) 기반 신소재 후보 목록 생성 |

---

## 3. 에이전트별 payload

### 3-1. `material_generation` — 재료 합성 (이미지 생성 포함)

스키마: `material_generation/schemas.py::MaterialCreationRequest`

```json
{
  "type": "agent.request",
  "agent": "material_generation",
  "session_id": "player-123",
  "payload": {
    "machine_type": "Synthesizer",
    "inputs": [
      { "item_id": "iron_ingot", "qty": 2 },
      { "item_id": "copper_ingot", "qty": 1 }
    ],
    "process_conditions": {
      "temperature": "1200C",
      "pressure": "5atm",
      "catalyst": "palladium"
    },
    "generate_visual_asset": true
  }
}
```

| payload 필드 | 타입 | 필수 | 기본값 | 설명 |
|--------------|------|------|--------|------|
| `machine_type` | string | **필수** | — | `Smelter` / `Grinder` / `Synthesizer` 중 하나 (그 외는 `invalid_machine`) |
| `inputs` | array | **필수** | — | 입력 아이템 목록 |
| `inputs[].item_id` | string | **필수** | — | 등록된 아이템 ID (미등록이면 `invalid_input`) |
| `inputs[].qty` | int | **필수** | — | 수량 |
| `process_conditions` | object | 선택 | 전부 default | 공정 조건 |
| `process_conditions.temperature` | string | 선택 | `"default"` | 예: `"1200C"` |
| `process_conditions.pressure` | string | 선택 | `"default"` | 예: `"5atm"` |
| `process_conditions.catalyst` | string \| null | 선택 | `null` | 예: `"palladium"`, `"none"` |
| `generate_visual_asset` | bool | 선택 | `true` | 합성 성공 시 아이콘/텍스처/썸네일 생성 여부 |

> `player_id`는 payload에 넣지 않습니다 — 엔벨로프의 `session_id`에서 파생됩니다(`runtime.py:239`).

**생성 이미지 조회**

`generate_visual_asset=true`인 신규 material은 백그라운드에서 이미지 생성이 진행됩니다. 생성 상태와 asset key는
`GET /api/v1/materials/{material_id}/visual-status`로 조회합니다.

```json
{
  "material_id": "mat_001",
  "visual_status": "visual_ready",
  "visual_asset_key": "materials/mat_001/icon.png",
  "texture_asset_key": "materials/mat_001/texture.png",
  "thumbnail_asset_key": "materials/mat_001/thumbnail.png"
}
```

asset key는 서버의 `FACTORY_IMAGE_STORAGE_PATH`(기본값: `var/assets`) 아래 상대 경로입니다. 클라이언트가 이미지를 직접 받을 때는 asset key 앞에 `/`를 붙여 요청합니다.

| asset key | GET 경로 |
|-----------|----------|
| `materials/{material_id}/icon.png` | `/materials/{material_id}/icon.png` |
| `materials/{material_id}/texture.png` | `/materials/{material_id}/texture.png` |
| `materials/{material_id}/thumbnail.png` | `/materials/{material_id}/thumbnail.png` |

아직 생성되지 않았거나 존재하지 않는 파일은 정적 경로에서 `404`를 반환합니다.

**머신 × 입력 조합에 따른 결과(`result_type`)**

클라이언트가 받는 `result_type`은 다음 5종입니다: `existing_recipe`, `new_material`, `cached_experiment`, `failed_result`, `invalid_input`.

| 조합 | `result_type` |
|------|---------------|
| 기존 레시피와 일치 (예: `Smelter` + `iron_ore`) | `existing_recipe` |
| 동일 머신·동일 재료, 수량만 다름 | `existing_recipe` (내부 분류 `simple_variation`) |
| `Synthesizer` + 서로 다른 2종 이상 (신규) | `new_material` (신물질 합성) |
| `Smelter` + 2종 이상 | `failed_result` (제련기는 합성 불가) |
| `Grinder` + 2종 이상 | `failed_result` (내부 분류 `intermediate_material`) |
| 미등록 `item_id` 포함 | `invalid_input` |
| `Smelter`/`Grinder`/`Synthesizer` 외 머신 | `invalid_input` (`failure_reason: invalid_machine`) |
| 이전에 동일 실험이 캐시됨 | `cached_experiment` |

**유효 아이템 ID 형태**: `<원소>_<형태>` — 형태는 `ingot` / `ore` 등.
원소 예: `iron, copper, aluminum, titanium, magnesium, nickel, tungsten, zinc, tin, lead, gold, silver`.
(전체 목록은 `frontend/Source/Wanted_Factory/Data/RecipeTable.csv` 기준)

---

### 3-2. `process_optimizer` — 공정 최적화

```json
{
  "type": "agent.request",
  "agent": "process_optimizer",
  "payload": {
    "machines": [
      { "id": "iron-smelter-1", "throughput": 45, "capacity": 100 },
      { "id": "assembler-2", "throughput": 80, "capacity": 100 }
    ]
  }
}
```

| payload 필드 | 타입 | 필수 | 설명 |
|--------------|------|------|------|
| `machines` | array | **필수** | 분석 대상 설비 목록 |
| `machines[].id` | string | **필수** | 설비 ID |
| `machines[].throughput` | int | 선택 | 현재 처리량 |
| `machines[].capacity` | int | 선택 | 최대 용량 |

---

### 3-3. `operator_guide` — 설비/레시피 도움말 (RAG)

```json
{
  "type": "agent.request",
  "agent": "operator_guide",
  "payload": {
    "question": "컨베이어 벨트 속도를 어떻게 조절하나요?",
    "sub_agent": "operator_guide.machine_help"
  }
}
```

| payload 필드 | 타입 | 필수 | 설명 |
|--------------|------|------|------|
| `question` | string | **필수** | 사용자 질문 |
| `sub_agent` | string | 선택 | `operator_guide.machine_help` / `.recipe_explainer` / `.troubleshooter`. 생략 시 도메인 오케스트레이터가 자동 선택 |

---

### 3-4. `quest_generator` — 퀘스트 생성

```json
{
  "type": "agent.request",
  "agent": "quest_generator",
  "payload": {
    "request": "철광석을 모아 첫 생산 라인을 가동하게 하는 퀘스트를 만들어줘.",
    "sub_agent": "quest_generator.production_quest"
  }
}
```

| payload 필드 | 타입 | 필수 | 설명 |
|--------------|------|------|------|
| `request` | string | **필수** | 원하는 퀘스트 설명 |
| `sub_agent` | string | 선택 | `quest_generator.production_quest` / `.economy_quest`. 생략 시 자동 선택 |

---

### 3-5. `new_material_generator` — 신소재 후보 생성 (목표 기반)

구체적인 머신/레시피 없이 **설계 제약(목표)** 만으로 신소재 후보 목록을 LLM이 생성합니다.
(레시피로 단일 물질을 합성하는 `material_generation`과 구분됩니다.)

```json
{
  "type": "agent.request",
  "agent": "new_material_generator",
  "payload": {
    "goal": "고온 내성 경량 합금",
    "constraints": ["저비용", "재활용 가능"]
  }
}
```

| payload 필드 | 타입 | 필수 | 설명 |
|--------------|------|------|------|
| `goal` | string | 선택 | 신소재 목표/용도. LLM 부재 시 fallback의 `role`로 사용됨 |
| 그 외 키 | any | 선택 | 자유 형식 제약 조건. 프롬프트에 그대로 전달됨 |

응답 payload는 `materials` 배열입니다(각 항목: `name` / `role` / `rarity` / `production_notes`).
LLM 실패 시 deterministic fallback이 후보 1개를 반환합니다.

```json
{ "materials": [ { "name": "...", "role": "...", "rarity": "...", "production_notes": "..." } ] }
```

---

## 4. `context` (선택) — 메타데이터 및 LLM 오버라이드

런타임에서 컨텍스트 메타데이터로 전달됩니다. 테스트 콘솔은 다음 키를 오버라이드로 주입합니다.

```json
{
  "context": {
    "model": "gpt-5.4-nano",
    "temperature": 0.2,
    "max_tokens": 2048,
    "system_prompt": "…",
    "user_prompt": "…"
  }
}
```

모두 선택값이며, 지정하지 않으면 서버 기본 LLM 설정을 사용합니다.

---

## 5. 응답 / 오류 엔벨로프 (참고)

**성공** — `AgentResponseEnvelope`

```json
{
  "type": "agent.response",
  "request_id": "unreal-smoke-1",
  "session_id": "player-123",
  "client_id": "unreal-client",
  "agent": "material_generation",
  "payload": { "result_type": "new_material", "material_id": "mat_...", "visual_status": "pending" },
  "streams": []
}
```

**오류** — `AgentErrorEnvelope`

```json
{
  "type": "agent.error",
  "request_id": "unreal-smoke-1",
  "session_id": "player-123",
  "client_id": "unreal-client",
  "agent": "material_generation",
  "error": { "code": "INVALID_REQUEST_PAYLOAD", "message": "…" }
}
```

주요 오류 코드: `INVALID_ENVELOPE`, `INVALID_REQUEST_PAYLOAD`, `INVALID_SUB_AGENT`.

---

## 6. 최소 예시 (복붙용)

신물질 합성 1건:

```json
{
  "type": "agent.request",
  "agent": "material_generation",
  "session_id": "player-123",
  "payload": {
    "machine_type": "Synthesizer",
    "inputs": [
      { "item_id": "titanium_ingot", "qty": 1 },
      { "item_id": "aluminum_ingot", "qty": 2 }
    ],
    "generate_visual_asset": true
  }
}
```
