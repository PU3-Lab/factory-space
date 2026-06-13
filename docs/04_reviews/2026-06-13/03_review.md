# 코드 리뷰: Material Generation Agent 2차 재검증

| 항목 | 내용 |
| --- | --- |
| 브랜치 | `feature/material-generation-sync` |
| 리뷰 일자 | 2026-06-13 |
| 기준 커밋 | `c42ba59` 이후 작업 트리 |
| 직전 리뷰 | `docs/04_reviews/2026-06-13/02_review.md` |
| 리뷰 범위 | 직전 리뷰 대응 수정, 비주얼 후처리 생명주기, 레시피 저장소 통합 설계 |

## 1. 발견 사항

### [Important] I1. 외부 `recipe_key`와 내부 `recipe_id`의 API 계약이 혼재함

- 위치: `docs/02_work_plans/recipe_storage_unification_design.md:93-94`, `docs/02_work_plans/recipe_storage_unification_design.md:230-239`, `docs/02_work_plans/recipe_storage_unification_design.md:247-269`
- 내용: 수정된 설계는 `recipes.id`를 내부 정수 PK로 유지하고 `recipe_key`를 `rcp_` 형식의 안정적인 외부 키로 정의한다. 그러나 응답 필드는 `draft_recipe_ids`이면서 값은 `rcp_generated_ab12cd`이고, repository 메서드와 관리 API 경로도 `{recipe_id}`가 정수 PK인지 `recipe_key`인지 정의하지 않는다.
- 영향: 구현자가 내부 정수 ID를 외부에 노출하거나, 반대로 문자열 키를 정수 PK 컬럼으로 조회하는 서로 다른 계약을 만들 수 있다. 클라이언트가 저장해야 할 식별자도 불명확하다.
- 권고: 외부 계약은 `recipe_key`로 통일해 `draft_recipe_keys`, `{recipe_key}`로 명명하거나, 외부에도 정수 ID를 사용할 경우 응답 예시와 `recipe_key`의 역할을 다시 정의한다. repository 메서드도 입력 식별자 타입을 명시한다.

### [Important] I2. 비주얼 executor의 애플리케이션 종료 경계가 없음

- 위치: `backend/src/agents/material_generation/events.py:36-48`, `backend/src/app.py:17-23`
- 내용: `wait_for_jobs()`는 기존 executor를 종료한 즉시 새 executor를 생성하므로 테스트 격리에는 사용할 수 있지만 최종 종료 API로는 사용할 수 없다. FastAPI lifespan 종료에서도 executor를 종료하지 않는다.
- 영향: 서버 종료나 reload 시 이미 제출된 작업이 애플리케이션 수명 밖에서 계속 실행되며, 대기 작업이 많으면 프로세스 종료가 지연될 수 있다. 테스트에서 확보한 작업 생명주기 제어가 운영 경로에는 적용되지 않는다.
- 권고: 작업 대기·테스트용 reset과 영구 shutdown을 분리하고, FastAPI lifespan 종료에서 영구 shutdown을 호출한다. 종료 이후 제출 정책도 함께 정의한다.

## 2. 직전 리뷰 대응 상태

| 직전 항목 | 상태 | 확인 내용 |
| --- | --- | --- |
| 인메모리 SQLite 스레드 공유 | 해결 | `StaticPool`, `check_same_thread=False` 적용 |
| 커밋 이후 비주얼 처리 검증 | 해결 | 커밋 후 executor 완료 및 `visual_ready` 상태를 검증하는 테스트 추가 |
| 레시피 PK 타입 전환 누락 | 해결 | 기존 정수 PK 유지, 자식 FK 타입과 rollback 전략 명시 |
| 승인 레시피 DB 유일성 | 해결 | 승인 상태 partial unique index 설계 추가 |
| `nodes.py` 500줄 초과 | 해결 | routing 분리 후 `nodes.py` 495줄 |
| 루트 `AGENTS.md` 삭제 | 해결 | 파일 복원 확인 |

## 3. 검증 결과

```text
$ cd backend && UV_CACHE_DIR=/tmp/uv-cache uv run pytest
198 passed in 4.16s

$ cd backend && UV_CACHE_DIR=/tmp/uv-cache uv run ruff check .
All checks passed!

$ cd backend && UV_CACHE_DIR=/tmp/uv-cache uv run ruff format --check .
120 files already formatted

$ git diff --check
오류 없음
```

## 4. 최종 판정

**결론: 조건부 승인(approve with follow-up).**

현재 코드 변경의 기능 회귀와 정적 검사 문제는 확인되지 않았다. 구현 착수 전 설계 문서의 외부 식별자 계약을 확정하고, 운영 환경에서 비주얼 executor를 종료할 수 있는 생명주기 경계를 추가해야 한다.
