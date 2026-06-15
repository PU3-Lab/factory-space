# 코드 리뷰: Material Generation Agent 3차 재검증

| 항목 | 내용 |
| --- | --- |
| 브랜치 | `feature/material-generation-sync` |
| 리뷰 일자 | 2026-06-13 |
| 기준 커밋 | `c42ba59` 이후 작업 트리 |
| 직전 리뷰 | `docs/04_reviews/2026-06-13/03_review.md` |
| 리뷰 범위 | executor 종료·재시작 생명주기, 레시피 외부 식별자 계약, 전체 회귀 검사 |

## 1. 발견 사항

### [Major] M1. 첫 앱 종료 후 다음 앱 수명주기에서 비주얼 작업이 영구 비활성화됨

- 위치: `backend/src/app.py:18-25`, `backend/src/agents/material_generation/events.py:30-36`, `backend/src/agents/material_generation/events.py:53-78`
- 내용: FastAPI lifespan 종료 시 `shutdown_executor()`가 전역 `_shutdown=True`, `_executor=None`으로 만든다. 그러나 다음 lifespan 시작에서는 executor를 다시 초기화하지 않는다. 이후 신물질 커밋의 `after_commit` 콜백이 실행돼도 publisher가 경고만 남기고 작업을 버린다.
- 재현:

```text
$ cd backend && UV_CACHE_DIR=/tmp/uv-cache uv run pytest \
    tests/test_docs_router.py::test_docs_router_does_not_change_health_endpoint \
    tests/agents/material_generation/test_agent.py::test_agent_synthesize_new_material_visual_asset_true_background_processing -q

1 failed, 1 passed
WARNING MaterialEventPublisher: Attempted to publish material created event after shutdown
AssertionError: assert 'pending' == 'visual_ready'
```

- 영향: 같은 프로세스에서 앱 lifespan이 재시작되거나 테스트 앱을 순차 생성하면 비주얼 상태가 `pending`에 영구적으로 남는다. 기본 전체 테스트 순서에서는 신물질 테스트가 앱 종료 테스트보다 먼저 실행되어 결함이 가려진다.
- 테스트 공백: `test_visual_executor_permanent_shutdown_and_block`는 테스트 끝에서 직접 reset하며, 실제 앱 종료 후 두 번째 앱 시작 시 작업이 재개되는지는 검증하지 않는다.
- 권고: executor를 lifespan 시작에서 명시적으로 초기화하고 종료는 `try/finally`에서 보장한다. 모듈 import 시 생성하는 대신 앱 시작·종료가 executor를 소유하도록 만들고, 두 번의 연속 lifespan 이후 두 번째 앱에서도 이벤트가 처리되는 회귀 테스트를 추가한다.

### [Important] I1. 설계 문서 후속 결정에 폐기된 응답 필드명이 남아 있음

- 위치: `docs/02_work_plans/recipe_storage_unification_design.md:255`, `docs/02_work_plans/recipe_storage_unification_design.md:393`
- 내용: 공식 응답 예시는 외부 식별자를 `draft_recipe_keys`로 정리했지만 후속 결정 항목은 여전히 `draft_recipe_ids`를 사용한다.
- 영향: 구현 범위를 결정할 때 두 이름 중 어떤 필드를 노출해야 하는지 다시 모호해진다.
- 권고: 후속 결정 항목도 `draft_recipe_keys`로 통일한다.

## 2. 직전 리뷰 대응 상태

| 직전 항목 | 상태 | 확인 내용 |
| --- | --- | --- |
| 외부 `recipe_key`와 내부 `recipe_id` 혼재 | 대부분 해결 | repository와 API 경로는 `recipe_key`, 응답은 `draft_recipe_keys`로 정리됐으나 후속 결정 문구 1곳 미수정 |
| executor 운영 종료 경계 | 부분 해결 | 영구 shutdown과 lifespan 종료 호출은 추가됐으나 다음 앱 시작 시 재초기화되지 않음 |

## 3. 검증 결과

```text
$ cd backend && UV_CACHE_DIR=/tmp/uv-cache uv run pytest
199 passed in 6.07s

$ cd backend && UV_CACHE_DIR=/tmp/uv-cache uv run ruff check .
All checks passed!

$ cd backend && UV_CACHE_DIR=/tmp/uv-cache uv run ruff format --check .
120 files already formatted

$ git diff --check
오류 없음
```

전체 테스트는 현재 파일 정렬 순서에서 통과하지만, 앱 종료 테스트를 먼저 실행하는 순서 의존 검증에서는 비주얼 처리 테스트가 실패한다.

## 4. 최종 판정

**결론: 변경 요청(changes requested).**

머지 전 최소 조건:

1. 앱 lifespan 시작 시 executor가 사용 가능한 상태임을 보장할 것.
2. 연속된 두 앱 lifespan에서 두 번째 앱의 비주얼 이벤트 처리를 검증할 것.
3. 설계 문서의 `draft_recipe_ids` 잔여 표기를 `draft_recipe_keys`로 통일할 것.
