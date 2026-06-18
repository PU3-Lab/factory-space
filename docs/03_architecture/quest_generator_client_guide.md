# Quest Generator 클라이언트 연동 가이드

> 대상: 언리얼(Unreal) 클라이언트 등 Quest Generator API를 호출하는 모든 클라이언트
> 백엔드 모듈: `backend/src/agents/quest_generator`
> 라우터: `quest_router.py` (마운트 prefix: `/api/v1`)

---

## 1. 개요

Quest Generator는 공장(factory)의 현재 상태 스냅샷을 받아, 메인 퀘스트 진행에 도움이 되는
**지원 퀘스트(support quest)** 를 생성·관리하는 에이전트입니다. 클라이언트는 아래 3개의 REST
엔드포인트만으로 전체 라이프사이클을 다룹니다.

| # | 동작 | 메서드 · 경로 |
|---|------|---------------|
| 1 | 지원 퀘스트 생성 | `POST /api/v1/factories/{factory_id}/quests/compose-support` |
| 2 | 퀘스트 목록 조회 | `GET /api/v1/factories/{factory_id}/quests` |
| 3 | 진행 이벤트 보고 | `POST /api/v1/factories/{factory_id}/quests/events` |

기본 흐름은 다음과 같습니다.

```
[클라이언트]                                  [백엔드 / Quest Generator]
   │  공장 상태 스냅샷 전송                       │
   ├─ POST .../compose-support ───────────────▶ │ 부족 자원 분석 → 초안 생성 → 검증
   │                                            │ → LLM 문구 윤색 → 재검증 → 영속화
   │ ◀──────────────── QuestInstance(201) ───── │
   │                                            │
   │  아이템을 모을 때마다                          │
   ├─ POST .../events ────────────────────────▶ │ 멱등 진행도 갱신
   │ ◀──────────────── 204 No Content ───────── │
   │                                            │
   │  UI 갱신이 필요할 때                          │
   ├─ GET .../quests ─────────────────────────▶ │ 전체 퀘스트 목록 반환
   │ ◀──────────────── QuestInstance[] ──────── │
```

---

## 2. 엔드포인트 상세

### 2.1 지원 퀘스트 생성 — `POST /factories/{factory_id}/quests/compose-support`

공장 상태를 분석해 **가장 우선순위가 높고 달성 가능한** 지원 퀘스트 1개를 생성합니다.

**요청 바디 — `QuestContext`**

| 필드 | 타입 | 필수 | 설명 |
|------|------|:----:|------|
| `factory_id` | string | ✅ | 공장 식별자 |
| `factory_level` | int (≥1) | ✅ | 공장 레벨 |
| `current_main_quest` | object \| null | | 현재 진행 중인 메인 퀘스트 (아래 표 참고) |
| `inventory` | `{ [item_id]: int }` | ✅ | 인벤토리 아이템 수량 매핑 |
| `recent_production` | `{ [item_id]: int }` | | 최근 생산한 아이템 수량 매핑 (기본 `{}`) |
| `unlocked_machines` | string[] | | 해금된 기계 목록 (기본 `[]`) |
| `unlocked_recipes` | string[] | | 해금된 레시피 목록 (기본 `[]`) |
| `active_support_quest_ids` | string[] | | 진행 중인 지원 퀘스트 ID 목록 (기본 `[]`) |
| `completed_quest_ids` | string[] | | 완료된 퀘스트 ID 목록 (기본 `[]`) |
| `known_issues` | `KnownIssue[]` | | 구조화된 자원 부족 이슈 목록 (기본 `[]`) |

> **주의:** 모든 스키마는 `extra="forbid"` 입니다. 정의되지 않은 필드를 보내면 **422**로 거부됩니다.

`current_main_quest` (`CurrentMainQuest`)

| 필드 | 타입 | 설명 |
|------|------|------|
| `quest_id` | string | 메인 퀘스트 식별자 |
| `title` | string | 메인 퀘스트 제목 |
| `objectives` | `MainQuestObjective[]` | 목표 목록 |

`MainQuestObjective` = `{ main_objective_id, objective_type, item_id, required(>0), current(≥0) }`

`KnownIssue` = `{ item_id, shortage_amount(>0), main_objective_id, producible(bool) }`

**요청 예시**

```json
{
  "factory_id": "factory_001",
  "factory_level": 3,
  "current_main_quest": {
    "quest_id": "mq_iron_plate",
    "title": "철판 100개 납품",
    "objectives": [
      {
        "main_objective_id": "mobj_1",
        "objective_type": "collect_item",
        "item_id": "resource_iron_plate",
        "required": 100,
        "current": 20
      }
    ]
  },
  "inventory": { "resource_iron_ore": 40, "resource_iron_plate": 20 },
  "recent_production": { "resource_iron_plate": 5 },
  "unlocked_machines": ["smelter"],
  "unlocked_recipes": ["recipe_iron_plate"],
  "active_support_quest_ids": [],
  "completed_quest_ids": [],
  "known_issues": [
    {
      "item_id": "resource_iron_plate",
      "shortage_amount": 80,
      "main_objective_id": "mobj_1",
      "producible": true
    }
  ]
}
```

**성공 응답 — `201 Created`, 바디 `QuestInstance`** (스키마는 [4장](#4-questinstance-스키마) 참고)

**서버 처리 정책 (클라이언트가 알아야 할 부분)**

1. 공장당 **active 지원 퀘스트는 최대 3개**(`MAX_ACTIVE_SUPPORT_QUESTS`)로 제한됩니다.
2. 이미 active한 퀘스트의 타겟 아이템과 **동일한 아이템**은 중복 생성되지 않습니다.
3. 부족 자원이 없거나 후보가 모두 차단되면 생성에 실패합니다.
4. 생성된 초안은 검증(feasibility)을 통과한 뒤 LLM으로 문구만 윤색됩니다.
   목표 수량·보상 등 **수치 값은 LLM이 바꿀 수 없도록 원본으로 강제 고정**됩니다.

**에러 응답**

| 상태 | `detail` | 의미 |
|------|----------|------|
| 400 | `Active support quest limit exceeded (maximum 3)` | active 퀘스트 상한 초과 |
| 400 | `No candidate support quests could be generated ...` | 부족 자원 없음 / 전부 이미 active |
| 400 | `No valid support quest draft passed the feasibility conditions` | 검증 통과 초안 없음 |
| 422 | (Pydantic 검증 오류) | 요청 바디 형식 오류 |

> 클라이언트는 **400을 "지금은 생성할 퀘스트가 없음"** 으로 정상 처리하고, UI에서 조용히 무시하거나
> 다음 스냅샷에서 재시도하면 됩니다. 사용자에게 에러 팝업을 띄울 필요는 없습니다.

---

### 2.2 퀘스트 목록 조회 — `GET /factories/{factory_id}/quests`

해당 공장에 발급된 **모든 상태**(`in_progress` / `completed` / `reward_claimed`)의 퀘스트를 반환합니다.

- **응답:** `200 OK`, 바디 `QuestInstance[]`
- 퀘스트 보드 / HUD 진입 시 또는 폴링 주기로 호출해 UI 상태를 동기화하는 용도입니다.

---

### 2.3 진행 이벤트 보고 — `POST /factories/{factory_id}/quests/events`

플레이어가 아이템을 모을 때 발생하는 이벤트를 보고하면, 매칭되는 퀘스트의 진행도가 갱신됩니다.

**요청 바디 — `ItemCollectedEvent`**

| 필드 | 타입 | 설명 |
|------|------|------|
| `event_id` | string | 이벤트 고유 식별자 (멱등 키) |
| `factory_id` | string | 공장 식별자 |
| `item_id` | string | 획득한 아이템 ID |
| `current_total` | int (≥0) | 현재 아이템 **총 보유량** (증분이 아닌 누적 총량) |

```json
{
  "event_id": "evt_8f3a21",
  "factory_id": "factory_001",
  "item_id": "resource_iron_plate",
  "current_total": 35
}
```

- **응답:** `204 No Content`
- **멱등성:** 같은 `event_id`로 여러 번 보내도 진행도가 중복 반영되지 않습니다. 네트워크 재시도가 안전합니다.
- `current_total`은 **현재 총 보유량**을 그대로 보내면 됩니다(델타 계산 불필요).

**에러 응답**

| 상태 | `detail` | 의미 |
|------|----------|------|
| 400 | `Path factory_id does not match request body factory_id` | 경로와 바디의 `factory_id` 불일치 |
| 422 | (Pydantic 검증 오류) | 요청 바디 형식 오류 |

> 경로의 `{factory_id}`와 바디 `factory_id`는 **반드시 동일**해야 합니다.

---

## 3. 아이템 ID 규칙

- 자원 아이템 ID는 `resources.csv`의 `resource_*` 형태입니다. (예: `resource_iron_plate`)
- 보상은 MVP 기준 `currency` / `gold` 고정입니다.
- 목표 타입은 MVP 기준 `collect_item` 고정입니다.

---

## 4. `QuestInstance` 스키마

생성/조회 응답으로 내려오는 핵심 객체입니다.

| 필드 | 타입 | 설명 |
|------|------|------|
| `id` | string | 인스턴스 식별자 (`qinst_xxxxxxxx`) |
| `factory_id` | string | 공장 식별자 |
| `quest_type` | string | 퀘스트 유형 (지원 퀘스트는 `support`) |
| `support_type` | string | 지원 퀘스트 세부 유형 (`collect_item`) |
| `related_main_quest_id` | string \| null | 연관 메인 퀘스트 식별자 |
| `title` | string | 퀘스트 제목 (LLM 윤색 결과) |
| `description` | string | 퀘스트 설명 (LLM 윤색 결과) |
| `status` | enum | `in_progress` \| `completed` \| `reward_claimed` |
| `objectives` | `QuestObjective[]` | 목표 목록 |
| `rewards` | `QuestReward[]` | 보상 목록 |
| `created_by` | string | 생성 주체 (예: `rule`) |
| `level_reward_applied` | bool | 레벨 보상 적용 여부 |
| `created_at` | datetime | 생성 일시 (UTC) |
| `completed_at` | datetime \| null | 완료 일시 |

`QuestObjective`

| 필드 | 타입 | 설명 |
|------|------|------|
| `id` | string | 목표 식별자 (`obj_{uuid}`) |
| `type` | `"collect_item"` | 목표 유형 |
| `target_id` | string | 대상 아이템 ID |
| `target_amount` | int (>0) | 목표 수량 |
| `current_amount` | int (≥0) | 현재 수량 |
| `status` | `in_progress` \| `completed` | 목표 상태 |

`QuestReward`

| 필드 | 타입 | 설명 |
|------|------|------|
| `type` | `"currency"` | 보상 유형 |
| `target_id` | string | 보상 대상 (기본 `gold`) |
| `amount` | int (>0) | 보상 수량 |

**응답 예시**

```json
{
  "id": "qinst_1a2b3c4d",
  "factory_id": "factory_001",
  "quest_type": "support",
  "support_type": "collect_item",
  "related_main_quest_id": "mq_iron_plate",
  "title": "철판 80개를 모아 생산 라인을 채우세요",
  "description": "메인 임무를 완수하려면 철판이 더 필요합니다. 80개를 모아보세요.",
  "status": "in_progress",
  "objectives": [
    {
      "id": "obj_7d9e",
      "type": "collect_item",
      "target_id": "resource_iron_plate",
      "target_amount": 80,
      "current_amount": 0,
      "status": "in_progress"
    }
  ],
  "rewards": [
    { "type": "currency", "target_id": "gold", "amount": 500 }
  ],
  "created_by": "rule",
  "level_reward_applied": false,
  "created_at": "2026-06-18T08:30:00Z",
  "completed_at": null
}
```

---

## 5. 권장 클라이언트 연동 흐름

1. **퀘스트 보드 진입** → `GET .../quests`로 현재 상태를 동기화한다.
2. **지원 퀘스트 요청 시점**(메인 퀘스트 진행 중 자원 부족 감지 등) → 공장 상태 스냅샷을 모아
   `POST .../compose-support` 호출.
   - `201`이면 반환된 `QuestInstance`를 보드에 추가.
   - `400`이면 "생성할 퀘스트 없음"으로 간주하고 조용히 넘어감.
3. **아이템 획득 시마다** → `POST .../events`로 `current_total`(현재 총 보유량)을 보고.
   - `event_id`는 클라이언트가 고유하게 생성하고, 재시도 시 **동일 값**을 사용해 멱등성을 보장.
4. 진행도/상태 변화를 UI에 반영할 때 다시 `GET .../quests`로 갱신.

---

## 6. 참고 사항 / 주의

- **인가(auth):** 현재 라우터에는 호출자–`factory_id` 소유권 검증이 `TODO`로 남아 있습니다.
  운영 단계에서는 인증 헤더가 추가될 수 있으니, 클라이언트는 인증 토큰 주입 지점을 미리 확보해 두는 것을 권장합니다.
- **스키마 엄격성:** 모든 요청 바디는 `extra="forbid"`이므로, 불필요한 추가 필드를 보내지 마세요(422 발생).
- **수량 의미:** 이벤트의 `current_total`은 증분이 아니라 **누적 총량**입니다.
- **LLM 윤색 범위:** `title`/`description`만 LLM이 다듬습니다. 목표·보상 수치는 항상 규칙(rule)이 결정한 원본 값입니다.

---

## 부록 — 관련 소스

| 역할 | 파일 |
|------|------|
| 라우터(엔드포인트 정의) | `backend/src/agents/quest_generator/quest_router.py` |
| 요청/응답 스키마 | `backend/src/agents/quest_generator/models.py` |
| 컨텍스트 정규화 | `backend/src/agents/quest_generator/context_builder.py` |
| 초안 생성 규칙 | `backend/src/agents/quest_generator/rule_generator.py` |
| 검증 | `backend/src/agents/quest_generator/validator.py` |
| 문구 윤색(LLM) | `backend/src/agents/quest_generator/phrase_refiner.py` |
| 진행도 추적 | `backend/src/agents/quest_generator/tracker.py` |
| 라이프사이클 관리 | `backend/src/agents/quest_generator/manager.py` |
| 라우터 마운트 | `backend/src/app.py` (`prefix="/api/v1"`) |
