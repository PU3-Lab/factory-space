# Quest Generator 클라이언트 연동 가이드

> 대상: 언리얼(Unreal) 클라이언트 등 Quest Generator API를 호출하는 모든 클라이언트
> 백엔드 모듈: `backend/src/agents/quest_generator`
> 라우터: `quest_router.py` (마운트 prefix: `/api/v1`)

---

## 1. 개요

Quest Generator는 공장(factory)의 현재 상태 스냅샷을 받아, 메인 퀘스트 진행에 도움이 되는
**지원 퀘스트(support quest)** 를 생성·관리하는 에이전트입니다. 클라이언트는 아래 3개의 REST
엔드포인트만으로 전체 라이프사이클을 다룹니다.

| # | 동작 | 채널 · 경로 |
|---|------|---------------|
| 0 | **튜토리얼 완료 → 지원 퀘스트 트리거** | **WS** `/ws/agent` · `quest.tutorial_completed` (§2.4) |
| 1 | 지원 퀘스트 생성 (직접 호출) | REST `POST /api/v1/factories/{factory_id}/quests/compose-support` |
| 2 | 퀘스트 목록 조회 | REST `GET /api/v1/factories/{factory_id}/quests` |
| 3 | 진행 이벤트 보고 | REST `POST /api/v1/factories/{factory_id}/quests/events` |

> **구현 상태 (2026-06-18 기준)**
> - ✅ **구현 완료:**
>   - REST 1·2·3 (compose-support / list / events), 규칙 기반 생성·검증·문구 윤색·진행도 추적.
>   - WS `quest.tutorial_completed` 핸들러 구현 완료 (§2.4).
>   - 공장 레벨 기반 생성 구현 완료 (목표 수량·보상 스케일링 + 퀘스트 게이트) (§3).

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

**성공 응답 — `201 Created`, 바디 `QuestInstance`** (스키마는 [5장](#5-questinstance-스키마) 참고)

**서버 처리 정책 (클라이언트가 알아야 할 부분)**

1. 공장당 **active 지원 퀘스트는 최대 3개**(`MAX_ACTIVE_SUPPORT_QUESTS`)로 제한됩니다.
2. 이미 active한 퀘스트의 타겟 아이템과 **동일한 아이템**은 중복 생성�### 2.4 (WS) 튜토리얼 완료 → 지원 퀘스트 트리거 — `quest.tutorial_completed`

튜토리얼은 **클라이언트에서 전부 진행**하고, 완료되면 **이미 열려 있는 `/ws/agent` 연결로** 완료 신호를
보냅니다(HTTP 별도 호출 X). 서버는 이 신호를 트리거로 첫 지원 퀘스트를 생성해 같은 연결로 돌려줍니다.

> 서버는 튜토리얼을 **개별 관리·DB 기록하지 않습니다.** "끝났다"는 신호를 받는 즉시 지원 퀘스트 생성만
> 수행합니다. 단, 생성을 하려면 부족 자원 계산이 필요하므로 **완료 신호 + 그 시점의 공장 스냅샷(`context`)** 을
> 한 메시지로 보내야 합니다.

**클라 → 서버**

```json
{
  "type": "quest.tutorial_completed",
  "request_id": "unreal-tut-1",
  "session_id": "dev-session",
  "client_id": "unreal-client",
  "payload": {
    "context": {
      "factory_id": "factory_001",
      "factory_level": 1,
      "inventory": { "resource_iron_ore": 40, "resource_iron_plate": 20 },
      "current_main_quest": { "...있으면 부족 자원 산출이 더 정확...": "..." }
    }
  }
}
```

| 필드 | 필수 | 설명 |
|------|:--:|------|
| `type` | ✅ | `"quest.tutorial_completed"` 고정. **이 타입 자체가 "튜토리얼 끝남" 플래그** — 별도 튜토리얼 ID 불필요 |
| `request_id` | ✅ | 클라 발급. 응답이 이 값으로 돌아오며, 재시도 시 같은 값 → **멱등** |
| `session_id` / `client_id` | ✅ | 기존 WS 핸드셰이크 값 그대로 |
| `payload.context` | ✅ | 완료 **그 시점**의 공장 스냅샷 = `QuestContext` (§2.1과 동일 스키마). 튜토리얼 직후이므로 `factory_level`은 **1** |

**서버 → 클라** (같은 `request_id`로 셋 중 하나)

| `type` | `payload` | 의미 |
|--------|-----------|------|
| `quest.composed` | `QuestInstance` | 첫 지원 퀘스트 생성됨 |
| `quest.none` | — | 만들 게 없음(부족 자원 없음 등). **정상 상황 — 무시** |
| `agent.error` | 오류 정보 | 형식 오류 등 |

**중복 방지** — 튜토리얼을 개별 추적하지 않으므로 신호가 중복돼도 안전해야 합니다. 두 겹으로 막힙니다:
1. `request_id` 멱등 — 재시도 시 동일 값.
2. 서버 가드 — active 지원 퀘스트 최대 3개 상한 + 동일 아이템 중복 차단(compose-support와 공유).

> **공유 로직:** `quest.tutorial_completed`와 REST `compose-support`는 **동일한 규칙 기반 파이프라인**
> (ContextBuilder → RuleGenerator → Validator → PhraseRefiner → QuestManager)을 호출합니다.
> 트리거 채널만 다를 뿐 생성 결과·정책은 같습니다.

> **이후 진행은 REST:** 첫 퀘스트를 받은 뒤 진행도 보고(`events`)·목록 조회(`list`)는 그대로 REST를
> 사용합니다. WS는 **튜토리얼 완료 트리거 한 지점**에만 적용합니다.

---

## 3. 공장 레벨 모델 (클라이언트 소유)

레벨은 **클라이언트가 소유·관리**합니다. 서버는 레벨을 저장하지 않고, 스냅샷으로 받은 `factory_level`을
그대로 사용해 퀘스트를 생성합니다.

| 시점 | 레벨 |
|------|------|
| 튜토리얼 완료 (`quest.tutorial_completed` 전송) | **1** 로 시작 |
| 메인 퀘스트 1개 완료할 때마다 | **+1** |

- 레벨 증가는 **클라 내부 규칙**이며, 서버로 보내는 별도 "메인 퀘스트 완료" 신호는 없습니다.
  클라가 증가시킨 결과 값을 이후 모든 요청의 `context.factory_level`에 담아 보내면 됩니다.

**레벨이 생성에 미치는 영향** (✅ 구현 완료)
1. **목표 수량·보상 스케일링:** 레벨이 높을수록 `target_amount`와 `reward.amount`가 커집니다.
   - `target_amount = min(10 * factory_level, 1000)` (`producible = True` 일 때)
   - `target_amount = min(10 * factory_level, 10)` (`producible = False` 일 때)
   - `reward.amount = 100 * factory_level` (골드 보상)
2. **생성 가능 퀘스트 게이트:** 아이템별 최소 요구 레벨(`min_level`) 미만인 경우 퀘스트가 생성되지 않고 스킵됩니다.
   - 1단계 자원 (나무, 철광석, 구리 등): 1레벨 이상
   - 2단계 자원 (아연, 납, 주석 등): 2레벨 이상
   - 3단계 자원 (알루미늄, 니켈 등): 3레벨 이상
   - 4단계 자원 (텅스텐, 티타늄 등): 4레벨 이상
   - 5단계 자원 (마그네슘, 금, 은, 우라늄 등): 5레벨 이상이언트에서 전부 진행**하고, 완료되면 **이미 열려 있는 `/ws/agent` 연결로** 완료 신호를
보냅니다(HTTP 별도 호출 X). 서버는 이 신호를 트리거로 첫 지원 퀘스트를 생성해 같은 연결로 돌려줍니다.

> 서버는 튜토리얼을 **개별 관리·DB 기록하지 않습니다.** "끝났다"는 신호를 받는 즉시 지원 퀘스트 생성만
> 수행합니다. 단, 생성을 하려면 부족 자원 계산이 필요하므로 **완료 신호 + 그 시점의 공장 스냅샷(`context`)** 을
> 한 메시지로 보내야 합니다.

**클라 → 서버**

```json
{
  "type": "quest.tutorial_completed",
  "request_id": "unreal-tut-1",
  "session_id": "dev-session",
  "client_id": "unreal-client",
  "payload": {
    "context": {
      "factory_id": "factory_001",
      "factory_level": 1,
      "inventory": { "resource_iron_ore": 40, "resource_iron_plate": 20 },
      "current_main_quest": { "...있으면 부족 자원 산출이 더 정확...": "..." }
    }
  }
}
```

| 필드 | 필수 | 설명 |
|------|:--:|------|
| `type` | ✅ | `"quest.tutorial_completed"` 고정. **이 타입 자체가 "튜토리얼 끝남" 플래그** — 별도 튜토리얼 ID 불필요 |
| `request_id` | ✅ | 클라 발급. 응답이 이 값으로 돌아오며, 재시도 시 같은 값 → **멱등** |
| `session_id` / `client_id` | ✅ | 기존 WS 핸드셰이크 값 그대로 |
| `payload.context` | ✅ | 완료 **그 시점**의 공장 스냅샷 = `QuestContext` (§2.1과 동일 스키마). 튜토리얼 직후이므로 `factory_level`은 **1** |

**서버 → 클라** (같은 `request_id`로 셋 중 하나)

| `type` | `payload` | 의미 |
|--------|-----------|------|
| `quest.composed` | `QuestInstance` | 첫 지원 퀘스트 생성됨 |
| `quest.none` | — | 만들 게 없음(부족 자원 없음 등). **정상 상황 — 무시** |
| `agent.error` | 오류 정보 | 형식 오류 등 |

**중복 방지** — 튜토리얼을 개별 추적하지 않으므로 신호가 중복돼도 안전해야 합니다. 두 겹으로 막힙니다:
1. `request_id` 멱등 — 재시도 시 동일 값.
2. 서버 가드 — active 지원 퀘스트 최대 3개 상한 + 동일 아이템 중복 차단(compose-support와 공유).

> **공유 로직:** `quest.tutorial_completed`와 REST `compose-support`는 **동일한 규칙 기반 파이프라인**
> (ContextBuilder → RuleGenerator → Validator → PhraseRefiner → QuestManager)을 호출합니다.
> 트리거 채널만 다를 뿐 생성 결과·정책은 같습니다.

> **이후 진행은 REST:** 첫 퀘스트를 받은 뒤 진행도 보고(`events`)·목록 조회(`list`)는 그대로 REST를
> 사용합니다. WS는 **튜토리얼 완료 트리거 한 지점**에만 적용합니다.

---

## 3. 공장 레벨 모델 (클라이언트 소유)

레벨은 **클라이언트가 소유·관리**합니다. 서버는 레벨을 저장하지 않고, 스냅샷으로 받은 `factory_level`을
그대로 사용해 퀘스트를 생성합니다.

| 시점 | 레벨 |
|------|------|
| 튜토리얼 완료 (`quest.tutorial_completed` 전송) | **1** 로 시작 |
| 메인 퀘스트 1개 완료할 때마다 | **+1** |

- 레벨 증가는 **클라 내부 규칙**이며, 서버로 보내는 별도 "메인 퀘스트 완료" 신호는 없습니다.
  클라가 증가시킨 결과 값을 이후 모든 요청의 `context.factory_level`에 담아 보내면 됩니다.

**레벨이 생성에 미치는 영향** (✅ 구현 완료)
1. **목표 수량·보상 스케일링:** 레벨이 높을수록 `target_amount`와 `reward.amount`가 커집니다.
   - `target_amount = min(10 * factory_level, 1000)` (`producible = True` 일 때)
   - `target_amount = min(10 * factory_level, 10)` (`producible = False` 일 때)
   - `reward.amount = 100 * factory_level` (골드 보상)
2. **생성 가능 퀘스트 게이트:** 아이템별 최소 요구 레벨(`min_level`) 미만인 경우 퀘스트가 생성되지 않고 스킵됩니다.
   - 1단계 자원 (나무, 철광석, 구리 등): 1레벨 이상
   - 2단계 자원 (아연, 납, 주석 등): 2레벨 이상
   - 3단계 자원 (알루미늄, 니켈 등): 3레벨 이상
   - 4단계 자원 (텅스텐, 티타늄 등): 4레벨 이상
   - 5단계 자원 (마그네슘, 금, 은, 우라늄 등): 5레벨 이상

---

## 4. 아이템 ID 규칙
>>>>>>> b0ec402 (feat(quest): Quest WS 튜토리얼 트리거 및 레벨링 기능 구현 및 리뷰 피드백 반영)

- 자원 아이템 ID는 `resources.csv`의 `resource_*` 형태입니다. (예: `resource_iron_plate`)
- 보상은 MVP 기준 `currency` / `gold` 고정입니다.
- 목표 타입은 MVP 기준 `collect_item` 고정입니다.

---

## 5. `QuestInstance` 스키마

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

## 6. 권장 클라이언트 연동 흐름

0. **튜토리얼 완료(클라 진행)** → 레벨을 **1**로 두고, `/ws/agent`로 `quest.tutorial_completed`
   (+ 공장 스냅샷)를 전송. 응답 `quest.composed`의 `QuestInstance`를 보드에 추가(`quest.none`이면 무시).
1. **퀘스트 보드 진입** → `GET .../quests`로 현재 상태를동기화한다.
2. **추가 지원 퀘스트가 필요할 때** → 공장 스냅샷을 모아 `POST .../compose-support` 호출.
   - `201`이면 반환된 `QuestInstance`를 보드에 추가.
   - `400`이면 "생성할 퀘스트 없음"으로 간주하고 조용히 넘어감.
3. **아이템 획득 시마다** → `POST .../events`로 `current_total`(현재 총 보유량)을 보고.
   - `event_id`는 클라이언트가 고유하게 생성하고, 재시도 시 **동일 값**을 사용해 멱등성을 보장.
4. **메인 퀘스트 1개 완료할 때마다** → 클라가 `factory_level`을 **+1** 하고, 이후 모든 요청 스냅샷에 반영.
5. 진행도/상태 변화를 UI에 반영할 때 다시 `GET .../quests`로 갱신.

---

## 7. 참고 사항 / 주의

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
