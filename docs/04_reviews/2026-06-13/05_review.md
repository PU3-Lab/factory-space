# 코드 리뷰: Material Generation Agent 4차 재검증

| 항목 | 내용 |
| --- | --- |
| 브랜치 | `feature/material-generation-sync` |
| 리뷰 일자 | 2026-06-13 |
| 기준 커밋 | `c42ba59` 이후 작업 트리 |
| 직전 리뷰 | `docs/04_reviews/2026-06-13/04_review.md` |
| 리뷰 범위 | executor 재시작 회귀, 연속 앱 lifespan, 레시피 외부 식별자 계약, 전체 회귀 검사 |

## 1. 발견 사항

**새로운 Blocker, Major, Important 발견 사항 없음.**

직전 리뷰에서 재현된 앱 재시작 후 비주얼 작업 비활성화 문제는 lifespan 시작 시 executor를 초기화하고 종료를 `finally`에서 처리하도록 수정되어 해소됐다. 외부 레시피 식별자도 `recipe_key`와 `draft_recipe_keys`로 일관되게 정리됐다.

## 2. 직전 리뷰 대응 상태

| 직전 항목 | 상태 | 확인 내용 |
| --- | --- | --- |
| 다음 앱 lifespan에서 executor 미복구 | 해결 | lifespan 시작 시 `reset_executor(wait=False)` 호출 |
| lifespan 종료 시 executor 정리 | 해결 | `finally`에서 `shutdown_executor(wait=True)` 호출 |
| 연속 앱 lifespan 회귀 테스트 부재 | 해결 | 두 번째 앱에서도 `visual_ready` 상태가 되는 테스트 추가 |
| `draft_recipe_ids` 잔여 표기 | 해결 | 응답 예시와 후속 결정 모두 `draft_recipe_keys`로 통일 |

## 3. 검증 결과

직전 실패 순서 재검증:

```text
$ cd backend && UV_CACHE_DIR=/tmp/uv-cache uv run pytest \
    tests/test_docs_router.py::test_docs_router_does_not_change_health_endpoint \
    tests/agents/material_generation/test_agent.py::test_agent_synthesize_new_material_visual_asset_true_background_processing -q

2 passed in 2.15s
```

연속 lifespan 회귀 테스트:

```text
$ cd backend && UV_CACHE_DIR=/tmp/uv-cache uv run pytest \
    tests/agents/material_generation/test_agent.py::test_consecutive_app_lifespans_processing_events -q

1 passed in 4.19s
```

전체 검증:

```text
$ cd backend && UV_CACHE_DIR=/tmp/uv-cache uv run pytest
200 passed in 10.23s

$ cd backend && UV_CACHE_DIR=/tmp/uv-cache uv run ruff check .
All checks passed!

$ cd backend && UV_CACHE_DIR=/tmp/uv-cache uv run ruff format --check .
120 files already formatted

$ git diff --check
오류 없음
```

## 4. 잔여 위험

- 비주얼 executor는 프로세스 내부 메모리 큐이므로 프로세스 강제 종료 시 대기 작업의 영속성은 보장하지 않는다. 현재 구현이 모의 비주얼 파이프라인인 범위에서는 수용 가능하며, 실제 렌더링 작업으로 전환할 때 durable queue 도입을 별도로 검토한다.

## 5. 최종 판정

**결론: 승인(approve).**

직전 리뷰의 필수 수정사항이 모두 반영됐고, 순서 의존 회귀 검증과 전체 테스트 및 정적 검사가 통과했다.
