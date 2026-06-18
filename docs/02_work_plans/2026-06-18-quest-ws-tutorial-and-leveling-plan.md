# Quest WS 튜토리얼 트리거 + 레벨링 구현 계획

> 기반 합의: [docs/03_architecture/quest_generator_client_guide.md](../03_architecture/quest_generator_client_guide.md) (§2.4 WS 트리거, §3 레벨 모델)
> 선행 계획: [docs/02_work_plans/2026-06-16-quest-agent-mvp-plan.md](2026-06-16-quest-agent-mvp-plan.md) — 본 계획은 그 "후속(WS 푸시·공장 레벨 진행)"에 해당
> 작성일: 2026-06-18

---

## 0. 목표와 스코프

튜토리얼 완료를 **기존 `/ws/agent` WebSocket 연결**로 전달받아 첫 지원 퀘스트를 생성하고,
**클라이언트가 소유하는 공장 레벨**에 따라 퀘스트 생성 결과(수량·보상·게이트)를 달라지게 한다.

| 항목 | 결정 (대화에서 확정) |
|---|---|
| 튜토리얼 완료 채널 | **WS** `quest.tutorial_completed` (HTTP 별도 호출 X). 서버는 튜토리얼을 DB에 기록하지 않음 |
| 트리거 페이로드 | 완료 신호 + 그 시점 공장 스냅샷(`context` = `QuestContext`) |
| 레벨 소유 | **클라이언트.** 튜토리얼 완료 시 1, 메인 퀘스트 완료마다 +1. 서버는 스냅샷 `factory_level`을 그대로 사용 |
| 레벨의 효과 | ① 목표 수량·보상 스케일링 ② 생성 가능 퀘스트(아이템) 게이트 |
| 생성 로직 공유 | WS 트리거와 REST `compose-support`는 **동일 규칙 기반 파이프라인** 호출 |

### 스코프 밖
- `events`(진행도 보고)·`list`(목록 조회)의 WS 전환 — REST 유지.
- 메인 퀘스트 완료의 서버 권위화 — 레벨은 클라 소유이므로 불필요.
- WS 인증/인가(현재 미구현) — 별도 과제(리스크에 명시).

---

## 1. 현재 코드베이스 사실 (착수 전 확인 완료)

| 사실 | 근거 |
|---|---|
| `/ws/agent`는 `agent.request`만 처리. 그 외 타입은 `INVALID_MESSAGE_TYPE` 거부 | [pipeline/runtime.py:191](../../backend/src/agents/pipeline/runtime.py) `validate_envelope` |
| 게이트웨이는 수신 메시지를 무조건 `pipeline.run`으로 전달 | [websocket_gateway/gateway.py](../../backend/src/agents/websocket_gateway/gateway.py) `agent_websocket` 루프 |
| 규칙 기반 compose 파이프라인은 `compose_support` 라우터 **함수 내부에 인라인** | [quest_router.py:46-152](../../backend/src/agents/quest_generator/quest_router.py) |
| `RuleGenerator`는 `factory_level`을 **사용하지 않음**. `target_amount = main.required` 고정, 보상 100 골드 고정 | [rule_generator.py:62,81-85](../../backend/src/agents/quest_generator/rule_generator.py) |
| `ContextBuilder`는 `factory_level`을 읽어 컨텍스트에 담기만 함(기본 1) | [context_builder.py:39,114](../../backend/src/agents/quest_generator/context_builder.py) |
| 서버에 공장 레벨 영속화 없음 | [db/models.py](../../backend/src/db/models.py) — factory 테이블 없음 |
| 컴포넌트별 테스트 존재 | `backend/tests/test_quest_*.py` (router, rule_generator, validator, tracker 등) |

---

## 2. 구현 단계 (TDD 권장: 각 단계 테스트 먼저)

### Phase 1 — compose 로직을 공용 서비스로 추출 (동작 보존 리팩터)

**문제:** compose 파이프라인이 REST 라우터 함수에 인라인이라 WS에서 재사용 불가.

**작업**
- `quest_generator`에 공용 서비스 함수 신설 (예: `compose_service.py`의 `compose_first_support_quest(session, factory_id, context_payload) -> ComposeResult`).
- 현재 [quest_router.py:66-152](../../backend/src/agents/quest_generator/quest_router.py)의 로직(상한 검사 → 활성 타겟 수집 → ContextBuilder → RuleGenerator → Validator → PhraseRefiner → 재검증 → QuestManager)을 **그대로** 이 함수로 이동.
- 반환 타입을 결과 객체로 정의해 채널별 매핑을 분리:
  ```
  ComposeResult = { outcome: "composed" | "none", instance: QuestInstance | None, reason: str | None }
  ```
  - `outcome="none"` 사유(reason): `limit_exceeded` / `no_candidates` / `no_valid_draft`.
- `compose_support` 라우터는 이 함수를 호출해 **기존 HTTP 동작을 보존**:
  - `composed` → `201` + instance
  - `none` → 사유별 기존 `400` detail 그대로 매핑 (외부 계약 불변).

**테스트**
- 신규 `test_quest_compose_service.py`: composed / 각 none 사유 분기.
- 기존 `test_quest_router.py` **전부 그대로 통과** (회귀 없음 = 추출 성공 기준).

**완료 기준:** REST `compose-support` 응답·상태코드가 리팩터 전후 동일.

---

### Phase 2 — WS `quest.tutorial_completed` 핸들러

**작업**
- 게이트웨이 수신 루프에 **타입 분기** 추가: `message.type`이 `quest.`로 시작하면 LLM `pipeline.run` 대신 quest 디스패처로 라우팅, 그 외는 기존대로.
  - 위치: [gateway.py](../../backend/src/agents/websocket_gateway/gateway.py) `agent_websocket`의 `json.loads` 직후, `pipeline.run` 호출 앞.
- quest 디스패처 (예: `websocket_gateway/quest_dispatch.py`):
  - `quest.tutorial_completed` 수신 → `payload.context`에서 `factory_id` 추출 → `with get_db_session()` → Phase 1의 `compose_first_support_quest` 호출.
  - 동기 함수이므로 게이트웨이의 기존 패턴대로 `asyncio.to_thread`로 실행(이벤트 루프 비차단).
  - 응답 매핑:
    | ComposeResult | WS 응답 `type` | payload |
    |---|---|---|
    | composed | `quest.composed` | `QuestInstance` |
    | none | `quest.none` | `{ reason }` |
    | 예외/형식오류 | `agent.error` | `build_error_payload(...)` |
  - 응답 봉투에 요청의 `request_id`/`session_id`/`client_id` 반향(echo).
- 멱등: 동일 `request_id` 재시도 시 중복 생성 방지. (1차는 서버 가드(상한 3 + 동일아이템 차단)로 충분, 강한 멱등이 필요하면 `request_id` 처리 기록 추가 — 리스크 참조.)

**테스트**
- 신규 `test_quest_ws_dispatch.py`:
  - `quest.tutorial_completed` → `quest.composed` (정상)
  - 부족자원 없음 → `quest.none`
  - 잘못된 payload → `agent.error`
  - `quest.` 아닌 타입은 기존 파이프라인 경로로 감(분기 회귀 없음).
- FastAPI `TestClient`의 `websocket_connect`로 통합 검증.

**완료 기준:** WS로 튜토리얼 완료 메시지 전송 시 `QuestInstance`가 같은 연결로 반환.

---

### Phase 3 — 레벨 기반 생성 (스케일링 + 게이트)

`RuleGenerator.generate_drafts`가 `context.factory_level`을 사용하도록 확장.

**3-A. 보상 스케일링 (낮은 리스크)**
- 현재 고정 100 골드 → 레벨 함수로. 예: `reward = base * level` 또는 레벨→보상 테이블.
- [rule_generator.py:81-85](../../backend/src/agents/quest_generator/rule_generator.py) 수정.

**3-B. 목표 수량 스케일링 (✅ 결정 완료 — 레벨 우선, 단 생산가능성으로 상한)**
- **결정:** 목표 수량을 **레벨에 맞춰 스케일**한다. 기존 `target_amount = main.required` 불변식(U-1)은 **폐기**하고, 지원 퀘스트 목표를 메인 `required`에서 **분리**한다. (사용자: "레벨에 맞는 목표 수량이 더 중요")
- 공식: `target_amount = f(factory_level)` (예: `base × level` 또는 레벨→수량 테이블). 구체 커브는 밸런싱 파라미터로 별도 조정(코드 상수로 시작).
- [rule_generator.py:62](../../backend/src/agents/quest_generator/rule_generator.py) `target_amount = main_objective.required` → 레벨 함수로 교체.

- **⚠️ 생산가능성 상한 (사용자 요구: "생산이 안되는데 너무 큰 수량이 나오면 안됨")**
  - 현재 [Validator](../../backend/src/agents/quest_generator/validator.py)는 **도달가능성(reachable)만** 검사하고 *수량 현실성*은 안 봄. 그래서 레벨 스케일을 그대로 두면 생산 불가 아이템에 과대 목표가 나옴.
  - 규칙: `KnownIssue.producible`로 분기해 **상한을 다르게** 적용.

    | 조건 | 목표 수량 |
    |---|---|
    | `producible = True` (해금 레시피로 생산 가능) | `min( f(level), HARD_CAP )` — 레벨 스케일 정상 적용 |
    | `producible = False` (생산 불가 — 원재료/미해금) | `min( f(level), NONPRODUCIBLE_CAP )` — **낮은 상한으로 캡**. 예: `shortage_amount` 또는 소량 고정값 이하 |

  - `HARD_CAP`은 Validator의 기존 절대 상한(1~**1000**, [validator.py:62](../../backend/src/agents/quest_generator/validator.py))을 넘지 않게. 생성기에서 **선제적으로 캡**해 퀘스트가 거부되지 않고 합리적 수량으로 나오게 함.
  - `NONPRODUCIBLE_CAP` 구체값은 밸런싱 상수(Phase 3 구현 시 결정).

- **영향:** 지원 퀘스트 완료가 메인 `required`를 1:1로 채우지 않을 수 있음(의도된 분리 — 모은 아이템은 인벤토리에 쌓여 메인 진행에 여전히 기여). 설명문(description)은 레벨 기반 목표 수량 기준으로 재작성. `shortage_amount`는 description·비생산 상한 참고용으로 유지.

**3-C. 생성 가능 퀘스트 게이트**
- 레벨→최소요구레벨 매핑 도입(예: [game_data.py](../../backend/src/agents/quest_generator/game_data.py)에 `item min_level` 테이블).
- `generate_drafts` 순회 시 `min_level(item) > factory_level`인 `known_issue`는 skip.

**테스트**
- `test_quest_rule_generator.py` 확장:
  - 레벨↑ → 보상↑·목표수량↑ (스케일)
  - **생산가능성 상한:** `producible=True`는 레벨대로 큰 목표, `producible=False`는 `NONPRODUCIBLE_CAP` 이하로 캡 (동일 레벨에서 두 케이스 목표 수량 차이 검증)
  - 고레벨에서도 목표 수량이 `HARD_CAP`(≤1000)을 넘지 않음 → Validator 거부 없이 생성됨
  - 게이트: 저레벨에서 고레벨 아이템 퀘스트 미생성, 레벨 충족 시 생성
  - 게이트로 후보 0 → 빈 리스트(→ 상위에서 `quest.none`/`400`).

**완료 기준:** 동일 스냅샷에서 `factory_level`만 바꿔 호출 시 보상/생성여부가 규칙대로 달라짐.

---

### Phase 4 — 문서 동기화 & 스모크

- [quest_generator_client_guide.md](../03_architecture/quest_generator_client_guide.md)의 🚧 "미구현" 표기를 구현 완료에 맞춰 갱신.
- WS 메시지 ↔ 실제 응답 예시 일치 확인.
- `ruff check --fix .` + `ruff format .` 실행.

---

## 3. 변경 파일 요약

| 파일 | 변경 |
|---|---|
| `agents/quest_generator/compose_service.py` | **신규** — 공용 compose 함수 + `ComposeResult` |
| `agents/quest_generator/quest_router.py` | compose 인라인 로직 → 서비스 호출로 교체 (외부 계약 불변) |
| `agents/websocket_gateway/gateway.py` | 수신 루프에 `quest.*` 타입 분기 추가 |
| `agents/websocket_gateway/quest_dispatch.py` | **신규** — `quest.tutorial_completed` 핸들러·응답 매핑 |
| `agents/quest_generator/rule_generator.py` | `factory_level` 기반 보상 스케일 + 게이트 |
| `agents/quest_generator/game_data.py` | (게이트용) 아이템 최소 레벨 테이블 |
| `backend/tests/test_quest_compose_service.py` | **신규** |
| `backend/tests/test_quest_ws_dispatch.py` | **신규** |
| `backend/tests/test_quest_rule_generator.py` | 레벨 케이스 확장 |

---

## 4. 열린 결정 / 리스크

| # | 항목 | 심각도 | 내용 · 권장 |
|---|---|:---:|---|
| 1 | ~~목표 수량 스케일 vs `main.required` 불변식~~ | ✅ | **결정 완료:** (b) 지원 목표를 메인 `required`에서 분리해 **레벨 기반으로 스케일**. U-1 불변식 폐기. (Phase 3-B 참조) — 남은 미세결정은 스케일 커브(상수) 뿐 |
| 2 | 게이트용 레벨 데이터 출처 | 🟡 | 아이템별 최소레벨을 어디에 둘지(`game_data` 상수 vs CSV). 1차는 상수 테이블 권장 |
| 3 | WS 강한 멱등성 | 🟡 | 1차는 서버 가드(상한+중복차단)로 충분. 네트워크 재전송에서 완벽 멱등이 필요하면 `request_id` 처리 기록 도입 |
| 4 | WS 인증/인가 | 🟠 | 현재 `/ws/agent`·REST 모두 `factory_id` 소유권 검증 `TODO`. 본 과제 범위 밖이나 운영 전 필수 |
| 5 | `quest.none` 정보량 | ⚪ | 클라가 사유별 분기를 원하면 `reason` 코드 노출, 아니면 단순 무시 |

---

## 5. 작업 순서 권고

Phase 1 (리팩터·회귀0) → Phase 2 (WS 경로) → Phase 3 (레벨) → Phase 4 (문서·정리).
각 Phase는 독립 PR 가능. Phase 1은 외부 동작 불변이라 안전하게 선행.

**열린 결정 모두 해소 — 착수 가능.** (목표 수량은 레벨 기반 스케일로 확정. 스케일 커브 상수만 Phase 3 구현 시 정하면 됨.)
