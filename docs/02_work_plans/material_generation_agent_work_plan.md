# 신물질 생성 Agent 작업 계획서

> 기획서: [docs/01_planning/material_generation_agent.md](../01_planning/material_generation_agent.md)
>
> 현재 구현 구조: [docs/03_architecture/material_generation_current_structure.md](../03_architecture/material_generation_current_structure.md)
> 작성일: 2026-06-12
> 대상 범위: 기획서 §16 MVP 범위

---

## 0. 현재 상태 (As-Is)

코드베이스를 조사한 결과, 신물질 생성 Agent는 **이름만 있는 스텁(stub)** 상태이다.

| 항목 | 현재 상태 | 위치 |
|---|---|---|
| `NewMaterialGeneratorAgent` | 스텁 (build_prompt + fallback만, 51줄) | `backend/src/agents/new_material_generator.py` |
| 라우터 등록 | 등록됨 | `backend/src/agents/router.py` |
| 카탈로그 등록 | 등록됨 (`new_material_generator`) | `backend/src/agents/agent_catalog.py` |
| WS 테스트 스크립트 | `agent: "material_generation"`로 호출 (id 불일치 주의) | `backend/scripts/ws_test_material_generation.py` |
| Recipe 데이터 | 존재 (2종) | `data/game/recipes.csv`, `frontend/Source/Wanted_Factory/Data/RecipeTable.csv` |
| Machine 데이터 | 존재 | `frontend/Source/Wanted_Factory/Data/MachineTable.csv` |
| DB 인프라 | Alembic + PostgreSQL + pgvector, 마이그레이션 1개(manual_rag)만 존재. **`src` 내 DB 세션/엔진 헬퍼 없음** | `backend/migrations/` |
| experiment/material 테이블 | **없음** | — |

**핵심 격차**: 기획서가 정의한 결정론적 코어(hash, validator, registry, classifier), DB 3개 테이블, LLM 제안/검증 분리, 이벤트/비주얼 파이프라인이 **전무**하다. 현재 스텁은 제약조건 → 소재후보 프롬프트만 던지는 수준으로, 기획서의 "코드가 판정, LLM이 제안, 코드가 확정" 구조와 맞지 않는다.

### 기존 패턴 (재사용 대상)

| 패턴 | 참고 파일 |
|---|---|
| Agent 계약 (`agent_id`, `tools`, `build_prompt`, `fallback`) | `agents/base.py` |
| 도메인 패키지 구조 (agent/service/schemas/tools 분리) | `agents/quest_generator/` |
| Pydantic 검증 스키마 | `agents/quest_generator/schemas.py` |
| 서비스 레이어 | `agents/quest_generator/service.py` |
| CSV 리포지토리 (dataclass 레코드) | `agents/operator_guide/csv_repository.py` |
| LLM 어댑터 (`invoke(prompt) -> str \| None`) | `llm/adapter.py` |
| Alembic 마이그레이션 | `migrations/versions/0001_*.py` |
| LangGraph 실행 파이프라인 | `agents/pipeline/runtime.py` |

---

## 1. 목표 아키텍처 (To-Be)

기획서 §9의 구성요소를 기존 `quest_generator/` 패턴에 맞춰 패키지화한다.

```
backend/src/agents/material_generation/
 ├─ __init__.py
 ├─ agent.py                  # MaterialCreationAgent — 서브그래프 보유 + 노드 바디(기존 synthesize() 분해)
 ├─ graph.py                  # ★ §3 LangGraph 서브그래프 builder (StateGraph + 조건부 엣지 + 검증 루프)
 ├─ graph_state.py            # ★ MaterialGraphState (TypedDict: db, request, hash, classification, proposal, attempt 등)
 ├─ schemas.py                # Pydantic: 입력/제안/결과/응답 모델
 ├─ normalizer.py             # 입력 정규화 + experiment_hash / material_hash
 ├─ recipe_repository.py      # recipes DB 테이블 조회 + known_items + recipe key 매칭
 ├─ prevalidator.py           # RecipePreValidator (결정론적 검증)
 ├─ classifier.py             # ExperimentClassifier (룰 기반 분류)
 ├─ similarity.py             # ExperimentSimilarityService
 ├─ proposal_generator.py     # LLM MaterialProposalGenerator (build_prompt + 파싱)
 ├─ result_validator.py       # MaterialResultValidator
 ├─ registry/
 │   ├─ experiment_registry.py   # ExperimentRegistryService (DB)
 │   └─ material_registry.py      # MaterialRegistryService (DB)
 ├─ events.py                 # MaterialEventPublisher (MaterialCreated)
 └─ visual_pipeline.py        # VisualAssetPipeline (비동기 후처리)

backend/src/db/                # ★ 신규: src 내 DB 세션/엔진 헬퍼 (현재 없음, sync)
 ├─ __init__.py
 ├─ engine.py                 # sync engine + sessionmaker (psycopg)
 └─ models.py                 # SQLAlchemy 테이블 정의 (recipes + 신물질 3종)

backend/scripts/
 └─ ingest_recipes.py         # ★ 신규: RecipeTable.csv → recipes 테이블 적재 (manual_rag ingestion 패턴)

backend/migrations/versions/
 └─ 0002_create_material_generation_tables.py   # 4개 테이블 (recipes 포함)

backend/tests/agents/material_generation/       # 단계별 테스트
```

> **결정 반영(2026-06-12)**
> - **Recipe 저장소**: `RecipeTable.csv`(authoring 원본) → `recipes` DB 테이블로 ingest, 에이전트는 DB 조회. known_items도 DB에서 파생. (기존 `manual_rag` CSV→DB 패턴과 동일)
> - **DB 접근**: **sync SQLAlchemy** (psycopg 드라이버). 기존 최상위 파이프라인이 전부 sync이므로 정합. ⚠️ 현재 async로 작성된 코드(`synthesize()`, `db/engine.py`, registry, REST 라우터)는 **sync로 변환 필요**.
> - **§3 흐름 구현**: **LangGraph 서브그래프**(sync 노드)로 구현 (노드가 많고 검증 재시도 루프가 있어 그래프가 적합). 기존 `synthesize()` 순차 로직을 sync 노드/엣지로 재구성.
> - **통신 채널**: **WebSocket 단독**. 최상위 sync LangGraph 파이프라인이 §3 서브그래프를 `graph.invoke`로 직접 실행. REST 엔드포인트(`material_generation/router.py`)는 개발/테스트용으로만 유지(선택), 정식 채널 아님.

---

## 1-A. LangGraph 구현 계획 (★ WS 단독, §3 서브그래프)

### 배경 / 현재 격차

- 최상위 파이프라인(`pipeline/runtime.py`)은 이미 LangGraph `StateGraph`이고 **전부 sync** (`graph.invoke`, 노드·LLM 모두 동기). WS 게이트웨이는 async지만 `pipeline.run()`을 동기 호출(`gateway.py:85`). → **sync 방식이 이 구조와 그대로 정합.**
- §3 흐름은 현재 `agent.py`의 `synthesize()`로 **async**로 작성돼 있으나, 최상위 파이프라인의 material 분기는 이를 호출하지 않고 **제네릭 단발 LLM 경로**로 빠진다 → WS로는 §3 흐름이 실행되지 않는 **통합 구멍**. + async라 sync 파이프라인에 직접 못 꽂힘.
- **방침: 전부 sync.** 기존 async 코드(`synthesize`, `db/engine`, registry, REST 라우터)를 sync로 변환하고, §3을 **sync 서브그래프**로 재구성해 sync 파이프라인에 `graph.invoke`로 직결.

### 목표 구조

§3 흐름을 LangGraph **sync 서브그래프**로 만들고, 최상위 파이프라인의 material 분기에서 **단일 sync 노드**가 이 서브그래프를 실행한다. 세션 수명은 이 노드가 `with get_db_session()`으로 책임진다. async 경계 자체가 없어진다.

```
최상위 파이프라인 (기존 sync 그대로, ainvoke 불필요)
  … route_top_agent → (material) → 노드 "material.synthesize" [sync]
        └─ with get_db_session() as db:
               result = material_subgraph.invoke({db, request, context})
        └─ result → responsePayload → 기존 응답/캐시/미들웨어 경로 합류

material_subgraph (LangGraph, §3, sync 노드들 — 기존 컴포넌트 재사용)
  normalize → exp_hash → registry_lookup
     ├─(cache hit)──────────────────────────────────→ build_cached_result ─┐
     └─(miss)→ recipe_match                                                 │
                ├─(existing)→ save_existing ──────────────────────────────→┤
                └─(no)→ prevalidate                                         │
                         ├─(invalid)→ save_invalid ───────────────────────→┤
                         └─(valid)→ classify                                │
                            ├─(variation/intermediate/failed)→ handle_rule →┤
                            └─(candidate/ambiguous)→ similarity → llm_propose│
                                                          ↑(retry, 상한 N)  ↓
                                                       ┌── validate_result ──┤
                                              (보정/재생성)        (pass)     │
                                                                  mat_hash    │
                                                                  material_dedup
                                                          ├─(exists)→ link ──→┤
                                                          └─(new)→ register ──→┤
                                                                  save_exp + discovery + event
                                                                  + visual(백그라운드 스레드) ─→┤
                                                                                               END
```

### 핵심 설계 결정

| 항목 | 결정 |
|---|---|
| **sync 정합** | 서브그래프·노드·DB·LLM 전부 sync. 최상위 파이프라인은 **변경 없이** `material.synthesize` sync 노드만 추가해 `graph.invoke`로 직결. async 진입점(`arun`/`ainvoke`)·WS 게이트웨이 변경 불필요. |
| **async→sync 변환 (재작업)** | 현재 async로 작성된 `agent.py synthesize()`, `db/engine.py`, `registry/*`, `proposal_generator`, `material_generation/router.py`를 sync로 변환. `AsyncSession`→`Session`, `async with`→`with`, `await`/`async def` 제거. |
| **DB 세션 수명** | 서브그래프 전체가 하나의 `Session` 공유. `material.synthesize` 노드가 `with get_db_session()`로 열고 닫음. 세션은 `MaterialGraphState`에 담아 노드 간 전달. 커밋/롤백은 종료 노드 또는 컨텍스트 매니저가 처리. |
| **노드 ↔ 기존 컴포넌트** | 이미 작성된 `normalizer`/`prevalidator`/`classifier`/`similarity`/`proposal_generator`/`result_validator`/`registry/*`는 **로직 재사용**(sync 변환 후). `graph.py`의 노드는 이 컴포넌트를 호출하는 얇은 래퍼. = `synthesize()`의 순차 단계를 노드로 분해(재작성 아님). |
| **검증 재시도 루프** | `validate_result` → 조건부 엣지로 `llm_propose`에 되돌아감. `MaterialGraphState.attempt` 카운터로 상한(N회) → 초과 시 거부/실패 결과로 종료. |
| **visual 파이프라인** | 종료 직전 백그라운드 스레드(`threading.Thread`/`concurrent.futures`)로 분리. 실패가 그래프 결과에 전파되지 않음(기획 §10.10). |
| **(알려진 한계)** | sync DB/LLM 호출이 WS 이벤트 루프를 블로킹 — 단, 기존 모든 에이전트가 동일(established pattern). 향후 비차단 필요 시 `asyncio.to_thread`로 `pipeline.run` 오프로딩하는 별도 cross-cutting 작업으로 처리. 이번 범위 밖. |

### 이름 정합성 정리 (필수 선행)

현재 `pipeline/state.py`(`TopRoute`), `graph_edges.py`, `runtime.py`(`validate_material_payload`)는 아직 옛 이름 `new_material_generator`를 사용 → router/catalog의 `material_generation`과 불일치. 통합 전에 일괄 정리.

| 파일 | 수정 |
|---|---|
| `pipeline/state.py` | `TopRoute`의 `new_material_generator` → `material_generation` |
| `pipeline/graph_edges.py` | TOP_ROUTE_MAP / 분기 키 `new_material_generator` → `material_generation` |
| `pipeline/runtime.py` | `validate_material_payload` 반환 `selectedLeafAgent` 및 비교값 → `material_generation` |

---

## 2. 작업 단계 (Phase)

각 단계는 **독립적으로 테스트 가능**하도록 분리했다. Phase 1까지는 LLM/DB 없이 순수 로직으로 검증 가능하다.

### Phase 0 — 데이터 · DB 기반 정비
> 목표: 결정론적 코어가 의존하는 데이터·저장소 토대 마련

| # | 작업 | 산출물 | 완료 기준 |
|---|---|---|---|
| 0.1 | `src/db/` sync 엔진·세션 헬퍼 신설 (psycopg) | `db/engine.py` | 설정에서 DB URL 로딩, `sessionmaker` + `get_db_session()` 컨텍스트 제공 |
| 0.2 | SQLAlchemy 모델 정의 — `recipes` + 신물질 3종 (기획 §13) | `db/models.py` | 4개 테이블 모델 선언 |
| 0.3 | 마이그레이션 작성 (4개 테이블) | `migrations/versions/0002_*.py` | `alembic upgrade head` 성공 |
| 0.4 | Recipe ingestion 스크립트 (`RecipeTable.csv` → `recipes` 테이블) | `scripts/ingest_recipes.py` | CSV 파싱 → upsert, 재실행 멱등(content_hash 기준), known_items 파생 가능한 정규화 컬럼 적재 |
| 0.5 | `recipe_repository.py` — DB 조회 기반 | `recipe_repository.py` | `known_items` 조회(0.6), recipe key 매칭(0.7)을 DB에서 수행 |
| 0.6 | `known_items` 구성 (모든 InputItem + OutputItem 합집합, 기획 §7.1) | `recipe_repository.py` | DB에서 `known_items: set[str]` 반환, 미등록 아이템 판별 |
| 0.7 | Recipe Key 정규화 (`MachineType + sorted(Input:Qty)`, 기획 §7.2) | `recipe_repository.py` | 입력 순서 무관 동일 키, DB 완전 매칭 조회 |

**남은 결정**: ingestion 원본 CSV는 `RecipeTable.csv`(영문 item_id, 기획 §7 스키마 일치)로 확정. `data/game/recipes.csv`(한글)는 operator_guide 전용으로 분리 유지. 두 CSV의 item_id 표기 차이가 있으므로, 향후 한글 데이터까지 통합하려면 매핑 테이블이 별도로 필요 — 이번 범위 밖.

**성능 메모**: `recipes`는 게임 데이터로 규모가 작고 변동이 드물다. recipe key/known_items는 매 요청 DB 조회 대신 **프로세스 시작 시 1회 로딩 후 인메모리 캐시** 권장 (변경 시 재적재). DB는 정본 저장소, 캐시는 조회 가속.

---

### Phase 1 — 결정론적 코어 (LLM 없음)
> 목표: 기획 §3 흐름도의 코드 판정 경로 전체. LLM 호출 없이 기존 레시피·분류·기록 처리 완결

| # | 작업 | 산출물 | 완료 기준 |
|---|---|---|---|
| 1.1 | 입력 정규화 (아이템 정렬, 수량, 공정조건, 기획 §5.1) | `normalizer.py` | 정규화된 입력 dict 반환 |
| 1.2 | `experiment_hash` 생성 (기획 §5) | `normalizer.py` | 입력 순서 무관 동일 해시, 안정적 직렬화 |
| 1.3 | `RecipePreValidator` (완전 매칭 / MachineType / InputItem / InputQty / known_items, 기획 §10.3) | `prevalidator.py` | `existing_recipe` / `invalid_input` / `unknown` 판정 |
| 1.4 | `ExperimentClassifier` (5종 분류, 기획 §10.4 + 장비정책 §11) | `classifier.py` | `simple_variation`/`intermediate_material`/`failed_result`/`candidate`/`ambiguous` 룰 분류. Synthesizer 외 장비는 candidate 직행 금지 |
| 1.5 | `ExperimentRegistryService` 조회/저장 (성공·실패 모두 저장, 기획 §10.2) | `registry/experiment_registry.py` | hash 조회 → 캐시 반환, 신규 기록 insert |
| 1.6 | `ExperimentSimilarityService` (동일 MachineType/주재료/수량차 조회, 기획 §10.5) | `similarity.py` | 유사 실험 리스트 반환 (LLM 컨텍스트용) |
| 1.7 | 서브그래프 골격 — `graph_state.py` + LLM 분기 전까지 노드/엣지 (normalize→exp_hash→registry_lookup→recipe_match→prevalidate→classify) | `graph.py`, `graph_state.py` | 기획 §7.3 우선순위 1~5가 서브그래프 경로로 동작. cache hit / existing / invalid / rule 분기는 각 종료 노드로 빠짐 |

**완료 기준**: 기획 §12 예시 표의 LLM 비호출 케이스(기존 레시피/단순 변형/중간재/실패/장비 부적합)가 전부 코드만으로 올바른 결과를 낸다.

---

### Phase 2 — LLM 제안 + 결정론적 검증
> 목표: `candidate`/`ambiguous`에만 LLM 호출 → 검증 → material_hash → DB 등록

| # | 작업 | 산출물 | 완료 기준 |
|---|---|---|---|
| 2.1 | 제안/결과 Pydantic 스키마 (기획 §10.6 출력 예시) | `schemas.py` | `MaterialProposal`, `MaterialResult` 검증 모델 |
| 2.2 | `MaterialProposalGenerator` — 프롬프트 빌드 + JSON 파싱 (기존 스텁 대체) | `proposal_generator.py` | 실험조합/분류/유사기록/세계관톤 주입, 기존 `LLMAdapter.invoke` 사용 |
| 2.3 | `material_hash` 생성 (기획 §6.1: category/base/속성/희귀도 정규화) | `normalizer.py` | 다른 조합 → 같은 물질이면 동일 해시 |
| 2.4 | `MaterialResultValidator` (Schema·decision enum·이름 중복·material_hash 중복·밸런스·금지결과, 기획 §10.7) | `result_validator.py` | 통과/보정/재생성/거부 판정. 재생성 루프 상한 설정 |
| 2.5 | `MaterialRegistryService` — material_id 생성, 저장, discovery 로그, visual_status=pending (기획 §10.8) | `registry/material_registry.py` | 중복 material_hash → 기존 id 연결, 신규 → insert |
| 2.6 | 서브그래프 LLM 분기 + 검증 재시도 루프 노드/엣지 (similarity→llm_propose→validate_result↺→mat_hash→material_dedup→register, 기획 §7.3 6~9) | `graph.py` | candidate 입력 → 서브그래프로 신물질 생성/등록 end-to-end. `attempt` 상한 동작 |
| 2.7 | LLM 미가용 시 `fallback` 정비 (NoopLLMAdapter 경로) | `proposal_generator.py` | LLM 없이도 안전한 기본 응답 |

**완료 기준**: 기획 §14.3 신물질 생성 응답 형식대로 반환. 동일 조합 재호출 시 §14.4 캐시 응답.

---

### Phase 3 — 이벤트 · 비주얼 파이프라인 (비동기)
> 목표: 생성 후처리. 텍스처 실패가 신물질 생성 실패로 전파되지 않도록 격리

| # | 작업 | 산출물 | 완료 기준 |
|---|---|---|---|
| 3.1 | `MaterialEventPublisher` — `MaterialCreated` 발행 (기획 §10.9) | `events.py` | 발행/구독 인터페이스, 동기 인메모리 우선 |
| 3.2 | `VisualAssetPipeline` — 비동기 실행, 실패 시 fallback 아이콘, asset_key 업데이트 (기획 §10.10) | `visual_pipeline.py` | 실패해도 material 상태 정상, `visual_status` 전이(pending→ready/failed) |
| 3.3 | `visual-status` 조회 (기획 §14.5) | `agent.py`/router | `GET /materials/{id}/visual-status` 응답 |

**완료 기준**: 기획 §17 밸런스 정책 "텍스처 생성 실패는 신물질 생성 실패로 처리하지 않는다" 검증.

---

### Phase 4 — WS 통합 · LangGraph 결선 · 정리
> 목표: §3 서브그래프를 최상위 LangGraph 파이프라인에 연결, WS 단독 채널로 동작 (§1-A 참조)

| # | 작업 | 산출물 | 완료 기준 |
|---|---|---|---|
| 4.1 | 이름 정합성 일괄 정리 (`new_material_generator` → `material_generation`) | `pipeline/state.py`, `graph_edges.py`, `runtime.py` | TopRoute/분기/노드 전부 `material_generation` |
| 4.2 | async→sync 변환 — `synthesize`, `db/engine`, `registry/*`, `proposal_generator`, REST 라우터 | `material_generation/*`, `db/*` | `await`/`async def`/`AsyncSession` 제거, sync로 동작 |
| 4.3 | material 분기 → `material.synthesize` **sync 노드**(서브그래프 `invoke` + `with get_db_session()`) 결선 | `pipeline/runtime.py`, `agent.py`/`graph.py` | WS로 §3 흐름 end-to-end 실행, 제네릭 LLM 경로 대체. 파이프라인 sync 그대로 |
| 4.4 | WS 응답 매핑 — §3 결과 → 응답 포맷(§14.2~14.4) | `runtime.py`/매핑 노드 | `ws_test_material_generation.py`로 신물질/캐시/기존레시피 응답 확인 |
| 4.5 | 카탈로그 설명 검토 (실험/합성 도메인) | `agent_catalog.py` | 라우팅 프롬프트 정확성 (이미 갱신됨, 확인만) |
| (4.6) | (선택) `POST /experiments/material-creation` REST는 sync로 변환 후 dev-only 유지 또는 제거 | `material_generation/router.py` | WS 단독 정책상 정식 채널 아님 |

---

## 3. 의존 관계 / 권장 순서

```
Phase 0 (데이터·DB)
   └─> Phase 1 (결정론 코어)  ──> 여기까지 LLM·DB 없이도 대부분 단위 테스트 가능
          └─> Phase 2 (LLM 제안+검증+등록)
                 └─> Phase 3 (이벤트·비주얼)
                        └─> Phase 4 (API·통합)
```

- Phase 0.1~0.3(Recipe 로더)과 0.4~0.5(DB)는 병렬 가능.
- Phase 1은 DB 없이도 in-memory registry로 선개발 후 0.5 완료 시 교체 가능.
- Phase 3은 Phase 2 완료 후 독립 진행 가능.

---

## 4. 사전 결정 필요 항목 (Open Questions)

**확정 완료 (2026-06-12)**

| # | 항목 | 결정 |
|---|---|---|
| Q1 | Recipe 저장소 | ✅ `RecipeTable.csv` ingest → `recipes` DB 테이블, 에이전트는 DB 조회 (manual_rag 패턴) |
| Q2 | DB 접근 방식 | ✅ **sync SQLAlchemy (psycopg)** — 기존 sync 파이프라인과 정합. 현 async 코드는 sync로 변환(§1-A, Phase 4.2) |
| Q3 | agent_id | ✅ `material_generation`로 통일, 스텁 대체 |

**남은 결정 (MVP 기본값으로 진행, 이견 시 조정)**

| # | 항목 | 선택지 | 제안 |
|---|---|---|---|
| Q4 | 이벤트 버스 | 인메모리 vs 외부 큐 | MVP는 인메모리 (기획 §16 제외 항목과 일치) |
| Q5 | VisualAssetPipeline 백엔드 | 실제 이미지 생성 vs fallback만 | MVP는 fallback 아이콘 + asset_key 스텁, 실제 생성은 후속 |
| Q6 | 연구 레벨의 hash 포함 여부 | 포함 vs 제외 (기획 §5.1 "정책에 따라 결정") | 기본 제외, 정책 확정 시 반영 |

---

## 5. 테스트 전략

| 레벨 | 대상 | 비고 |
|---|---|---|
| 단위 | hash 생성(순서 불변성), recipe key 매칭, classifier 룰, validator 규칙 | LLM/DB 불필요 — Phase 1에서 즉시 작성 |
| 통합 | registry CRUD, 오케스트레이터 분기 | DB 픽스처(테스트 트랜잭션) |
| 계약 | LLM 제안 JSON Schema 검증, 응답 포맷(§14) | NoopLLMAdapter로 결정론적 테스트 |
| E2E | WS/HTTP 합성 시도 → 결과 | 기존 `ws_test_material_generation.py` 확장 |

테스트 위치: `backend/tests/agents/material_generation/`

핵심 회귀 케이스(기획 §12 예시 표 전수):
- `Smelter + iron_ore x2` → 기존 레시피
- `Smelter + iron_ingot + copper_ingot` → 장비 부적합/실패 (candidate 아님)
- `Synthesizer + iron_ingot + copper_ingot` → 신물질 후보 → LLM
- 동일 조합 재시도 → 캐시 반환
- 다른 조합·동일 물질 → material_hash 중복 → 기존 id 연결

---

## 6. MVP 완료 정의 (Definition of Done)

기획 §16 MVP 포함 항목 전체가 다음을 만족:

- [x] RecipeTable.csv → `recipes` DB 적재(ingestion) + known_items + recipe key 매칭 동작
- [x] experiment_hash / material_hash 생성 (순서·재현성 보장)
- [x] ExperimentRegistry 조회·저장 (성공·실패 모두)
- [x] 기존 레시피 완전 매칭 + 미등록 아이템 차단
- [x] ExperimentClassifier 1차 분류 + 장비 정책 반영
- [x] candidate/ambiguous에만 LLM 호출
- [x] LLM 결과 JSON Schema 검증 + MaterialResultValidator 통과
- [x] generated_materials / generated_experiments DB 저장
- [x] MaterialCreated 이벤트 발행
- [x] VisualAssetPipeline 비동기 실행 + 실패 시 fallback 아이콘
- [x] §3 흐름이 LangGraph **sync 서브그래프**로 동작 (검증 재시도 루프 포함)
- [x] async 코드 sync 변환 완료 + material 분기가 서브그래프 실행 (제네릭 LLM 경로 아님)
- [x] `new_material_generator` 잔존 이름 전부 `material_generation`로 정리
- [x] WS 단독 채널로 합성 시도 → 신물질/캐시/기존레시피 응답(§14.2~14.4) 수신
- [x] 기획 §12 예시 표 회귀 테스트 전부 통과

---

## 7. 범위 제외 (기획 §16 준수)

화학 시뮬레이션 / 자동 후속 레시피 밸런싱 / 3D 모델 생성 / 멀티플레이 공유 / 실시간 경제 가격 / 이미지 품질 평가 Agent / 연구 트리 자동 확장 — **이번 작업 범위 아님.**
