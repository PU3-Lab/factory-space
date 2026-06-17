# Quest Agent 최소 구현(MVP) 작업 계획

> 기반 기획서: [docs/01_planning/quest_agent_implementation_plan_v3.md](../01_planning/quest_agent_implementation_plan_v3.md)
> 작성일: 2026-06-16

## 0. 전제와 스코프 결정

기획서 v3는 "서버에 이미 존재하는 공장 상태(인벤토리·공장레벨·메인퀘스트 진행·해금정보)"를
`QuestContextBuilder`가 읽는다고 가정하지만, **현재 백엔드에는 해당 상태가 전혀 없다.**
(`factory_id` 개념 없음, 인벤토리/레벨/메인퀘스트 영속화 없음, DB는 `recipes` + 소재 테이블뿐)

따라서 이번 작업의 전제를 다음과 같이 확정한다.

| 항목 | 결정 |
|---|---|
| 게임 상태 출처 | **클라이언트(Unreal)가 `agent.request` payload로 전달.** 서버는 게임 상태에 대해 stateless. `QuestContextBuilder`는 payload를 정규화만 한다. |
| 구현 범위 | **기획서 22장 "최소 구현 버전" 먼저.** 지원 퀘스트 1종 end-to-end. 단, 1차 타입은 0.1-#1 결정에 따라 22장의 `produce_item` 대신 **`collect_item`(보유량 달성)** 으로 채택. |
| 제외 | 5종 퀘스트 타입 전체 / WS 푸시 이벤트 / 공장 레벨 진행 / LLM 문장 보강 / 다중 목표 → **후속 단계**. |

서버가 영속화하는 것은 **퀘스트 인스턴스와 진행도뿐**이다(진행도 추적이 이벤트 기반이므로 필요).
인벤토리·해금정보 등은 매 요청 시 클라이언트가 보낸 값을 신뢰한다.

## 0.1 기획 검토 반영 결정

기획서 v3의 논리 결함 8건을 검토해, 아래와 같이 MVP 설계에 보정 반영한다.
(번호는 본 절에서만 사용하는 식별자이며, 각 항목은 해당 Phase에 구체화된다.)

| # | 기획 이슈 | 심각도 | MVP 반영 결정 |
|---|---|:---:|---|
| 1 | `produce_item`(생산 흐름) 완료가 메인 목표(재고)를 진행시킨다는 보장 없음 | 🔴 | **MVP 1차 지원 타입을 `collect_item`(보유량 달성)으로 채택.** 보유 총량(current_total) 스냅샷 대입 트래커 구현. R-1 부작용(T-1) 해결을 위해 **(a) 복제본 수용 방식 채택**: `target_amount`를 메인의 `required` 수량으로 정렬해 정합성을 보장하고 복제본을 허용. `shortage_amount`는 지원 퀘스트 설명(description) 생성에 소비. (Phase 3·6) |
| 2 | 생산/달성 *가능성* 미검증 → 달성 불가능 퀘스트 생성 | 🔴 | Validator에 **선행조건 체크** 추가: 부족 아이템이 (원재료/보유중)이거나, 해금 레시피의 입력 재료를 확보 가능한지 확인. 불충족이면 해당 후보를 폐기(skip). (Phase 4) |
| 3 | 이벤트→목표 매칭 규칙·멱등성 미정의 → 오매칭/이중 카운트 | 🔴 | **objective_id 생성 규칙 고정(UUID 스킴)** + 이벤트 스냅샷 대입 기반 매칭 규칙 명시 (이중 카운트가 발생하지 않는 대입 방식). (Phase 4·6) |
| 4 | 보상이 같은 생산 체인 입력으로 순환 → 파밍 악용 | 🟠 | RewardResolver는 **해당 퀘스트 대상 아이템과 그 레시피 입력·중간재를 보상에서 제외.** MVP는 소량 재화(`currency`)만 지급. (Phase 5) |
| 5 | 생성 트리거·쿨다운·상한·만료/실패 조건 미정의 → 퀘스트 스팸 | 🟠 | compose-support에 **factory당 active 지원 퀘스트 상한(MVP 3개)** + 동일 아이템 재생성 차단(기존 중복 규칙 강화). 만료/실패 트리거는 후속. (Phase 7) |
| 6 | 레벨/메인퀘스트 서버 권위 vs stateless 결정 충돌 (치팅) | 🟠 | MVP 범위 밖. 단, **본격 도입 시 메인퀘스트 완료·레벨만 서버 권위로 영속화** 필요를 리스크에 명시. (§7, 후속 §6-3) |
| 7 | 아이템 id 네임스페이스 불일치 (`iron_ingot` vs `resource_iron_ingot`) | 🟡 | item 존재 검증·예시는 **실제 `resources.csv`의 `resource_*` id를 단일 진실로 사용.** 기획서 예시 id는 실데이터 id로 매핑. (Phase 2·4) |
| 8 | `known_issues`가 문자열이라 정보 빈약 | 🟡 | `known_issues`를 **구조화**: `{item_id, shortage_amount, main_objective_id, producible}`. RuleGenerator 재계산 제거. (Phase 2) |

## 1. 현재 코드베이스 사실 정리 (착수 전 확인 완료)

- 기존 `backend/src/agents/quest_generator/`는 **하드코딩 프로토타입**: 예시 퀘스트 10개 중 랜덤 5개 선택([service.py](../../backend/src/agents/quest_generator/service.py)). DB·컨텍스트·진행도 없음.
- 따라야 할 레퍼런스 패턴 = **`material_generation` 모듈**:
  - 자체 `router.py` → `app.py`에서 `/api/v1` prefix로 include
  - `registry/`(또는 repository) = `Session` 받는 classmethod 서비스
  - `schemas.py` = Pydantic 입출력 모델
  - SQLAlchemy 모델은 `backend/src/db/models.py`에 추가, Alembic 마이그레이션은 `backend/migrations/versions/000N_*.py`
- 게임 마스터 데이터는 `data/game/*.csv`에만 존재. `recipes`만 DB(`RecipeModel`)에 적재됨. 자원(item) id는 `data/game/resources.csv`(57종)에 있음 → **item 존재 검증용 경량 로더 필요**.
- LLM은 `backend/src/llm/adapter.py`의 `LLMAdapter` 프로토콜(`invoke`) 사용. (MVP에서는 미사용)
- WS `/ws/agent`는 요청-응답 전용. 서버 푸시 채널 없음 → MVP는 REST 응답으로 진행도 회신.

## 2. 디렉터리 / 파일 구조 (신규)

`material_generation` 레이아웃을 따라 `backend/src/agents/quest_generator/` 하위에 신규 모듈을 둔다.
(기존 프로토타입 파일은 당장 삭제하지 않고, MVP 검증 후 정리)

```
backend/src/agents/quest_generator/
├─ models.py            # Pydantic: QuestContext, CurrentMainQuest, MainQuestObjective, KnownIssue,
│                       #           QuestObjective, QuestReward, SupportQuestDraft, ValidationResult,
│                       #           QuestInstance, QuestProgress
├─ context_builder.py   # payload → QuestContext 정규화 + known_issues(부족분) 계산
├─ rule_generator.py    # collect_item 초안 생성 (MVP: 단일 부족 아이템, 0.1-#1)
├─ validator.py         # QuestValidator
├─ manager.py           # QuestManager (지급/상태전이/완료)
├─ progress_tracker.py  # QuestProgressTracker (item_collected, 멱등 처리 0.1-#3)
├─ reward_resolver.py   # QuestRewardResolver (MVP: 소량 고정 보상)
├─ repository.py        # quest_instance / quest_progress DB 접근 (Session 기반)
├─ game_data.py         # resources.csv & recipes.csv 로더 — 검증용
└─ quest_router.py      # FastAPI 라우터 (compose-support / list / progress event)
```

`backend/src/db/models.py` 에 테이블 모델 추가, `backend/migrations/versions/0004_create_quest_tables.py` 신규.

## 3. 데이터 모델

### 3.1 DB 테이블 (마이그레이션 0004)

기획서 19장을 MVP로 축소. **`quest_instance` + `quest_progress` 2개만** 생성.
(`quest_master`는 메인퀘스트가 클라이언트 소유이므로 불필요, `quest_generation_log`는 후속.)

`QuestInstanceModel` (`quest_instances`):
| 컬럼 | 타입 | 비고 |
|---|---|---|
| `id` | String PK | `qinst_*` |
| `factory_id` | String | 클라이언트 식별자 |
| `quest_type` | String | MVP: `support` 고정 |
| `support_type` | String | MVP: `collect_item` (0.1-#1) |
| `related_main_quest_id` | String nullable | |
| `title` / `description` | String / Text | |
| `status` | String | `in_progress`/`completed`/`reward_claimed`/... server_default `in_progress` |
| `objective_json` | JSON | |
| `reward_json` | JSON | |
| `created_by` | String | `rule` |
| `level_reward_applied` | Boolean | 후속용, default false |
| `created_at` / `completed_at` | DateTime | |

`QuestProgressModel` (`quest_progress`):
| 컬럼 | 타입 |
|---|---|
| `id` | String PK |
| `quest_instance_id` | String FK→quest_instances |
| `objective_id` | String |
| `objective_type` | String (`collect_item`) |
| `target_id` | String (item_id) |
| `current_amount` / `target_amount` | Integer |
| `status` | String (`in_progress`/`completed`) |
| `last_event_id` | String nullable | 멱등 처리용 마지막 반영 이벤트 (0.1-#3) |

### 3.2 Pydantic 모델 (`models.py`)

`QuestContext`, `CurrentMainQuest`, `MainQuestObjective` (`main_objective_id` 포함), `KnownIssue` (`main_objective_id` 포함), `QuestObjective` (`id` 필드 UUID 형태 포함), `QuestReward`, `SupportQuestDraft`, `ValidationResult`, `QuestInstance`, `QuestProgress`.
기획서 5.3 / 7 / 12 예시 구조를 그대로 따른다. `extra="forbid"`로 입력 엄격 검증.

## 4. 구현 순서 (기획서 21장 → MVP 축약)

### Phase 1 — 데이터 모델 + 마이그레이션
- `models.py` Pydantic 정의
- `db/models.py`에 2개 테이블 + `0004_create_quest_tables.py`
- 검증: `alembic upgrade head` (sqlite), 모델 import 테스트

### Phase 2 — game_data 로더 + QuestContextBuilder
- `game_data.py`:
  - `data/game/resources.csv`에서 item id 집합 및 자원명 매핑(`item_id -> 자원명`) 로드(모듈 캐시). **id는 `resource_*` 네임스페이스를 단일 진실로 사용**(0.1-#7). 기획서 예시의 `iron_ingot`류는 `resource_iron_ingot`로 매핑.
  - `recipes.csv`에서 `recipe_id → (출력 item_id, 입력 item_id 목록)` 매핑 로드. #2 선행조건·#4 보상 제외 판정에 사용.
- `context_builder.py`: payload(현재 메인퀘스트 objectives + inventory + unlocked_recipes + active_support_quest_ids) → `QuestContext`
  - `known_issues`를 **구조화**(0.1-#8): 메인 퀘스트 objective 중 `required > current`인 항목마다
    `{item_id, shortage_amount, main_objective_id, producible}` 생성. `producible`은 해당 item 생산 레시피가 `unlocked_recipes`에 있는지.
- 검증: 기획서 5.3 예시 payload로 단위 테스트 (구조화 shortage 계산 + id 매핑)

### Phase 3 — RuleGenerator (collect_item)
- 0.1-#1에 따라 **`collect_item`** 초안 생성(메인 목표가 재고 기반이므로 직접 정렬).
- 부족 아이템(`known_issues`) 중 동일 지원퀘스트가 active가 아닌 것 1개 선택.
- `target_amount = 메인의 required 수량` (0.1-#1/R-1/T-1). **(a) 복제본 수용 결정**에 따라 메인 목표의 required 수량과 일치시켜 정합성을 최우선 확보하며, `shortage_amount`는 target 산정에서 제외(지원 퀘스트 설명 생성 시 소비). `objective_id`는 instance 비의존 스킴(`obj_{uuid}`)으로 생성(0.1-#3).
- `SupportQuestDraft` 생성 및 반환:
  * `title`: `f"{item_name} 확보 지원"` (`item_name`은 `game_data` 로더를 통해 `item_id`에 해당하는 한국어 자원명 조회)
  * `description`: `f"메인 퀘스트 진행을 위해 {item_name} {shortage_amount}개가 더 필요합니다. 총 {target_amount}개를 모으세요."` (U-1의 `shortage_amount`를 설명 소비처로 삼아 dead field 해소 및 U-2 템플릿 정의)
  * `objectives` / `rewards` 정의 (기획서 10.2 collect_item 예시 형태).
- 검증: 부족/중복 분기 + objective_id 독립성 및 생성 단위 테스트.

### Phase 4 — QuestValidator
- 검증 항목(MVP):
  - item_id가 `resources.csv`에 존재(`resource_*`)
  - objective_type 허용(`collect_item`)
  - `target_amount > 0` & 상한(예: ≤1000)
  - 동일목적 active 중복 아님
  - **선행조건 — 달성 가능성**(0.1-#2): 다음 중 하나를 만족하면 통과, 불충족 시 **폐기(skip)** 후 다음 후보로.
    1. 부족 아이템이 이미 인벤토리에 존재
    2. `resources.csv` 상 획득방법이 채굴/채집인 원재료 (레시피가 없는 자원)
    3. 부족 아이템의 생산 레시피가 해금되어 있고, 그 레시피의 입력 재료를 보유(inventory)하고 있거나 생산 가능(입력 재료의 레시피도 해금됨)
- 실패 시 `ValidationResult(valid=False, reason=..., message=...)` (reason 예: `unreachable_prerequisite`, `duplicate_support_quest`)
- 검증: 각 실패/skip 케이스 단위 테스트 (특히 입력 재료 미확보 → skip, 원재료는 패스)

### Phase 5 — QuestManager + Repository + RewardResolver
- `repository.py`: instance/progress 생성·조회·갱신 (material_registry의 Session classmethod 패턴)
- `manager.py`: draft → `QuestInstance`+`QuestProgress` 영속화, status 전이, 완료 처리
- `reward_resolver.py`: MVP는 **소량 재화(`currency`)만 지급**(0.1-#4). 순환 파밍 방지를 위해 대상 아이템 및 그 생산 레시피의 입력·중간재는 보상에서 제외(item 보상 도입 시 적용).
- 검증: 지급→저장→조회 통합 테스트(sqlite) + 보상이 대상 아이템/입력재를 포함하지 않음 단위 테스트

### Phase 6 — QuestProgressTracker
- `item_collected` 이벤트(`event_id`,`factory_id`,`item_id`,`current_total`) 수신.
- **매칭 규칙**(0.1-#3): 해당 factory의 `in_progress` + 같은 `target_id` 목표 중 **가장 오래된(created_at) 1개**에 반영.
- **멱등성 및 대입 방식**(0.1-#3): `current_amount = current_total` 로 대입 처리하여 누적 계산 오류 방지. 구식 이벤트 순서 꼬임 방지를 위해 `last_event_id == event_id`이면 무시하고 반영 시 `last_event_id` 갱신.
- target 도달 시 progress·instance `completed`, `completed_at` 기록.
- 검증: 보유량 대입 → 완료 전이 + **동일 event_id 재전송 시 상태 불변** 통합 테스트

### Phase 7 — Quest API 라우터 + 앱 통합
- `quest_router.py`:
  - `POST /api/v1/factories/{factory_id}/quests/compose-support` — payload(QuestContext 원천) → ContextBuilder→RuleGenerator→Validator→Manager, 생성된 instance 반환 (기획서 17.3).
    **스팸 방지**(0.1-#5): factory당 active 지원 퀘스트 **상한 3개** 초과 시 생성 거부, 동일 아이템 재생성 차단.
  - `GET  /api/v1/factories/{factory_id}/quests` — 목록 조회 (17.1)
  - `POST /api/v1/factories/{factory_id}/quests/events` — `item_collected` 수신 → ProgressTracker (WS 푸시 대체, MVP)
- `app.py`에 `include_router(quest_router, prefix="/api/v1")` 추가
- 검증: FastAPI TestClient로 compose→event→completed 시나리오 E2E

## 5. 테스트 전략

- 위치: `backend/tests/agents/` (기존 패턴), 파일 `test_quest_*`.
- 단위: context_builder / rule_generator / validator 분기.
- 통합: manager+repository+progress_tracker (sqlite in-memory 또는 임시 파일 DB).
- E2E: TestClient로 compose-support → events(item_collected ×N) → GET 목록에서 completed 확인.
- MVP 수용 기준: 부족 아이템 1개 → `collect_item` 퀘스트 생성·검증(선행조건 포함)·저장 → `item_collected`로 보유량 스냅샷 대입(`current_amount = current_total`) → 목표 도달 시 completed → active 상한/중복 차단 동작.

## 6. 후속 단계 (이번 범위 밖, 별도 계획)

1. 나머지 퀘스트 타입: `produce_item`/`setup_machine`/`connect_process`/`stabilize_goal` + 대응 이벤트
2. WebSocket 서버 푸시 채널 신설 → `quest_created`/`quest_progress_updated`/`quest_completed` (기획서 18장)
3. 공장 레벨 진행: `LevelProgressionResolver` + 메인퀘스트 완료 감지 (기획서 16장) — **단, 메인퀘스트/레벨 상태 출처 결정 선행 필요**
4. LLM 문장 보강(제목/설명) + 출력 Validator (기획서 11장)
5. `quest_generation_log` 적재 / 기존 프로토타입 `quest_generator` 정리

## 7. 리스크 / 확인 필요

- **상태 신뢰 모델**: 클라이언트 payload를 신뢰하므로, 진행도(quest_progress)와 클라이언트 인벤토리 간 정합성은 보장되지 않음. MVP에서는 허용, 후속에서 검토.
- **레벨/메인퀘스트 권위 충돌**(0.1-#6): 16장 레벨링은 메인퀘스트 완료를 서버가 권위 있게 감지해야 치팅을 막을 수 있으나, stateless 결정상 서버는 클라이언트 값을 신뢰만 함. **본격 도입 시 메인퀘스트 완료·공장 레벨만은 서버 권위로 영속화할지 선행 결정 필요.** 그때까지 레벨링 보류(중복 레벨업 방지 로직 포함).
- **`produce_item` 의미 미확정**(0.1-#1): MVP는 `collect_item`만 사용. `produce_item` 도입 시 "퀘스트 생성 시점 이후 순증가분"으로 정의를 확정해야 하며, 그렇지 않으면 생산이 즉시 소비되는 체인에서 완료가 메인 목표를 진행시키지 못함.
- **item 존재 검증 소스**: `resources.csv`를 단일 진실로 사용(`resource_*`). 추후 DB ItemTable 적재 시 로더 교체.
- **생성 트리거**: compose-support 호출 주체(스케줄러/조건)는 미정의. MVP는 수동(개발/테스트) 호출 + active 상한으로만 제어. 운영 트리거·만료/실패 조건은 후속.
- **코드 컨벤션**: 작성/수정 후 `ruff check --fix .` 및 `ruff format .` 실행(프로젝트 규칙).
