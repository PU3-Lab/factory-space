# 일반·생성 레시피 저장소 통합 설계

| 항목 | 내용 |
| --- | --- |
| 작성일 | 2026-06-13 |
| 대상 브랜치 | `feature/material-generation-sync` |
| 상태 | 설계안 |
| 대상 영역 | `backend/src/db`, `backend/src/agents/material_generation`, Recipe ingestion |

## 1. 결정 요약

모든 실제 제작법은 `recipes`를 단일 정본(source of truth)으로 사용한다.

- 기존 CSV 제작법: `source=authored`, `status=approved`
- 신물질 생성 과정에서 만들어진 제작법: 검증 후 `source=generated`, `status=draft`
- 게임의 레시피 매칭과 생산에는 `status=approved`인 행만 사용
- 신물질 자체, 합성 시도, 플레이어 발견 이력은 레시피와 의미가 다르므로 별도 테이블 유지

즉, **제작법 저장소만 통합**하고 다음 도메인은 분리한다.

```text
recipes                     제작 가능한 변환 규칙의 정본
generated_materials         생성된 물질의 속성 및 비주얼 상태
generated_experiments       합성 시도와 성공·실패 감사 이력
generated_material_discoveries
                            플레이어별 물질 발견 이력
```

## 2. 현재 문제

현재 일반 레시피는 `recipes`에 저장되지만 신물질 관련 정보는 다음 위치에 흩어져 있다.

- `generated_materials.recipe_candidates_json`: LLM이 제안한 다음 레시피 후보 문자열
- `generated_experiments`: 신물질을 만든 실험 기록
- `recipes`: CSV에서 적재한 일반 레시피만 저장

`recipe_candidates_json`은 이름 후보일 뿐 장비, 입력량, 출력량, 제작시간을 갖춘 실행 가능한 제작법이 아니다. 이를 그대로 일반 레시피와 동일하게 조회할 수 없으며, 승인 상태도 표현할 수 없다.

또한 현재 `recipes`는 입력 3개, 출력 2개를 고정 컬럼으로 보관한다. 생성 레시피까지 수용하면 입력·출력 개수가 변할 가능성이 있으므로 고정 슬롯 구조가 확장 한계가 된다.

## 3. 목표와 비목표

### 목표

1. 일반·생성 레시피를 동일한 저장소와 조회 계약으로 제공한다.
2. LLM 제안이 검증 없이 게임 생산 규칙에 반영되지 않게 한다.
3. 장비와 입력 조합에 대한 승인 레시피의 모호성을 방지한다.
4. 기존 `RecipeTable.csv` 적재와 기존 레시피 매칭 동작을 유지한다.
5. 입력·출력 개수 제한을 제거한다.
6. 어떤 실험에서 생성 레시피가 유래했는지 추적한다.

### 비목표

- 물질과 레시피를 하나의 테이블로 합치지 않는다.
- 실험 성공·실패 이력을 레시피 테이블에 저장하지 않는다.
- LLM 문자열 후보를 자동 승인하지 않는다.
- 이번 설계에서 전역 아이템 카탈로그 테이블을 새로 만들지 않는다.
- Unreal의 기존 RecipeTable 포맷을 즉시 제거하지 않는다.

## 4. 대안 비교

### 대안 A: 현재 분리 구조 유지

일반 레시피는 `recipes`, 생성 후보는 `generated_materials.recipe_candidates_json`에 유지한다.

- 장점: 변경량이 가장 작다.
- 단점: 생성 레시피를 실제 생산 규칙으로 승격할 표준 경로가 없고 조회 로직이 계속 분리된다.
- 판정: 생성 후보를 표시만 하고 실제 제작에는 사용하지 않을 경우에만 적합하다.

### 대안 B: 기존 `recipes` 고정 컬럼에 메타데이터만 추가

현재 입력 3개·출력 2개 구조에 `source`, `status`, `source_experiment_id`를 추가한다.

- 장점: 기존 ingestion과 매칭 코드를 적게 수정한다.
- 단점: 슬롯 수 제한과 반복 컬럼이 유지되며, 생성 레시피 확장 시 다시 스키마를 변경해야 한다.
- 판정: 단기 MVP는 가능하지만 같은 문제를 뒤로 미루는 과도기안이다.

### 대안 C: 단일 recipe catalog + 입력·출력 자식 테이블

`recipes`, `recipe_inputs`, `recipe_outputs`로 제작법을 정규화한다.

- 장점: 일반·생성 레시피가 동일한 모델을 사용하고 입력·출력 개수 제한이 사라진다.
- 장점: 승인 정책과 출처 추적을 한 곳에서 처리한다.
- 단점: 마이그레이션과 repository 변경 범위가 대안 B보다 크다.
- 판정: **권장안**. 현재 기능 규모에서 한 번 정리하는 편이 이후 생성 레시피 기능보다 단순하다.

## 5. 목표 데이터 모델

### 5.1 `recipes`

| 컬럼 | 타입 | 규칙 |
| --- | --- | --- |
| `id` | integer/bigint | PK. 기존 자동 증가 정수 PK 유지 |
| `recipe_key` | string | 외부에서 사용하는 안정적인 고유 키. `rcp_` 접두 ID (UNIQUE) |
| `recipe_name` | string | 표시 및 관리용 이름 |
| `source` | enum/string | `authored`, `generated` |
| `status` | enum/string | `draft`, `approved`, `rejected`, `disabled` |
| `machine_type` | string | 제작 장비 ID |
| `input_signature` | string(64) | 정규화된 장비+입력 조합 해시 |
| `crafting_time` | float | 0보다 커야 함 |
| `source_experiment_id` | string nullable | 생성 레시피의 원본 실험 ID |
| `created_by` | string nullable | 시스템, 운영자 또는 생성 주체 |
| `approved_by` | string nullable | 승인 주체 |
| `approved_at` | timestamp nullable | 승인 시각 |
| `created_at` | timestamp | 생성 시각 |
| `updated_at` | timestamp | 변경 시각 |

핵심 제약:

- `recipe_key`는 전체에서 유일하다.
- `source=authored`인 ingestion 레시피는 기본 `status=approved`다.
- `source=generated` 레시피는 반드시 `draft`로 시작한다.
- 동일한 `machine_type + input_signature`에는 승인된 레시피가 최대 하나만 존재한다.
- `status=approved` 전환 전에 입력, 출력, 장비, 제작시간을 결정론적으로 검증한다.

### 5.2 `recipe_inputs`

| 컬럼 | 타입 | 규칙 |
| --- | --- | --- |
| `id` | bigint/integer | PK |
| `recipe_id` | integer/bigint | `recipes.id` FK, cascade delete |
| `item_id` | string | 입력 아이템 ID |
| `quantity` | integer | 0보다 커야 함 |
| `sort_order` | integer | UI와 export의 안정적인 순서 |

제약:

- `(recipe_id, item_id)` 유일
- 중복 아이템은 저장 전에 수량을 합산

### 5.3 `recipe_outputs`

| 컬럼 | 타입 | 규칙 |
| --- | --- | --- |
| `id` | bigint/integer | PK |
| `recipe_id` | integer/bigint | `recipes.id` FK, cascade delete |
| `item_id` | string | 출력 아이템 또는 생성 물질 ID |
| `quantity` | integer | 0보다 커야 함 |
| `sort_order` | integer | UI와 export의 안정적인 순서 |

현재 별도 아이템 카탈로그가 없으므로 `item_id`에는 FK를 걸지 않는다. 일반 아이템 ID와 `generated_materials.id`는 동일한 문자열 ID 네임스페이스에서 충돌하지 않아야 한다. 전역 아이템 카탈로그가 도입되면 그때 FK를 추가한다.

### 5.4 유지되는 테이블

`generated_materials`는 물질의 속성과 비주얼 상태를 계속 소유한다. `recipe_candidates_json`은 구조화된 draft 레시피 전환 완료 후 폐기 대상이다.

`generated_experiments`는 실험 결과를 계속 소유한다. 기존 `recipe_name` 참조는 `recipe_id`로 전환하되 호환 기간에는 두 값을 함께 기록한다.

`generated_material_discoveries`는 변경하지 않는다.

## 6. 상태 전이

```text
CSV ingestion
  -> authored / approved

LLM recipe proposal
  -> schema validation
  -> deterministic balance validation
  -> generated / draft
  -> operator or policy approval
  -> generated / approved
  -> 게임 레시피 매칭 대상

draft -> rejected
approved -> disabled
```

`rejected`와 `disabled`는 의미를 구분한다.

- `rejected`: 검토 결과 게임 규칙에 편입되지 않은 제안
- `disabled`: 과거에는 유효했으나 더 이상 생산에 사용하지 않는 레시피

## 7. 처리 흐름

### 7.1 기존 CSV 레시피 적재

1. `RecipeTable.csv` 행을 읽는다.
2. 고정 슬롯 입력·출력을 정규화된 리스트로 변환한다.
3. `recipe_key`, `input_signature`를 결정론적으로 생성한다.
4. `recipes`를 `source=authored`, `status=approved`로 upsert한다.
5. `recipe_inputs`, `recipe_outputs`를 현재 CSV 값으로 교체한다.

CSV는 계속 authoring 원본이며 DB는 런타임 정본 역할을 한다.

### 7.2 기존 레시피 매칭

1. 요청 입력의 중복 아이템 수량을 합산하고 정렬한다.
2. `machine_type + normalized inputs`로 `input_signature`를 생성한다.
3. `status=approved`인 레시피만 조회한다.
4. 일치한 레시피의 출력 목록을 반환한다.

`draft`, `rejected`, `disabled` 레시피는 known item 계산과 생산 매칭에서 제외한다. 단, generated material 자체가 이미 발견된 아이템이라면 별도 물질/아이템 조회 경로에서 존재성을 확인한다.

### 7.3 신물질 레시피 생성

현재 `next_recipe_candidates: list[str]`는 실행 가능한 레시피가 아니므로 바로 `recipes`에 저장하지 않는다.

구조화된 후보는 최소한 다음 값을 가져야 한다.

```json
{
  "recipe_name": "Reinforced Alloy Plate",
  "machine_type": "Assembler",
  "inputs": [
    {"item_id": "mat_reinforced_alloy", "qty": 2},
    {"item_id": "iron_plate", "qty": 1}
  ],
  "outputs": [
    {"item_id": "reinforced_alloy_plate", "qty": 1}
  ],
  "crafting_time": 6.0
}
```

저장 순서:

1. 신물질과 실험을 기존 방식으로 먼저 확정한다.
2. 구조화된 레시피 후보의 아이템, 수량, 장비, 제작시간을 검증한다.
3. 검증된 후보만 `source=generated`, `status=draft`로 저장한다.
4. `source_experiment_id`로 원본 실험을 연결한다.
5. 승인 시에만 일반 레시피 매칭 캐시를 무효화하고 다시 적재한다.

신물질 생성 성공과 후속 레시피 후보 생성은 독립적이다. 후보 저장 실패가 신물질 생성 자체를 실패시키지 않는다.

## 8. Repository 경계

`RecipeRepository`는 저장 형식과 호출자를 분리하는 단일 인터페이스가 된다.

```text
RecipeRepository
  - find_approved_match(machine_type, normalized_inputs)
  - list_known_item_ids()
  - upsert_authored_recipe(recipe)
  - create_generated_draft(recipe, source_experiment_id)
  - approve_recipe(recipe_key: str, approved_by: str)
  - reject_recipe(recipe_key: str, rejected_by: str, reason: str)
  - disable_recipe(recipe_key: str, disabled_by: str)
```

에이전트 그래프는 SQLAlchemy 모델을 직접 조립하지 않고 service/repository를 호출한다. 승인 레시피 캐시는 repository가 관리하며 다음 사건에만 무효화한다.

- authored recipe ingestion 완료
- generated recipe 승인
- approved recipe 비활성화

## 9. API 및 응답 영향

기존 신물질 생성 응답은 유지한다. 생성된 draft 레시피가 있을 경우에만 선택 필드를 추가한다.

```json
{
  "result_type": "new_material",
  "material_id": "mat_reinforced_alloy",
  "draft_recipe_keys": ["rcp_generated_ab12cd"]
}
```

선택 필드 추가는 기존 클라이언트에 비호환 변경을 만들지 않는다.

관리용 API가 필요할 경우 다음 리소스 계약을 사용한다.

```text
GET  /api/v1/recipes?source=generated&status=draft
GET  /api/v1/recipes/{recipe_key}
POST /api/v1/recipes/{recipe_key}/approve
POST /api/v1/recipes/{recipe_key}/reject
POST /api/v1/recipes/{recipe_key}/disable
```

승인 충돌은 `409 Conflict`, 존재하지 않는 레시피는 `404 Not Found`, 유효하지 않은 상태 전이는 `422 Unprocessable Entity`로 반환한다. 초기 구현에 관리 UI가 없다면 service 계층과 테스트까지만 만들고 외부 endpoint는 후속 범위로 둘 수 있다.

## 10. 마이그레이션 전략

배포된 migration 파일은 수정하지 않고 새 migration을 추가한다.

### Phase 1: Expand

1. `recipes`에 출처·상태·해시·추적 컬럼을 nullable 또는 안전한 기본값으로 추가한다.
2. `recipe_inputs`, `recipe_outputs`를 생성한다. (기존 정수 PK인 `recipes.id`와 타입을 맞추기 위해 `recipe_id`는 `integer/bigint` 타입으로 생성한다)
3. `generated_experiments.recipe_id` (integer/bigint nullable)를 추가한다.
4. 필요한 FK와 인덱스를 추가한다.

권장 인덱스:

```text
recipes(recipe_key) UNIQUE
recipes(machine_type, input_signature) UNIQUE WHERE status = 'approved' (중복 승인 방지 partial unique index)
recipes(status, machine_type, input_signature)
recipe_inputs(recipe_id, sort_order)
recipe_inputs(item_id)
recipe_outputs(recipe_id, sort_order)
recipe_outputs(item_id)
generated_experiments(recipe_id)
```

**롤백 전략(Rollback Strategy):**
- 마이그레이션 적용 중 예기치 못한 실패가 발생할 경우를 대비하여 Alembic downgrade 스크립트를 작성한다.
- 롤백 시 생성된 FK 및 partial unique index를 먼저 Drop하고, 자식 테이블(`recipe_inputs`, `recipe_outputs`)과 추가된 컬럼들을 안전하게 삭제하도록 마이그레이션 트랜잭션 내에서 처리한다.

### Phase 2: Backfill

별도 데이터 migration으로 기존 `recipes` 행을 변환한다.

1. 기존 ID를 안정적인 `rcp_` 키와 연결한다.
2. 모든 기존 행을 `authored/approved`로 설정한다.
3. 입력·출력 고정 컬럼을 자식 테이블로 복사한다.
4. 정규화 입력으로 `input_signature`를 계산한다.
5. `generated_experiments.recipe_name`을 이용해 `recipe_id`를 연결한다.
6. 행 수, 입력·출력 수, signature 중복을 검증한다.

### Phase 3: Migrate Application

1. repository를 새 자식 테이블 조회로 전환한다.
2. ingestion을 신규 구조 dual-write로 전환한다.
3. 전체 테스트와 CSV smoke test를 통과시킨다.
4. 한 릴리스 동안 기존 고정 컬럼을 읽기 fallback으로 유지한다.

### Phase 4: Generated Drafts

1. 문자열 후보를 구조화된 후보 스키마로 변경한다.
2. 검증된 후보를 draft recipe로 저장한다.
3. 승인·거절 상태 전이와 캐시 무효화를 추가한다.

### Phase 5: Contract

새 구조가 안정화된 다음 별도 migration에서 다음 항목을 제거한다.

- `recipes.input_item_1..3`, `input_qty_1..3`
- `recipes.output_item_1..2`, `output_qty_1..2`
- `generated_experiments.recipe_name`
- 사용처가 사라진 `generated_materials.recipe_candidates_json`

## 11. 일관성 및 오류 처리

- recipe와 입력·출력 저장은 하나의 트랜잭션으로 처리한다.
- draft 생성 실패는 신물질 생성 결과를 롤백하지 않는다.
- 승인 처리 시 signature 충돌이 발생하면 기존 승인 recipe ID와 함께 `409`를 반환한다.
- ingestion 중 잘못된 수량이나 빈 장비 ID가 있으면 해당 행을 건너뛰지 않고 전체 작업을 실패시켜 원본 오류를 드러낸다.
- recipe 삭제 대신 상태 전이를 사용해 실험 이력 참조를 보존한다.
- 승인·비활성화 후 캐시 무효화 실패 시 요청을 성공 처리하지 않는다.

## 12. 테스트 전략

### 단위 테스트

- 입력 정규화와 `input_signature` 순서 독립성
- 중복 item 수량 합산
- source/status 상태 전이 허용 및 거부
- draft 레시피가 매칭되지 않음
- approved 레시피만 매칭됨
- 동일 장비·입력의 중복 승인 거부

### Repository 통합 테스트

- recipe + inputs + outputs 원자적 저장
- authored upsert 멱등성
- generated draft와 source experiment 연결
- 승인·거절·비활성화 시 캐시 무효화
- 다중 입력·출력 레시피 조회

### Migration smoke test

- 기존 migration부터 신규 head까지 SQLite upgrade
- 기존 `recipes` fixture backfill 후 데이터 동등성 검증
- downgrade 가능 범위 검증
- PostgreSQL에서는 partial unique/index 및 FK 검증

### E2E/Smoke

- 기존 `Smelt_Iron` 요청이 동일 출력을 반환
- generated draft는 생산 매칭에서 제외
- 승인 후 동일 요청이 새 recipe를 반환
- disabled 전환 후 다시 매칭에서 제외
- RecipeTable CSV 재적재 후 생성 레시피가 삭제되지 않음

## 13. 완료 기준

- 일반·생성 레시피가 같은 `RecipeRepository`를 사용한다.
- 모든 제작 가능 레시피가 `recipes` 정본에 존재한다.
- generated recipe는 `draft`로 시작하며 승인 전에는 생산되지 않는다.
- 기존 CSV 레시피의 매칭 결과가 변경되지 않는다.
- 입력 4개 이상 또는 출력 3개 이상인 레시피를 스키마 변경 없이 저장할 수 있다.
- experiment에서 원본 또는 생성 recipe를 ID로 추적할 수 있다.
- migration 전후 레시피·입력·출력 데이터 수가 검증된다.
- 전체 pytest, Ruff, migration smoke test가 통과한다.

## 14. 후속 결정

다음 항목은 구현 착수 전에 확정해야 하지만 현재 데이터 모델을 바꾸지는 않는다.

1. generated recipe 승인 주체를 운영자 수동 승인으로 할지, 결정론적 정책 자동 승인으로 할지
2. `draft_recipe_keys`를 Unreal에 즉시 노출할지 관리 API에만 노출할지
3. generated material ID를 장기적으로 전역 item catalog에 편입할지

기본 권장은 **운영자 승인**, **관리 API 우선 노출**, **item catalog는 별도 후속 설계**다.
