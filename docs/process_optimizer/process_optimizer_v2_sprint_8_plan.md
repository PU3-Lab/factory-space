# Process Optimizer v2 Sprint 8 계획

## 목표

v2 전체 흐름을 WebSocket smoke test와 문서까지 맞춰 최종 검증한다.

이 sprint는 기능 추가보다 통합 안정화와 시연 준비가 핵심이다.

## 구현 범위

```text
- analyze -> preview smoke test
- apply 승인 차단/성공 smoke test
- revision conflict smoke test
- undo conflict smoke test
- measure smoke test
- agent test sample 최신화
- Unreal WebSocket 계약 문서 최신화
- demo guide 최신화
- sprint review 문서 작성
```

## 수정 파일

```text
backend/scripts/smoke_agent_pipeline.py
backend/tests/test_process_optimizer_smoke.py
docs/process_optimizer/agent_test_sample.json
docs/process_optimizer/unreal_websocket_contract.md
docs/process_optimizer/process_optimizer_demo_guide.md
docs/04_reviews/process_optimizer/sprint_v2_8_review.md
```

## Smoke 시나리오

```text
1. analyze 요청 -> preview 반환
2. approval 없는 apply -> approval_required
3. 정상 apply -> command payload 반환
4. factoryRevision 변경 -> revision_conflict
5. undo 충돌 -> undo_conflict
6. measure 준비 전 -> measurement_not_ready
7. measure 완료 -> measurement summary 반환
```

## 성공 기준

```text
- 모든 v2 단위 테스트가 통과한다.
- smoke_agent_pipeline local에서 process_optimizer v2 시나리오가 통과한다.
- 문서의 JSON 예시와 실제 응답 schema가 일치한다.
- 기존 v1 설명과 v2 최종 구조가 문서에서 구분된다.
```

## 테스트 계획

```text
uv run pytest backend/tests/test_process_optimizer*.py -q
uv run --env-file .env.prod python backend/scripts/smoke_agent_pipeline.py local
```

## 문서 업데이트 기준

```text
- process_optimizer_demo_guide.md에 v2 시연 순서 추가
- unreal_websocket_contract.md에 preview/apply/undo/measure 예시 반영
- agent_test_sample.json에 v2 요청 샘플 추가
- sprint review 문서에 구현 결과와 미구현/확장 범위 기록
```

## 완료 후 확인 질문

```text
- 실제 agent-test 또는 smoke script로 end-to-end 검증했는가?
- Unreal 담당자가 사용할 JSON 계약이 최신인가?
- 발표에서 v1 MVP와 v2 최종 구조를 구분해 설명할 수 있는가?
```

