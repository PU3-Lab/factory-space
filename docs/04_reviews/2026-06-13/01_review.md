# 코드 리뷰: Material Generation Agent 전체 검증

| 항목 | 내용 |
| --- | --- |
| 브랜치 | `feature/material-generation-sync` |
| 리뷰 일자 | 2026-06-13 |
| 기준 커밋 | `c42ba59` |
| 직전 리뷰 | `docs/04_reviews/2026-06-12_reviews_04.md` |
| 리뷰 범위 | 전체 백엔드 테스트, 린트, 신물질 생성 및 비주얼 후처리 흐름 |

## 1. 발견 사항

### [Blocker] B1. Alembic 스모크 테스트가 전역 로깅 상태를 오염해 전체 테스트를 실패시킴

- 위치: `backend/migrations/env.py:16-17`, `backend/tests/test_migrations_smoke.py:35`
- 재현: `cd backend && uv run pytest`
- 결과: `193 passed, 3 failed`
- 내용: 마이그레이션 테스트가 같은 pytest 프로세스에서 `command.upgrade()`를 실행하면 `env.py`의 `fileConfig(config.config_file_name)`가 호출된다. `fileConfig()`는 기본적으로 기존 로거를 비활성화하므로, 이후 실행되는 `agents.pipeline.tool_node`, `agents.pipeline.runtime`, `uvicorn.error` 로그 검증 테스트가 로그를 받지 못한다.
- 근거: 실패한 3개 테스트는 각각 단독 실행하면 모두 통과하지만, `tests/test_migrations_smoke.py` 뒤에 실행하면 재현된다.
- 영향: 전체 테스트 품질 게이트가 순서 의존적으로 실패하며, 같은 프로세스에서 Alembic을 호출하는 애플리케이션 코드도 기존 로거를 잃을 수 있다.
- 권고: Alembic 로깅 설정에서 기존 로거를 비활성화하지 않도록 하고, 마이그레이션 실행 전후 로거 상태가 유지되는 회귀 테스트를 추가한다.

### [Major] M1. `generate_visual_asset=False` 요청이 무시됨

- 위치: `backend/src/agents/material_generation/schemas.py:32`, `backend/src/agents/material_generation/graph.py:422-436`, `backend/src/agents/material_generation/graph.py:508-516`
- 내용: 요청 스키마는 `generate_visual_asset` 플래그를 제공하지만 그래프는 해당 값을 확인하지 않는다. 신규 물질은 항상 `visual_status="pending"`과 "생성 중" 메시지를 받고, 항상 `MaterialEventPublisher.publish_material_created()`가 호출된다.
- 영향: 호출자가 비주얼 생성을 명시적으로 비활성화해도 백그라운드 작업과 DB 갱신이 수행되어 API 계약이 깨진다.
- 테스트 공백: `generate_visual_asset=False` 입력에 대한 테스트가 없다.
- 권고: 이벤트 발행과 초기 상태를 플래그에 따라 분기하고, 비활성화 시 기대 상태와 메시지를 계약으로 고정하는 테스트를 추가한다.

### [Major] M2. 비주얼 작업이 트랜잭션 커밋 전에 시작되며 요청 DB 세션과 다른 DB를 사용함

- 위치: `backend/src/agents/material_generation/router.py:35-37`, `backend/src/agents/material_generation/graph.py:485-516`, `backend/src/agents/material_generation/events.py:27-33`, `backend/src/agents/material_generation/visual_pipeline.py:31-40`
- 내용: 물질 행은 요청 세션에서 `flush()`만 된 상태로 비주얼 작업 큐에 들어간다. 요청 세션의 커밋은 `agent.synthesize()`가 반환된 뒤 컨텍스트 매니저 종료 시 수행되지만, 작업 스레드는 즉시 별도 전역 `get_db_session()`을 연다. 현재의 `time.sleep(2.0)`은 커밋 순서를 보장하지 않는 시간 기반 우회다.
- 관찰 결과: 신물질 테스트 묶음 실행 후 백그라운드 스레드가 테스트용 인메모리 DB가 아닌 기본 DB를 조회하면서 `sqlite3.OperationalError: no such table: generated_materials`를 출력했다.
- 영향: 커밋이 늦거나 다른 세션/DB를 주입한 호출 경로에서는 물질을 찾지 못하고 `visual_status="pending"`에 남을 수 있다. 테스트 종료 후에도 작업이 남아 로그와 DB 상태를 오염시킨다.
- 권고: 커밋 성공 이후 작업을 등록하도록 경계를 이동하고, 작업 큐가 사용할 세션 팩토리를 명시적으로 주입한다. 테스트에서는 executor 완료와 종료를 제어할 수 있어야 한다.

### [Important] I1. 현재 브랜치가 린트 및 프로젝트 구조 규칙을 통과하지 못함

- 린트: `cd backend && uv run ruff check .` 실행 시 `scripts/ingest_recipes.py:17-18`에서 `E402` 2건 발생.
- import 규칙: `backend/AGENTS.md`는 함수/메서드 내부 import를 금지하지만 `runtime.py`, `graph.py`, `events.py`, registry 및 similarity 모듈에 내부 import가 존재한다.
- 파일 크기: `backend/src/agents/material_generation/graph.py`는 645줄로 일반 source 파일 500줄 제한을 초과한다.
- 영향: 저장소에서 명시한 정적 품질 게이트와 구조 규칙을 만족하지 못한다.
- 권고: ingestion 스크립트의 실행/import 경계를 정리하고, 내부 import를 상단으로 이동한다. `graph.py`는 노드 책임을 기준으로 분리한다.

## 2. 검증 결과

```text
$ cd backend && uv run pytest tests/agents/material_generation tests/test_migrations_smoke.py -q
24 passed in 0.18s

$ cd backend && uv run pytest
193 passed, 3 failed in 5.10s

$ cd backend && uv run ruff check .
Found 2 errors (E402)
```

기능 전용 24개 테스트는 통과하지만, 전체 회귀 테스트와 린트가 실패하므로 부분 테스트 통과만으로 머지 가능 상태를 판단할 수 없다.

## 3. 직전 리뷰 결론 갱신

2026-06-12 4차 리뷰는 신물질 전용 테스트 24개를 근거로 `approve`를 판정했다. 이번 전체 테스트 실행에서 B1의 테스트 격리 문제와 M1/M2의 비주얼 처리 계약 및 트랜잭션 경계 문제가 확인되었으므로 해당 결론을 갱신한다.

## 4. 최종 판정

**결론: 변경 요청(changes requested).**

머지 전 최소 조건:

1. Alembic 실행 후에도 기존 로거가 유지되고 전체 196개 테스트가 통과할 것.
2. `generate_visual_asset=False` 계약을 구현하고 회귀 테스트를 추가할 것.
3. 비주얼 작업을 커밋 이후에 실행하고 테스트에서 작업 생명주기를 제어할 것.
4. Ruff 오류를 제거하고 프로젝트의 import 및 500줄 제한을 충족할 것.
