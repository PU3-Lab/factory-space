# Process Optimizer v2 Sprint 3 계획

## 목표

생성된 preview plan을 저장하고, 이후 apply 요청에서 유효성을 검증할 수 있게 한다.

핵심은 `plan_id`, `expires_at`, `factoryRevision`이다.

## 구현 범위

```text
- preview plan store 추가
- plan_id 생성
- expires_at 생성
- factoryRevision 저장
- plan 조회
- plan 만료 검증
- revision conflict 검증
```

## 추가 또는 수정 파일

```text
backend/src/agents/process_optimizer/preview_store.py
backend/src/agents/process_optimizer/schemas.py
backend/src/agents/process_optimizer/nodes.py
backend/tests/test_process_optimizer_preview_store.py
backend/tests/test_process_optimizer_graph.py
```

## Graph 흐름 변경

```text
validate_preview_candidates
-> create_preview_plan
-> save_preview_plan
-> return_preview_plan
```

## Preview 저장 데이터

```text
plan_id
session_id
factoryRevision
goal
changes
expected_effect
ui_hints
created_at
expires_at
```

## 성공 기준

```text
- preview 생성 시 plan_id가 반드시 포함된다.
- 같은 session_id에서 plan_id로 preview를 조회할 수 있다.
- 만료된 plan은 apply 단계로 넘어가지 않는다.
- 요청 factoryRevision과 preview factoryRevision이 다르면 revision_conflict를 반환한다.
```

## 테스트 계획

```text
- plan 저장/조회 성공
- 알 수 없는 plan_id 조회 실패
- expires_at 지난 plan은 plan_expired
- factoryRevision 불일치는 revision_conflict
```

## 완료 후 확인 질문

```text
- plan_id가 멱등 처리의 기준으로 쓸 수 있는가?
- preview 유효 기간이 테스트 가능한가?
- 다음 sprint에서 apply 요청 검증에 바로 사용할 수 있는가?
```

