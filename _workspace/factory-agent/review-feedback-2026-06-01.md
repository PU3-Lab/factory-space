# Review Feedback - 2026-06-01

## Sprint 6.1 LLM decision routing edge coverage

### Review 1

- Reviewer: Faraday (`reviewer`, gpt-5.5 high)
- Scope:
  - `backend/tests/test_pipeline_edges.py`
  - `backend/docs/plans/llm_implementation_sprint.md`
  - `_workspace/factory-agent/task_6_1_routing_tests_red.md`
- Status: finding resolved by commit step

Finding:

- `backend/docs/plans/llm_implementation_sprint.md`에서 `커밋: test: LLM 기반 agent routing 경로 보강` 항목을 `[x]`로 표시했지만, 리뷰 시점에는 아직 해당 커밋이 없어 사실과 맞지 않았다.

Resolution:

- Sprint 6.1 작업 범위는 커밋까지 포함하므로, 최종 완료 전에 `test: LLM 기반 agent routing 경로 보강` 커밋을 생성해 체크 상태와 실제 git history를 일치시킨다.

Notes:

- 새 테스트들은 실제 LangGraph routing edge 계약을 통과 경로와 JSON 거부 경로 양쪽에서 검증한다.
- `len(llm.prompts)` assertion은 JSON routing decision이 leaf/generation 단계로 진행되지 않았음을 고정한다.
- `backend/tests/test_message_router.py`와 일부 중복이 있지만, Sprint 6.1이 `test_pipeline_edges.py`에 명시적으로 보강하는 계획이므로 허용 가능한 중복으로 판단한다.
