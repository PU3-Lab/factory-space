# 코드 리뷰: Material Generation Agent 수정사항 재검증

| 항목 | 내용 |
| --- | --- |
| 브랜치 | `feature/material-generation-sync` |
| 리뷰 일자 | 2026-06-13 |
| 기준 커밋 | `c42ba59` 이후 작업 트리 |
| 직전 리뷰 | `docs/04_reviews/2026-06-13/01_review.md` |
| 리뷰 범위 | 직전 리뷰 대응 수정, 비주얼 후처리, 그래프 분리, 레시피 저장소 통합 설계 |

## 1. 발견 사항

### [Major] M1. 테스트용 인메모리 SQLite가 비주얼 작업 스레드와 공유되지 않음

- 위치: `backend/tests/agents/material_generation/conftest.py:25`, `backend/src/agents/material_generation/events.py:28-33`, `backend/src/agents/material_generation/visual_pipeline.py:38-45`
- 내용: fixture가 `sqlite:///:memory:` 엔진의 세션 팩토리를 비주얼 파이프라인에 주입하지만, 백그라운드 스레드는 별도 연결을 사용한다. SQLite 인메모리 데이터베이스는 연결별로 생성되므로 작업 스레드에서는 테스트가 만든 테이블과 데이터를 볼 수 없다.
- 재현 결과: 이벤트를 직접 발행하고 executor 완료를 기다리면 `sqlite3.OperationalError: no such table: generated_materials`가 발생하고 `visual_status`가 `pending`에 남는다.
- 테스트 공백: 현재 추가된 테스트는 `generate_visual_asset=False`만 검증한다. 커밋 이후 이벤트 발행과 최종 `visual_ready` 또는 `failed` 상태를 검증하는 테스트가 없다.
- 영향: 현재 전체 테스트는 통과하지만, 새로 도입한 커밋 이후 비동기 처리 경로가 실제로 동작하는지는 검증되지 않는다.
- 권고: 테스트 엔진에 `StaticPool`과 `check_same_thread=False`를 적용하고, 세션 커밋 후 executor 완료와 DB 상태 변경을 확인하는 회귀 테스트를 추가한다.

### [Major] M2. 레시피 통합 설계의 PK 타입 전환 절차가 누락됨

- 위치: `backend/src/db/models.py:29`, `docs/02_work_plans/recipe_storage_unification_design.md:93`, `docs/02_work_plans/recipe_storage_unification_design.md:277-305`
- 내용: 현재 `recipes.id`는 자동 증가 정수 PK지만 목표 모델은 `rcp_` 접두 문자열 PK이며, `recipe_inputs.recipe_id`, `recipe_outputs.recipe_id`, `generated_experiments.recipe_id`도 문자열 FK로 정의한다. 그러나 Phase 1은 기존 PK 타입을 바꾸지 않은 채 자식 테이블과 FK를 생성하도록 기술되어 있다.
- 영향: PostgreSQL에서는 정수 PK와 문자열 FK의 타입 불일치로 FK 생성이 실패한다. SQLite에서도 PK 교체는 테이블 재생성이 필요하므로 현재 단계만으로는 구현할 수 없다.
- 권고: 기존 정수 PK를 유지하고 별도 `recipe_key`를 외부 ID로 사용할지, 신규 문자열 PK 테이블을 생성해 복사·검증·교체할지 명시한다. 선택한 방식에 맞춰 FK 생성 순서와 rollback 전략도 추가한다.

### [Major] M3. 승인 레시피 중복을 DB 제약으로 방지하지 못함

- 위치: `docs/02_work_plans/recipe_storage_unification_design.md:108-114`, `docs/02_work_plans/recipe_storage_unification_design.md:284-294`
- 내용: 설계는 동일한 `machine_type + input_signature`에 승인 레시피가 최대 하나라고 정의하지만, 권장 인덱스는 `recipes(status, machine_type, input_signature)` 일반 인덱스뿐이다.
- 영향: 서비스 계층에서 충돌을 먼저 조회해도 동시 승인 트랜잭션 두 개가 모두 성공할 수 있어 핵심 불변식이 깨진다.
- 권고: PostgreSQL에서는 `status = 'approved'` 조건의 partial unique index를 추가한다. SQLite 테스트와 다른 DB 지원 정책도 함께 정의하고, unique 위반을 `409 Conflict`로 변환한다.

### [Important] I1. 분리된 `nodes.py`가 프로젝트의 500줄 제한을 초과함

- 위치: `backend/src/agents/material_generation/nodes.py:1-536`, `backend/AGENTS.md:193`
- 내용: 기존 `graph.py`의 노드를 별도 파일로 분리했지만 새 파일이 536줄이어서 일반 source 파일 500줄 제한을 여전히 만족하지 못한다.
- 영향: 직전 리뷰의 구조 규칙 위반이 파일 이동 형태로 남아 있다.
- 권고: 등록·중복 제거 노드 또는 라우팅 함수처럼 책임이 분명한 단위로 한 번 더 분리한다.

### [Important] I2. 저장소 루트의 `AGENTS.md`가 삭제됨

- 위치: `AGENTS.md` 전체 삭제
- 내용: 코딩 전 가정 명시, 외과적 변경, 검증 기준, 한국어 PR 작성 규칙을 담은 저장소 공통 지침이 대체 파일 없이 삭제되었다.
- 영향: `backend/AGENTS.md`가 적용되지 않는 프론트엔드와 루트 작업에서 팀 공통 규칙을 자동화 도구가 더 이상 발견하지 못한다.
- 권고: 의도된 정책 변경이 아니라면 삭제를 되돌린다. 지침 이전이 목적이라면 저장소에서 접근 가능한 대체 경로와 적용 범위를 함께 반영한다.

## 2. 직전 리뷰 대응 상태

| 직전 항목 | 상태 | 확인 내용 |
| --- | --- | --- |
| Alembic 실행 후 로거 비활성화 | 해결 | `disable_existing_loggers=False` 적용 후 전체 테스트 통과 |
| `generate_visual_asset=False` 무시 | 해결 | 신규 물질 상태 `skipped`, 이벤트 미발행, 응답 메시지 테스트 추가 |
| 커밋 전 비주얼 작업 시작 | 부분 해결 | `after_commit` 등록으로 실행 시점은 이동했으나 실제 비동기 경로 테스트는 실패하는 fixture 구조 |
| Ruff 및 내부 import 오류 | 해결 | Ruff 검사 통과, 내부 import 상단 이동 |
| 500줄 초과 | 미해결 | `graph.py`는 축소됐지만 `nodes.py`가 536줄 |

## 3. 검증 결과

```text
$ cd backend && UV_CACHE_DIR=/tmp/uv-cache uv run pytest
197 passed in 2.10s

$ cd backend && UV_CACHE_DIR=/tmp/uv-cache uv run ruff check .
All checks passed!

$ cd backend && UV_CACHE_DIR=/tmp/uv-cache uv run ruff format --check .
119 files already formatted
```

비동기 시각화 경로는 별도 재현에서 다음 오류가 확인됐다.

```text
sqlite3.OperationalError: no such table: generated_materials
visual_status=pending
```

## 4. 최종 판정

**결론: 변경 요청(changes requested).**

머지 전 최소 조건:

1. 테스트용 DB를 작업 스레드와 공유하고 커밋 이후 비주얼 상태 변경을 검증할 것.
2. 레시피 통합 설계에서 기존 정수 PK와 목표 문자열 PK 사이의 마이그레이션 절차를 확정할 것.
3. 승인 레시피 유일성을 DB 제약으로 보장할 것.
4. `nodes.py`를 500줄 이하로 분리할 것.
5. 루트 `AGENTS.md` 삭제 의도를 확인하고 공통 지침 유실을 해소할 것.
