# Process Optimizer v2 Sprint 4 계획

## 목표

플레이어 승인 없이 최적화 실행 단계로 넘어가지 못하게 `apply` 승인 흐름을 만든다.

이 sprint에서도 Unreal command를 실제로 생성하지 않고, 승인과 선택 항목 검증까지만 완료한다.

## 구현 범위

```text
- apply 요청 schema 정의
- approval 값 검증
- plan_id 조회
- plan 만료 검증 연결
- factoryRevision 충돌 검증 연결
- approved_change_ids 검증
- 승인된 change 목록 구성
```

## 수정 파일

```text
backend/src/agents/process_optimizer/schemas.py
backend/src/agents/process_optimizer/nodes.py
backend/src/agents/process_optimizer/graph.py
backend/tests/test_process_optimizer_apply.py
```

## Apply Graph 흐름

```text
START
-> validate_apply_request
-> load_preview_plan
-> verify_plan_not_expired
-> verify_factory_revision
-> validate_approval
-> validate_selected_changes
-> return_apply_ready
-> END
```

## 성공 기준

```text
- approval이 없거나 false이면 approval_required를 반환한다.
- 없는 plan_id는 plan_not_found를 반환한다.
- 만료된 plan은 plan_expired를 반환한다.
- factoryRevision이 다르면 revision_conflict를 반환한다.
- 존재하지 않는 change_id는 invalid_change_id를 반환한다.
- 승인된 change만 다음 단계로 넘긴다.
```

## 테스트 계획

```text
- approval 없음
- approval false
- 없는 plan_id
- 만료된 plan
- revision conflict
- 잘못된 change_id
- 일부 change만 승인
```

## 완료 후 확인 질문

```text
- 승인 없는 실행이 완전히 차단되는가?
- 승인된 변경 항목만 분리되는가?
- Unreal command 생성 전 단계까지 안전하게 멈추는가?
```

